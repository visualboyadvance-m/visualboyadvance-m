// 使用 AsmResolver 读取 ps12exe 生成的 exe 中内嵌的脚本资源，并返回原始 PowerShell 脚本文本。通过 exe21sp PowerShell 辅助程序对外暴露。
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text;
using AsmResolver;
using AsmResolver.DotNet;
using AsmResolver.PE;
using AsmResolver.PE.File;
using AsmResolver.PE.Win32Resources;

namespace exe21sp {
	/// <summary>
	/// 当前宿主没有 BrotliStream（.NET Framework 不提供），需要转交 pwsh / .NET Core 解压。
	/// </summary>
	public sealed class BrotliUnavailableException : Exception { }

	public static class Extractor {
		/// <summary>
		/// 从 ps12exe 生成的 exe 中提取内嵌的 PowerShell 脚本。
		/// </summary>
		/// <param name="exePath">.exe 文件的完整路径。</param>
		/// <returns>
		/// 普通 ps12exe exe：来自内嵌资源的原始 PowerShell 脚本。TinySharp 编译的 exe：合成脚本，它打印捕获的输出字符串，并在适用时追加带有所记录退出代码的 exit 语句。若该 exe 不是 ps12exe 输出或负载无法恢复，则返回 null。
		/// </returns>
		public static string ExtractScriptFromExe(string exePath) {
			if (string.IsNullOrEmpty(exePath) || !File.Exists(exePath))
				return null;
			// 首先尝试标准程序框架：内嵌的 main.ps1 资源。
			var script = TryExtractFromFrame(exePath);
			if (script != null)
				return script;

			// 回退：TinySharp 编译的最小 exe（无脚本资源）。
			return TryExtractFromTinySharp(exePath);
		}

		/// <summary>
		/// 判断 ps12exe 产物是否为 windowed（无控制台）构建，供反编译时补回 <c>#_pragma App.Windowed</c>。
		/// 标准产物（CodeDom/Core/pack）直接看最外层 PE 子系统；TinySharp 常量 GUI 产物仍标为控制台子系统，
		/// 但会 P/Invoke user32!MessageBoxW，额外识别这种情况。无法判断时返回 false。
		/// </summary>
		/// <param name="exePath">.exe 文件的完整路径。</param>
		public static bool IsWindowedExe(string exePath) {
			if (string.IsNullOrEmpty(exePath) || !File.Exists(exePath))
				return false;

			try {
				var peFile = PEFile.FromFile(exePath);
				if (peFile.OptionalHeader != null && peFile.OptionalHeader.SubSystem == SubSystem.WindowsGui)
					return true;
			}
			catch {
				return false;
			}

			// 控制台子系统：带内嵌 .ps1 资源的是标准控制台产物（脚本自身可能 P/Invoke MessageBoxW，不能据此误判）；否则按 TinySharp 常量产物处理。
			try {
				if (HasEmbeddedScriptResource(exePath))
					return false;
				return TinySharpUsesMessageBox(exePath);
			}
			catch {
				return false;
			}
		}

		// 标准程序框架（default.cs / pack.cs）会内嵌脚本：未压缩是 main.ps1，压缩后是 launcher 的 "main" 负载。据此把标准产物与 TinySharp 常量产物区分开。
		private static bool HasEmbeddedScriptResource(string exePath) {
			var module = ModuleDefinition.FromFile(exePath);
			foreach (var resource in module.Resources) {
				if (!resource.IsEmbedded)
					continue;
				string name = object.ReferenceEquals(resource.Name, null) ? null : resource.Name.ToString();
				if (name != null && (name.EndsWith(".ps1", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "main", StringComparison.OrdinalIgnoreCase)))
					return true;
			}
			return false;
		}

		/// <summary>
		/// TinySharp 常量 GUI 产物把 user32!MessageBoxW 声明为元数据方法 #1，控制台常量产物则用 puts/WriteConsoleW。方法表没有 TypeDef，AsmResolver 无法枚举 P/Invoke，故直接在 .text 的 CIL 区扫描 call Method#1 的指令字节。
		/// </summary>
		private static bool TinySharpUsesMessageBox(string exePath) {
			var peFile = PEFile.FromFile(exePath);
			if (peFile.OptionalHeader == null)
				return false;
			var clrDir = peFile.OptionalHeader.GetDataDirectory(DataDirectoryIndex.ClrDirectory);
			if (clrDir.Size == 0 || !clrDir.IsPresentInPE)
				return false;

			PESection section = null;
			foreach (var s in peFile.Sections) {
				if (!object.ReferenceEquals(s.Name, null) && s.Name.ToString() == ".text") {
					section = s;
					break;
				}
			}
			if (section == null)
				return false;

			var size = (uint)Math.Min(section.GetPhysicalSize(), 2048);
			if (size < 5)
				return false;
			var reader = peFile.CreateReaderAtFileOffset(section.Offset, size);
			var raw = reader.ReadBytes((int)reader.Length);
			if (raw == null || raw.Length < 5)
				return false;
			// call (0x28) + Method 元数据 token #1（0x06000001，小端 01 00 00 06）。
			for (int i = 0; i + 4 < raw.Length; i++) {
				if (raw[i] == 0x28 && raw[i + 1] == 0x01 && raw[i + 2] == 0x00 && raw[i + 3] == 0x00 && raw[i + 4] == 0x06)
					return true;
			}
			return false;
		}

		private static string TryExtractFromFrame(string exePath) {
			// 普通托管 exe 的镜像在偏移 0；Core 的单文件 exe 是原生 apphost 后追加托管负载，因此扫描文件内所有内嵌 PE 镜像，逐个尝试提取。
			foreach (var image in EnumerateEmbeddedImages(File.ReadAllBytes(exePath))) {
				try {
					var module = ModuleDefinition.FromBytes(image);
					var script = TryExtractFromModule(module);
					if (script != null)
						return script;

					// 非 const exe 把真正的程序集包在 launcher 的 "main" 资源里。拆开它，并在该负载中寻找 main.ps1 脚本资源。
					var payload = TryGetLauncherPayload(module);
					if (payload != null)
						return TryExtractFromModule(ModuleDefinition.FromBytes(payload));
				}
				catch (BrotliUnavailableException) {
					// 需要 .NET Core 才能解压的 Core 负载，交给上层转交 pwsh。
					throw;
				}
				catch {
					// 非有效 .NET 模块（如原生 apphost）或读取错误。
				}
			}
			return null;
		}

		/// <summary>
		/// 逐个产出文件内疑似 PE 镜像的字节切片（从每个 "MZ" 且带有效 PE 头的偏移到文件末尾）。单文件发布的 exe 把托管程序集追加在原生 apphost 之后，需要这样找出来。
		/// </summary>
		private static IEnumerable<byte[]> EnumerateEmbeddedImages(byte[] fileBytes) {
			for (int offset = 0; offset + 0x40 <= fileBytes.Length; offset++) {
				if (fileBytes[offset] != 'M' || fileBytes[offset + 1] != 'Z')
					continue;
				uint peHeaderOffset = BitConverter.ToUInt32(fileBytes, offset + 0x3C);
				if (peHeaderOffset < 0x40 || peHeaderOffset > 0x1000)
					continue;
				long peOffset = offset + (long)peHeaderOffset;
				if (peOffset + 4 > fileBytes.Length)
					continue;
				if (fileBytes[peOffset] != 'P' || fileBytes[peOffset + 1] != 'E' || fileBytes[peOffset + 2] != 0 || fileBytes[peOffset + 3] != 0)
					continue;
				var image = new byte[fileBytes.Length - offset];
				Buffer.BlockCopy(fileBytes, offset, image, 0, image.Length);
				yield return image;
			}
		}

		private static string TryExtractFromModule(ModuleDefinition module) {
			foreach (var resource in module.Resources) {
				if (!resource.IsEmbedded)
					continue;

				string name = object.ReferenceEquals(resource.Name, null) ? null : resource.Name.ToString();
				// 脚本以未压缩的 .ps1 资源内嵌（标准 frame 是 main.ps1）。
				if (name != null && name.EndsWith(".ps1", StringComparison.OrdinalIgnoreCase)) {
					var raw = resource.GetData();
					if (raw == null || raw.Length == 0)
						continue;

					using (var ms = new MemoryStream(raw))
					// 存在 BOM 时据其检测编码；默认使用不带 BOM 的 UTF-8。
					using (var reader = new StreamReader(ms, Encoding.UTF8, detectEncodingFromByteOrderMarks: true)) {
						return reader.ReadToEnd();
					}
				}
			}
			return null;
		}

		private static byte[] TryGetLauncherPayload(ModuleDefinition module) {
			foreach (var resource in module.Resources) {
				if (!resource.IsEmbedded)
					continue;
				string name = object.ReferenceEquals(resource.Name, null) ? null : resource.Name.ToString();
				if (!string.Equals(name, "main", StringComparison.OrdinalIgnoreCase))
					continue;

				var raw = resource.GetData();
				if (raw == null || raw.Length == 0)
					return null;

				return DecompressLauncherPayload(raw);
			}
			return null;
		}

		/// <summary>
		/// 解压 launcher 的 "main" 负载：Windows PowerShell 构建是 gzip，Core 构建是 Brotli。BrotliStream 不在 .NET Framework 中，故用反射取；不可用时抛 <see cref="BrotliUnavailableException"/>，由 exe21sp 转交 pwsh 处理。
		/// </summary>
		private static byte[] DecompressLauncherPayload(byte[] raw) {
			using (var ms = new MemoryStream(raw)) {
				// gzip 流以 1F 8B 开头；否则视为 Brotli（Brotli 无固定魔数）。
				Stream decompressor = raw.Length >= 2 && raw[0] == 0x1F && raw[1] == 0x8B
					? new GZipStream(ms, CompressionMode.Decompress)
					: CreateBrotliDecompressor(ms);
				using (decompressor)
				using (var outMs = new MemoryStream()) {
					decompressor.CopyTo(outMs);
					return outMs.ToArray();
				}
			}
		}

		private static Stream CreateBrotliDecompressor(Stream source) {
			var brotliType = Type.GetType("System.IO.Compression.BrotliStream, System.IO.Compression.Brotli", false);
			if (brotliType == null)
				throw new BrotliUnavailableException();
			return (Stream)Activator.CreateInstance(brotliType, source, CompressionMode.Decompress);
		}

		/// <summary>
		/// 把 ps12exe 生成的 exe 中内嵌的 Win32 图标重建为独立的 .ico 文件。ps12exe 编译时通过 /win32icon（CodeDom）或 ApplicationIcon（Core）把图标写入最外层 PE，反编译时把它还原出来，供 exe21sp 释放在输出目录并由 #_pragma icon 重新引用。
		/// </summary>
		/// <param name="exePath">.exe 文件的完整路径。</param>
		/// <returns>.ico 文件字节；当 exe 没有图标资源时返回 null。</returns>
		public static byte[] ExtractIconFromExe(string exePath) {
			if (string.IsNullOrEmpty(exePath) || !File.Exists(exePath))
				return null;
			try {
				return ExtractIconFromImage(PEImage.FromFile(exePath));
			}
			catch {
				return null;
			}
		}

		private static byte[] ExtractIconFromImage(PEImage image) {
			var root = image.Resources;
			if (root == null)
				return null;
			ResourceDirectory groupDir;
			ResourceDirectory iconDir;
			if (!root.TryGetDirectory(ResourceType.GroupIcon, out groupDir) || groupDir == null)
				return null;
			if (!root.TryGetDirectory(ResourceType.Icon, out iconDir) || iconDir == null)
				return null;

			// 可能有多个图标组（不同语言/名称），取第一个能完整还原的。
			foreach (var groupEntry in groupDir.Entries) {
				if (!groupEntry.IsDirectory)
					continue;
				var groupBytes = ReadFirstEntryBytes((ResourceDirectory)groupEntry);
				if (groupBytes == null)
					continue;
				var ico = BuildIconFile(groupBytes, iconDir);
				if (ico != null)
					return ico;
			}
			return null;
		}

		/// <summary>
		/// 深度优先读取资源目录下第一份数据。PE 资源树是 类型 → 名称/ID → 语言 → 数据，这里不假设层数，直接找叶子数据。
		/// </summary>
		private static byte[] ReadFirstEntryBytes(ResourceDirectory directory) {
			foreach (var entry in directory.Entries) {
				if (entry.IsData) {
					var data = entry as ResourceData;
					var bytes = data == null ? null : ReadSegmentBytes(data.Contents);
					if (bytes != null)
						return bytes;
				}
				else if (entry.IsDirectory) {
					var bytes = ReadFirstEntryBytes((ResourceDirectory)entry);
					if (bytes != null)
						return bytes;
				}
			}
			return null;
		}

		private static byte[] ReadSegmentBytes(ISegment segment) {
			var readable = segment as IReadableSegment;
			return readable == null ? null : Extensions.ToArray(readable);
		}

		/// <summary>
		/// 按资源 ID 在 RT_ICON 目录里找图标图像数据。目录项里存的 ID 是 16 位。
		/// </summary>
		private static byte[] FindIconImageBytes(ResourceDirectory directory, uint id) {
			foreach (var entry in directory.Entries) {
				if (!entry.IsDirectory)
					continue;
				if (entry.Id == id) {
					var bytes = ReadFirstEntryBytes((ResourceDirectory)entry);
					if (bytes != null)
						return bytes;
				}
				var nested = FindIconImageBytes((ResourceDirectory)entry, id);
				if (nested != null)
					return nested;
			}
			return null;
		}

		/// <summary>
		/// 把 GRPICONDIR（RT_GROUP_ICON 数据）和对应的 RT_ICON 图像拼成一个标准 .ico 文件。每个目录项 14 字节：宽/高/色数/保留 + 平面数 + 位深 + 数据大小 + 图标 ID。
		/// </summary>
		private static byte[] BuildIconFile(byte[] group, ResourceDirectory iconDir) {
			if (group == null || group.Length < 6)
				return null;
			int type = BitConverter.ToUInt16(group, 2);
			int count = BitConverter.ToUInt16(group, 4);
			if (type != 1 || count <= 0 || group.Length < 6 + (count * 14))
				return null;

			var directory = new byte[count][];
			var images = new byte[count][];
			for (int i = 0; i < count; i++) {
				int offset = 6 + (i * 14);
				ushort iconId = BitConverter.ToUInt16(group, offset + 12);
				var image = FindIconImageBytes(iconDir, iconId);
				if (image == null)
					return null;
				images[i] = image;

				var entry = new byte[16];
				entry[0] = group[offset];     // 宽度
				entry[1] = group[offset + 1]; // 高度
				entry[2] = group[offset + 2]; // 颜色数
				entry[3] = group[offset + 3]; // 保留
				Buffer.BlockCopy(group, offset + 4, entry, 4, 2); // 平面数
				Buffer.BlockCopy(group, offset + 6, entry, 6, 2); // 位深
				Buffer.BlockCopy(BitConverter.GetBytes((uint)image.Length), 0, entry, 8, 4);
				directory[i] = entry;
			}

			using (var output = new MemoryStream()) {
				output.Write(BitConverter.GetBytes((ushort)0), 0, 2);
				output.Write(BitConverter.GetBytes((ushort)1), 0, 2);
				output.Write(BitConverter.GetBytes((ushort)count), 0, 2);
				uint dataOffset = (uint)(6 + (count * 16));
				for (int i = 0; i < count; i++) {
					Buffer.BlockCopy(BitConverter.GetBytes(dataOffset), 0, directory[i], 12, 4);
					output.Write(directory[i], 0, directory[i].Length);
					dataOffset += (uint)images[i].Length;
				}
				for (int i = 0; i < count; i++)
					output.Write(images[i], 0, images[i].Length);
				return output.ToArray();
			}
		}

		private static string TryExtractFromTinySharp(string exePath) {
			var peFile = PEFile.FromFile(exePath);
			// 仅当该 PE 是 .NET 程序集（含 CLR 头）时才视作 TinySharp。否则原生 exe（如 notepad.exe）会从 .text 中读出垃圾数据。
			if (peFile.OptionalHeader == null)
				return null;
			var clrDir = peFile.OptionalHeader.GetDataDirectory(DataDirectoryIndex.ClrDirectory);
			if (clrDir.Size == 0 || !clrDir.IsPresentInPE)
				return null;

			// 从这里开始我们将其视为潜在的 TinySharp exe；布局解析失败必须抛出异常。
			PESection section = null;
			foreach (var s in peFile.Sections) {
				if (!object.ReferenceEquals(s.Name, null) && s.Name.ToString() == ".text") {
					section = s;
					break;
				}
			}
			if (section == null)
				throw new InvalidOperationException("TinySharpNoTextSection");
			var size = (uint)Math.Min(section.GetPhysicalSize(), 1024 * 1024);
			if (size == 0)
				throw new InvalidOperationException("TinySharpTextSectionEmpty");
			var sectionReader = peFile.CreateReaderAtFileOffset(section.Offset, size);
			var raw = sectionReader.ReadBytes((int)sectionReader.Length);
			if (raw == null || raw.Length == 0)
				throw new InvalidOperationException("TinySharpCannotReadText");

			// 通过统计 CIL 区域中 ldc.i4 的 VA 引用次数来定位消息字符串。TinySharp 把消息地址打入每个 MessageBoxW 调用点（双路径 MessageBox 构建为 2×，控制台构建为 1×），而基础设施字符串（如 VerQueryValueW 的 subBlock 路径）只被引用一次。因此，映射到 .text 中实际文件内容且引用次数最多的 VA 就是消息——无需内容启发式。
			string message = FindMessageByVARefCount(raw, peFile.OptionalHeader.ImageBase, section);
			if (string.IsNullOrEmpty(message))
				throw new InvalidOperationException("TinySharpPayloadNotRecovered");

			// TinySharp 把非零退出代码内嵌为 CIL：Ldc_I4 (0x20) + 4 字节 LE + Ret (0x2A)。查找最后一处这样的序列。
			int exitCode = TryDetectTinySharpExitCode(raw);

			var builder = new StringBuilder();
			var escaped = message.Replace("'", "''");
			builder.Append('\'').Append(escaped).Append('\'');
			if (exitCode != 0)
				builder.Append("\nexit ").Append(exitCode);
			return builder.ToString();
		}

		/// <summary>
		/// 在 .text 中扫描 TinySharp main 末尾的 CIL：Ldc_I4 (0x20) + 4 字节 LE 退出代码 + Ret (0x2A)。只扫描前 2KB（CIL 区域）；否则 .text 末尾的字符串数据可能产生误匹配。返回最后一处匹配的退出代码，若未找到或不可信则返回 0。
		/// </summary>
		private static int TryDetectTinySharpExitCode(byte[] raw) {
			const byte CilLdcI4 = 0x20;
			const byte CilRet = 0x2A;
			const int MinPlausible = -32768;
			const int MaxPlausible = 32767;
			int scanLen = Math.Min(raw.Length - 6, 2048);
			if (scanLen < 0) return 0;
			int lastExit = 0;
			for (int i = 0; i <= scanLen; i++) {
				if (raw[i] != CilLdcI4 || raw[i + 5] != CilRet)
					continue;
				int code = BitConverter.ToInt32(raw, i + 1);
				if (code >= MinPlausible && code <= MaxPlausible)
					lastExit = code;
			}
			return lastExit;
		}

		private static bool IsPrintableAscii(string s) {
			foreach (var c in s)
				if (c < 32 || c > 126)
					return false;
			return true;
		}

		private static bool IsPrintableUnicode(string s) {
			foreach (var c in s)
				if (char.IsControl(c) && c != '\r' && c != '\n' && c != '\t')
					return false;
			return true;
		}

		/// <summary>
		/// 在 .text 的前 2 KB（CIL 区域）中扫描那些取值位于 .text 物理文件内容内的 VA 的 ldc.i4 操作数。统计每个此类 VA 出现的次数；引用最多者即为消息字符串（TinySharp 在每个 MessageBox 调用点都打入它——2×，而 VerQueryValueW 的 subBlock 路径等基础设施字符串只出现 1×）。不使用内容启发式。
		/// </summary>
		private static string FindMessageByVARefCount(byte[] raw, ulong imageBase, PESection section) {
			ulong textVABase = imageBase + section.Rva;
			// 使用并行数组而非 Dictionary<>，以避免引入额外的程序集引用。2 KB 的 CIL 中最多出现少数几个不同的 .text VA 作为 ldc.i4 操作数。
			const int MaxSlots = 64;
			uint[] vaKeys   = new uint[MaxSlots];
			int[]  vaCounts = new int[MaxSlots];
			int    slotCount = 0;
			int cilEnd = Math.Min(raw.Length - 6, 2048);
			for (int i = 0; i <= cilEnd; i++) {
				if (raw[i] != 0x20) continue; // ldc.i4 操作码
				uint operand = (uint)BitConverter.ToInt32(raw, i + 1);
				// TinySharp 的 imageBase < 2^32，因此 ldc.i4 操作数就是完整的 32 位 VA。
				ulong va = (imageBase & 0xFFFFFFFF00000000UL) | (ulong)operand;
				if (va < textVABase) continue;
				ulong fileOff = va - textVABase;
				if (fileOff >= (ulong)raw.Length) continue; // BSS/虚拟内存——无文件内容
				// 线性查找即可；预计不同候选少于 20 个。
				int idx = -1;
				for (int j = 0; j < slotCount; j++) if (vaKeys[j] == operand) { idx = j; break; }
				if (idx < 0 && slotCount < MaxSlots) { vaKeys[slotCount] = operand; vaCounts[slotCount] = 1; slotCount++; }
				else if (idx >= 0) vaCounts[idx]++;
			}
			// 引用最多的 VA 即消息；相同则取文件偏移最小者（消息被放在最前）。
			int bestCount = 0;
			ulong bestFileOff = ulong.MaxValue;
			uint bestOperand = 0;
			for (int j = 0; j < slotCount; j++) {
				ulong va = (imageBase & 0xFFFFFFFF00000000UL) | (ulong)vaKeys[j];
				ulong fileOff = va - textVABase;
				if (vaCounts[j] > bestCount || (vaCounts[j] == bestCount && fileOff < bestFileOff)) {
					bestCount = vaCounts[j]; bestFileOff = fileOff; bestOperand = vaKeys[j];
				}
			}
			if (bestOperand == 0) return null;
			int off = (int)bestFileOff;
			// 通过检查第二个字节是否为 null（UTF-16LE 特征）来区分编码。MessageBox / WriteConsoleW 构建使用 Unicode（对 ASCII 范围内文本有 raw[off+1] == 0x00）。puts 构建使用纯 ASCII（raw[off+1] 是可打印字节，而非零）。
			bool looksUtf16 = (off + 1 < raw.Length && raw[off + 1] == 0);
			if (looksUtf16) {
				var msgU = TryReadNullTermUnicode(raw, off);
				return msgU ?? TryReadNullTermAscii(raw, off);
			} else {
				var msgA = TryReadNullTermAscii(raw, off);
				return msgA ?? TryReadNullTermUnicode(raw, off);
			}
		}

		private static string TryReadNullTermUnicode(byte[] raw, int offset) {
			if (offset < 0 || offset + 2 > raw.Length) return null;
			int end = offset;
			while (end + 1 < raw.Length && (raw[end] != 0 || raw[end + 1] != 0)) end += 2;
			if (end == offset || end - offset > 8192) return null;
			var s = Encoding.Unicode.GetString(raw, offset, end - offset);
			return IsPrintableUnicode(s) ? s : null;
		}

		private static string TryReadNullTermAscii(byte[] raw, int offset) {
			if (offset < 0 || offset >= raw.Length) return null;
			int end = offset;
			while (end < raw.Length && raw[end] != 0) end++;
			if (end == offset || end - offset > 8192) return null;
			var s = Encoding.ASCII.GetString(raw, offset, end - offset);
			return IsPrintableAscii(s) ? s : null;
		}
	}
}
