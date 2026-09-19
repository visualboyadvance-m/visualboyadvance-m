//代码来自 https://blog.washi.dev/posts/tinysharp/
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Linq;
using System.Runtime.InteropServices;
using AsmResolver;
using AsmResolver.DotNet;
using AsmResolver.DotNet.Builder.Metadata;
using AsmResolver.DotNet.Code.Cil;
using AsmResolver.DotNet.Signatures;
using AsmResolver.IO;
using AsmResolver.PE;
using AsmResolver.PE.Builder;
using AsmResolver.PE.Code;
using AsmResolver.PE.DotNet;
using AsmResolver.PE.DotNet.Cil;
using AsmResolver.PE.DotNet.Metadata;
using AsmResolver.PE.DotNet.Metadata.Tables;
using AsmResolver.PE.Win32Resources;
using AsmResolver.PE.Win32Resources.Icon;
using AsmResolver.PE.Win32Resources.Version;
using AsmResolver.PE.File;

namespace TinySharp {
	public class Program {
		// 压缩壳（cabinet 解压 P/Invoke + 解压 CIL）相对未压缩壳的额外开销估计，须与 ConstProgramCheck.ps1 的 $ConstCompressedOverhead 一致。
		private const int CompressionOverhead = 512;

		private static string ClrVersionString(string targetRuntime) {
			return targetRuntime == "Framework2.0" ? "v2.0." : "v4.0.";
		}

		// 追加一个 static P/Invoke 方法行，返回其方法号（1 起）。
		private static uint AddPInvoke(
			TablesStream tablesStream, ModuleDefinition module,
			StringsStreamBuffer stringsStreamBuffer, BlobStreamBuffer blobStreamBuffer,
			string name, MethodSignature signature
		) {
			var methodTable = tablesStream.GetTable<MethodDefinitionRow>();
			methodTable.Add(new MethodDefinitionRow(
				SegmentReference.Null, MethodImplAttributes.PreserveSig,
				MethodAttributes.Static | MethodAttributes.PInvokeImpl,
				stringsStreamBuffer.GetStringIndex(name),
				blobStreamBuffer.GetBlobIndex(module, new DummyProvider(), signature, ThrowErrorListener.Instance), 1));
			return (uint)methodTable.Count;
		}

		public static Program Compile(
			string targetRuntime, string architecture = "x64",
			string outputValue = "Hello World!", int ExitCode = 0, bool hasOutput = true,
			bool useMessageBox = false
		) {
			if (useMessageBox && hasOutput)
				return CompileMessageBox(targetRuntime, architecture, outputValue, ExitCode);
			string baseFunction = "7";
			bool allASCIIoutput = outputValue.All(c => c >= 0 && c <= 127);

			// 非 ASCII 控制台常量：WriteConsoleW 在 stdout 被重定向（管道/文件）时会失败并丢输出，
			// 因此单独走「控制台 WriteConsoleW / 重定向 UTF-8 WriteFile」的壳（见 CompileUnicode）。
			if (hasOutput && !allASCIIoutput)
				return CompileUnicode(targetRuntime, architecture, outputValue, ExitCode);

			// 输出字符串按 ASCII 或 UTF-16 编码，末尾补 NUL 方便 puts 直接输出。
			byte[] payloadBytes = (allASCIIoutput?Encoding.ASCII:Encoding.Unicode).GetBytes(outputValue+'\0');
			// 常量输出较大时用 XPRESS 压缩内嵌，运行时解压后再打印；只有压缩确实更小才走该路径。
			if (hasOutput) {
				byte[] compressedPayload = TryCompressXpress(payloadBytes);
				if (compressedPayload != null && compressedPayload.Length + CompressionOverhead < payloadBytes.Length)
					return CompileCompressed(targetRuntime, architecture, outputValue, allASCIIoutput, compressedPayload, payloadBytes.Length, ExitCode);
			}
			var module = new ModuleDefinition("Dummy");

			// 包含待输出字符串的段。
			DataSegment segment = new DataSegment(payloadBytes);

			var PEKind = OptionalHeaderMagic.PE64;
			var ArchType = MachineType.Amd64;
			if (architecture != "x64") {
				PEKind = OptionalHeaderMagic.PE32;
				ArchType = MachineType.I386;
			}

			// 初始化新的 PE 映像并设置一些默认值。
			var image = new PEImage {
				ImageBase = 0x00000000004e0000,
				PEKind = PEKind,
				MachineType = ArchType
			};

			// 确保 PE 加载到给定的映像基址。
			image.DllCharacteristics &= ~DllCharacteristics.DynamicBase;

			// 创建新的元数据流。
			var tablesStream = new TablesStream();
			var blobStreamBuffer = new BlobStreamBuffer();
			var stringsStreamBuffer = new StringsStreamBuffer();

			// 添加空的模块行。
			tablesStream.GetTable<ModuleDefinitionRow>().Add(new ModuleDefinitionRow());

			// 为 main 函数添加容器类型定义（<Module>）。
			tablesStream.GetTable<TypeDefinitionRow>().Add(new TypeDefinitionRow(
				0, 0, 0, 0, 1, 1
			));

			var methodTable = tablesStream.GetTable<MethodDefinitionRow>();

			// 添加 puts 方法。
			if (hasOutput)
				if(allASCIIoutput) {
					baseFunction = "puts";
					methodTable.Add(new MethodDefinitionRow(
						SegmentReference.Null,
						MethodImplAttributes.PreserveSig,
						MethodAttributes.Static | MethodAttributes.PInvokeImpl,
						stringsStreamBuffer.GetStringIndex("puts"),
						blobStreamBuffer.GetBlobIndex(
							module,
							new DummyProvider(),
							MethodSignature.CreateStatic(
								module.CorLibTypeFactory.Void,
								new[] { module.CorLibTypeFactory.IntPtr }),
							ThrowErrorListener.Instance),
						1
					));
				}
				else {
					baseFunction = "WriteConsoleW";
					methodTable.Add(new MethodDefinitionRow(
						SegmentReference.Null,
						MethodImplAttributes.PreserveSig,
						MethodAttributes.Static | MethodAttributes.PInvokeImpl,
						stringsStreamBuffer.GetStringIndex("GetStdHandle"),
						blobStreamBuffer.GetBlobIndex(
							module,
							new DummyProvider(),
							MethodSignature.CreateStatic(
								module.CorLibTypeFactory.IntPtr,
								new[] { module.CorLibTypeFactory.Int32 }),
							ThrowErrorListener.Instance),
						1
					));
					methodTable.Add(new MethodDefinitionRow(
						SegmentReference.Null,
						MethodImplAttributes.PreserveSig,
						MethodAttributes.Static | MethodAttributes.PInvokeImpl,
						stringsStreamBuffer.GetStringIndex("WriteConsoleW"),
						blobStreamBuffer.GetBlobIndex(
							module,
							new DummyProvider(),
							MethodSignature.CreateStatic(
								module.CorLibTypeFactory.Void,
								new[]
								{
									module.CorLibTypeFactory.IntPtr,
									module.CorLibTypeFactory.IntPtr,
									module.CorLibTypeFactory.Int32,
									module.CorLibTypeFactory.IntPtr,
									module.CorLibTypeFactory.IntPtr
								}),
							ThrowErrorListener.Instance),
						1
					));
				}

			// 添加调用 puts 的 main 方法。
			using(var codeStream = new MemoryStream()) {
				var assembler = new CilAssembler(new BinaryStreamWriter(codeStream), new CilOperandBuilder(new OriginalMetadataTokenProvider(null), ThrowErrorListener.Instance));
				uint patchIndex = 0;
				if (hasOutput) {
					if(allASCIIoutput) {
						patchIndex = 2;
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ldc_I4, 5112224));
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Call, new MetadataToken(TableIndex.Method, 1)));
					}
					else {
						patchIndex = 12;
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ldc_I4, -11)); // STD_OUTPUT_HANDLE
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Call, new MetadataToken(TableIndex.Method, 1)));
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ldc_I4, 5112224));
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ldc_I4, outputValue.Length)); // 字符串长度
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ldc_I4, 0x00000000)); // 输出的保留大小
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ldc_I4, 0x00000000)); // 保留
						assembler.WriteInstruction(new CilInstruction(CilOpCodes.Call, new MetadataToken(TableIndex.Method, 2)));
					}
				}
				if (ExitCode != 0)
					assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ldc_I4, ExitCode));
				assembler.WriteInstruction(new CilInstruction(CilOpCodes.Ret));

				var body = new CilRawTinyMethodBody(codeStream.ToArray()).AsPatchedSegment();
				if (hasOutput)
					body = body.Patch(patchIndex, AddressFixupType.Absolute32BitAddress, new Symbol(segment.ToReference()));

				var retype = module.CorLibTypeFactory.Void;
				if (ExitCode != 0)
					retype = module.CorLibTypeFactory.Int32;

				methodTable.Add(new MethodDefinitionRow(
					body.ToReference(),
					0,
					MethodAttributes.Static,
					0,
					blobStreamBuffer.GetBlobIndex(
						module,
						new DummyProvider(),
						MethodSignature.CreateStatic(retype),
						ThrowErrorListener.Instance),
					1
				));
			}

			if (hasOutput) {
				// 添加 ucrtbase 模块引用
				var baseLibrary = allASCIIoutput ? "ucrtbase" : "Kernel32";
				tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex(baseLibrary)));

				// 为 puts 方法添加 P/Invoke 元数据。
				if (allASCIIoutput)
					tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
						ImplementationMapAttributes.CallConvCdecl,
						tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 1)),
						stringsStreamBuffer.GetStringIndex("puts"),
						1
					));
				else {
					tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
						ImplementationMapAttributes.CallConvCdecl,
						tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 1)),
						stringsStreamBuffer.GetStringIndex("GetStdHandle"),
						1
					));
					tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
						ImplementationMapAttributes.CallConvCdecl,
						tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 2)),
						stringsStreamBuffer.GetStringIndex("WriteConsoleW"),
						1
					));
				}
			}

			// 定义程序集清单。
			tablesStream.GetTable<AssemblyDefinitionRow>().Add(new AssemblyDefinitionRow(
				0,
				1, 0, 0, 0,
				0,
				0,
				stringsStreamBuffer.GetStringIndex(baseFunction), // CLR 不允许程序集名为空，复用 "puts" 以节省空间。
				0
			));

			// 把所有 .NET 元数据加入 PE 映像。
			var metadataDirectory = new MetadataDirectory {
				VersionString = ClrVersionString(targetRuntime)
			};
			metadataDirectory.Streams.Add(tablesStream);
			metadataDirectory.Streams.Add(blobStreamBuffer.CreateStream());
			metadataDirectory.Streams.Add(stringsStreamBuffer.CreateStream());

			image.DotNetDirectory = new DotNetDirectory {
				EntryPoint = new MetadataToken(TableIndex.Method, hasOutput?allASCIIoutput?2u:3u:1u),
				Metadata = metadataDirectory
			};
			if (architecture == "anycpu")
				image.DotNetDirectory.Flags &= ~DotNetDirectoryFlags.Bit32Required;

			var result = new Program();
			result.Image = image;

			// 把待输出的字符串放入填充数据。
			if (hasOutput) result.OutSegment = segment;

			return result;
		}

		/// <summary>非 ASCII 控制台常量壳：stdout 是控制台时用 WriteConsoleW 打印 UTF-16；句柄不是控制台（被重定向到管道/文件）时 WriteConsoleW 返回 0，改用 WriteFile 输出 UTF-8。这样重定向下不再丢失输出（旧实现直接用 WriteConsoleW，重定向时静默失败）。</summary>
		private static Program CompileUnicode(
			string targetRuntime, string architecture,
			string outputValue, int ExitCode
		) {
			var module = new ModuleDefinition("Dummy");
			// s16 末尾补 NUL（WriteConsoleW 用显式长度，NUL 只供 exe21sp 还原时定位字符串边界）；s8 供重定向输出。
			var utf16Bytes = Encoding.Unicode.GetBytes(outputValue + '\0');
			var utf8Bytes = Encoding.UTF8.GetBytes(outputValue);
			var s16 = new DataSegment(utf16Bytes);
			var s8 = new DataSegment(utf8Bytes);

			var PEKind = OptionalHeaderMagic.PE64;
			var ArchType = MachineType.Amd64;
			if (architecture != "x64") {
				PEKind = OptionalHeaderMagic.PE32;
				ArchType = MachineType.I386;
			}

			var image = new PEImage {
				ImageBase = 0x00000000004e0000,
				PEKind = PEKind,
				MachineType = ArchType
			};
			image.DllCharacteristics &= ~DllCharacteristics.DynamicBase;

			var tablesStream = new TablesStream();
			var blobStreamBuffer = new BlobStreamBuffer();
			var stringsStreamBuffer = new StringsStreamBuffer();
			tablesStream.GetTable<ModuleDefinitionRow>().Add(new ModuleDefinitionRow());
			tablesStream.GetTable<TypeDefinitionRow>().Add(new TypeDefinitionRow(0, 0, 0, 0, 1, 1));

			var methodTable = tablesStream.GetTable<MethodDefinitionRow>();
			var corlib = module.CorLibTypeFactory;
			uint getStdHandleIndex = AddPInvoke(tablesStream, module, stringsStreamBuffer, blobStreamBuffer, "GetStdHandle",
				MethodSignature.CreateStatic(corlib.IntPtr, new[] { corlib.Int32 }));
			uint writeConsoleIndex = AddPInvoke(tablesStream, module, stringsStreamBuffer, blobStreamBuffer, "WriteConsoleW",
				MethodSignature.CreateStatic(corlib.Int32, new[] { corlib.IntPtr, corlib.IntPtr, corlib.Int32, corlib.IntPtr, corlib.IntPtr }));
			uint writeFileIndex = AddPInvoke(tablesStream, module, stringsStreamBuffer, blobStreamBuffer, "WriteFile",
				MethodSignature.CreateStatic(corlib.Int32, new[] { corlib.IntPtr, corlib.IntPtr, corlib.Int32, corlib.IntPtr, corlib.IntPtr }));

			// WriteFile 的 lpNumberOfBytesWritten 不能为 NULL（同步写），需要一个 BSS 槽位。
			var writtenBuf = new VirtualSegment(null, 8u);

			using(var codeStream = new MemoryStream()) {
				var patches = new Dictionary<int, ISegment>();
				Action<byte> emit = b => codeStream.WriteByte(b);
				Action<ISegment> ldcAddress = seg => {
					codeStream.WriteByte(0x20); // ldc.i4 <addr>
					patches[(int)codeStream.Position] = seg;
					codeStream.Write(new byte[4], 0, 4);
				};
				Action<int> ldcInt = v => {
					codeStream.WriteByte(0x20); // ldc.i4 <int32>
					var bytes = BitConverter.GetBytes(v);
					codeStream.Write(bytes, 0, 4);
				};
				Action<uint> callMethod = idx => {
					codeStream.WriteByte(0x28); // call
					var bytes = BitConverter.GetBytes(0x06000000u | idx);
					codeStream.Write(bytes, 0, 4);
				};

				// WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), s16, len, NULL, NULL)
				ldcInt(-11); // STD_OUTPUT_HANDLE
				callMethod(getStdHandleIndex);
				ldcAddress(s16);
				ldcInt(outputValue.Length);
				emit(0x16); // ldc.i4.0
				emit(0x16); // ldc.i4.0
				callMethod(writeConsoleIndex);
				// 返回 0（非控制台句柄）则跳过下面的 UTF-8 回退块
				emit(0x2D); // brtrue.s
				emit(32);   // 回退块固定 32 字节

				// WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s8, len, &written, NULL)
				ldcInt(-11);
				callMethod(getStdHandleIndex);
				ldcAddress(s8);
				ldcInt(utf8Bytes.Length);
				ldcAddress(writtenBuf);
				emit(0x16); // ldc.i4.0
				callMethod(writeFileIndex);
				emit(0x26); // pop

				if (ExitCode != 0)
					ldcInt(ExitCode);
				emit(0x2A); // ret

				var body = BuildFatMethodBody(codeStream.ToArray(), 8, patches);
				var retype = module.CorLibTypeFactory.Void;
				if (ExitCode != 0) retype = module.CorLibTypeFactory.Int32;
				methodTable.Add(new MethodDefinitionRow(
					body.ToReference(), 0, MethodAttributes.Static, 0,
					blobStreamBuffer.GetBlobIndex(module, new DummyProvider(), MethodSignature.CreateStatic(retype), ThrowErrorListener.Instance), 1));
			}
			uint entryPointIndex = (uint)methodTable.Count;

			tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex("Kernel32")));
			var implMapTable = tablesStream.GetTable<ImplementationMapRow>();
			Action<uint, string> addImplMap = (methodIndex, name) =>
				implMapTable.Add(new ImplementationMapRow(
					ImplementationMapAttributes.CallConvStdcall,
					tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, methodIndex)),
					stringsStreamBuffer.GetStringIndex(name), 1));
			addImplMap(getStdHandleIndex, "GetStdHandle");
			addImplMap(writeConsoleIndex, "WriteConsoleW");
			addImplMap(writeFileIndex, "WriteFile");

			tablesStream.GetTable<AssemblyDefinitionRow>().Add(new AssemblyDefinitionRow(
				0, 1, 0, 0, 0, 0, 0,
				stringsStreamBuffer.GetStringIndex("WriteConsoleW"), 0));

			var metadataDirectory = new MetadataDirectory {
				VersionString = ClrVersionString(targetRuntime)
			};
			metadataDirectory.Streams.Add(tablesStream);
			metadataDirectory.Streams.Add(blobStreamBuffer.CreateStream());
			metadataDirectory.Streams.Add(stringsStreamBuffer.CreateStream());
			image.DotNetDirectory = new DotNetDirectory {
				EntryPoint = new MetadataToken(TableIndex.Method, entryPointIndex),
				Metadata = metadataDirectory
			};
			if (architecture == "anycpu")
				image.DotNetDirectory.Flags &= ~DotNetDirectoryFlags.Bit32Required;

			var result = new Program();
			result.Image = image;
			// s16 放在前面：exe21sp 还原时按文件偏移最小者取消息字符串。
			var roData = new SegmentBuilder();
			roData.Add(s16);
			roData.Add(s8);
			result.OutSegment = roData;
			var bssData = new SegmentBuilder();
			bssData.Add(writtenBuf);
			result.WritableSegment = bssData;
			return result;
		}

		/// <summary>压缩常量输出路径：内嵌 XPRESS 压缩字节，运行时用 cabinet.dll 解压到 .data 的 BSS 缓冲区，再沿用 puts/WriteConsoleW 打印。负载不可压缩时不会走到这里。</summary>
		private static Program CompileCompressed(
			string targetRuntime, string architecture, string outputValue,
			bool allASCIIoutput, byte[] compressedPayload, int uncompressedSize, int ExitCode
		) {
			var module = new ModuleDefinition("Dummy");
			// 压缩后的负载放只读的 .text
			DataSegment payload = new DataSegment(compressedPayload);

			var PEKind = OptionalHeaderMagic.PE64;
			var ArchType = MachineType.Amd64;
			if (architecture != "x64") {
				PEKind = OptionalHeaderMagic.PE32;
				ArchType = MachineType.I386;
			}

			var image = new PEImage {
				ImageBase = 0x00000000004e0000,
				PEKind = PEKind,
				MachineType = ArchType
			};
			image.DllCharacteristics &= ~DllCharacteristics.DynamicBase;

			var tablesStream = new TablesStream();
			var blobStreamBuffer = new BlobStreamBuffer();
			var stringsStreamBuffer = new StringsStreamBuffer();
			tablesStream.GetTable<ModuleDefinitionRow>().Add(new ModuleDefinitionRow());
			tablesStream.GetTable<TypeDefinitionRow>().Add(new TypeDefinitionRow(0, 0, 0, 0, 1, 1));

			var methodTable = tablesStream.GetTable<MethodDefinitionRow>();
			var corlib = module.CorLibTypeFactory;

			Func<string, MethodSignature, uint> addPInvoke = (name, signature) =>
				AddPInvoke(tablesStream, module, stringsStreamBuffer, blobStreamBuffer, name, signature);

			uint putsIndex = 0, getStdHandleIndex = 0, writeConsoleIndex = 0;
			if (allASCIIoutput)
				putsIndex = addPInvoke("puts", MethodSignature.CreateStatic(corlib.Void, new[] { corlib.IntPtr }));
			else {
				getStdHandleIndex = addPInvoke("GetStdHandle", MethodSignature.CreateStatic(corlib.IntPtr, new[] { corlib.Int32 }));
				writeConsoleIndex = addPInvoke("WriteConsoleW", MethodSignature.CreateStatic(corlib.Void, new[] {
					corlib.IntPtr, corlib.IntPtr, corlib.Int32, corlib.IntPtr, corlib.IntPtr
				}));
			}
			// cabinet.dll Compression API（Windows 8+），XPRESS 算法与编译期一致
			uint createDecompressorIndex = addPInvoke("CreateDecompressor", MethodSignature.CreateStatic(corlib.Int32, new[] {
				corlib.UInt32, corlib.IntPtr, corlib.IntPtr
			}));
			uint decompressIndex = addPInvoke("Decompress", MethodSignature.CreateStatic(corlib.Int32, new[] {
				corlib.IntPtr, corlib.IntPtr, corlib.IntPtr, corlib.IntPtr, corlib.IntPtr, corlib.IntPtr
			}));
			uint closeDecompressorIndex = addPInvoke("CloseDecompressor", MethodSignature.CreateStatic(corlib.Int32, new[] {
				corlib.IntPtr
			}));

			// .data BSS 缓冲：解压输出、解压器句柄、解压后长度（均不占文件体积）
			var outputBuf = new VirtualSegment(null, (uint)uncompressedSize);
			var handleBuf = new VirtualSegment(null, 8u);
			var resultSizeBuf = new VirtualSegment(null, 8u);

			using(var codeStream = new MemoryStream()) {
				var patches = new Dictionary<int, ISegment>();
				Action<byte> emit = b => codeStream.WriteByte(b);
				Action<ISegment> ldcAddress = seg => {
					codeStream.WriteByte(0x20); // ldc.i4 <addr>
					patches[(int)codeStream.Position] = seg;
					codeStream.Write(new byte[4], 0, 4);
				};
				Action<int> ldcInt = v => {
					codeStream.WriteByte(0x20); // ldc.i4 <int32>
					var bytes = BitConverter.GetBytes(v);
					codeStream.Write(bytes, 0, 4);
				};
				Action<uint> callMethod = idx => {
					codeStream.WriteByte(0x28); // call
					var bytes = BitConverter.GetBytes(0x06000000u | idx);
					codeStream.Write(bytes, 0, 4);
				};

				EmitDecompress(emit, ldcAddress, ldcInt, callMethod,
					payload, compressedPayload.Length, outputBuf, uncompressedSize, handleBuf, resultSizeBuf,
					createDecompressorIndex, decompressIndex, closeDecompressorIndex);

				if (allASCIIoutput) {
					// puts(output)
					ldcAddress(outputBuf);
					callMethod(putsIndex);
				}
				else {
					// WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), output, length, NULL, NULL)
					ldcInt(-11);
					callMethod(getStdHandleIndex);
					ldcAddress(outputBuf);
					ldcInt(outputValue.Length);
					emit(0x16); // ldc.i4.0
					emit(0x16); // ldc.i4.0
					callMethod(writeConsoleIndex);
				}

				if (ExitCode != 0)
					ldcInt(ExitCode);
				emit(0x2A); // ret

				var body = BuildFatMethodBody(codeStream.ToArray(), 8, patches);

				var retype = module.CorLibTypeFactory.Void;
				if (ExitCode != 0) retype = module.CorLibTypeFactory.Int32;
				methodTable.Add(new MethodDefinitionRow(
					body.ToReference(), 0, MethodAttributes.Static, 0,
					blobStreamBuffer.GetBlobIndex(module, new DummyProvider(), MethodSignature.CreateStatic(retype), ThrowErrorListener.Instance), 1));
			}
			uint entryPointIndex = (uint)methodTable.Count; // main 是最后添加的一行

			// 模块引用：1=基础库（ucrtbase/Kernel32），2=cabinet
			var baseLibrary = allASCIIoutput ? "ucrtbase" : "Kernel32";
			tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex(baseLibrary)));
			tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex("cabinet")));

			var implMapTable = tablesStream.GetTable<ImplementationMapRow>();
			Action<uint, string, uint, ImplementationMapAttributes> addImplMap = (methodIndex, name, moduleIndex, conv) =>
				implMapTable.Add(new ImplementationMapRow(
					conv,
					tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, methodIndex)),
					stringsStreamBuffer.GetStringIndex(name),
					moduleIndex));

			if (allASCIIoutput)
				addImplMap(putsIndex, "puts", 1, ImplementationMapAttributes.CallConvCdecl);
			else {
				addImplMap(getStdHandleIndex, "GetStdHandle", 1, ImplementationMapAttributes.CallConvCdecl);
				addImplMap(writeConsoleIndex, "WriteConsoleW", 1, ImplementationMapAttributes.CallConvCdecl);
			}
			addImplMap(createDecompressorIndex, "CreateDecompressor", 2, ImplementationMapAttributes.CallConvStdcall);
			addImplMap(decompressIndex, "Decompress", 2, ImplementationMapAttributes.CallConvStdcall);
			addImplMap(closeDecompressorIndex, "CloseDecompressor", 2, ImplementationMapAttributes.CallConvStdcall);

			tablesStream.GetTable<AssemblyDefinitionRow>().Add(new AssemblyDefinitionRow(
				0, 1, 0, 0, 0, 0, 0,
				stringsStreamBuffer.GetStringIndex(allASCIIoutput ? "puts" : "WriteConsoleW"), 0));

			var metadataDirectory = new MetadataDirectory {
				VersionString = ClrVersionString(targetRuntime)
			};
			metadataDirectory.Streams.Add(tablesStream);
			metadataDirectory.Streams.Add(blobStreamBuffer.CreateStream());
			metadataDirectory.Streams.Add(stringsStreamBuffer.CreateStream());
			image.DotNetDirectory = new DotNetDirectory {
				EntryPoint = new MetadataToken(TableIndex.Method, entryPointIndex),
				Metadata = metadataDirectory
			};
			if (architecture == "anycpu")
				image.DotNetDirectory.Flags &= ~DotNetDirectoryFlags.Bit32Required;

			var result = new Program();
			result.Image = image;
			result.OutSegment = payload;
			var bssData = new SegmentBuilder();
			bssData.Add(outputBuf);
			bssData.Add(handleBuf);
			bssData.Add(resultSizeBuf);
			result.WritableSegment = bssData;
			return result;
		}

		[DllImport("cabinet.dll", SetLastError = true)]
		private static extern bool CreateCompressor(uint algorithm, IntPtr allocationRoutines, out IntPtr compressorHandle);
		[DllImport("cabinet.dll", SetLastError = true)]
		private static extern bool Compress(IntPtr compressorHandle, byte[] input, IntPtr inputSize, byte[] output, IntPtr outputSize, out IntPtr resultSize);
		[DllImport("cabinet.dll", SetLastError = true)]
		private static extern bool CloseCompressor(IntPtr compressorHandle);

		/// <summary>用 cabinet.dll 的 XPRESS 算法压缩；任何失败（含 Windows 8 以下无此 API）都返回 null，调用方退回未压缩内嵌。</summary>
		private static byte[] TryCompressXpress(byte[] input) {
			try {
				IntPtr handle;
				if (!CreateCompressor(3u, IntPtr.Zero, out handle))
					return null;
				try {
					byte[] buffer = new byte[input.Length + (input.Length / 2) + 1024];
					IntPtr resultSize;
					if (!Compress(handle, input, (IntPtr)input.Length, buffer, (IntPtr)buffer.Length, out resultSize))
						return null;
					int size = (int)resultSize;
					byte[] result = new byte[size];
					Buffer.BlockCopy(buffer, 0, result, 0, size);
					return result;
				}
				finally {
					CloseCompressor(handle);
				}
			}
			catch {
				return null;
			}
		}

		/// <summary>把 XPRESS 解压序列写入 CIL：CreateDecompressor → Decompress → CloseDecompressor，解压结果落在 outputBuf；handleBuf / resultSizeBuf 作为输出参数中转。</summary>
		private static void EmitDecompress(
			Action<byte> emit, Action<ISegment> ldcAddress, Action<int> ldcInt, Action<uint> callMethod,
			ISegment payload, int compressedLength, ISegment outputBuf, int uncompressedSize,
			ISegment handleBuf, ISegment resultSizeBuf,
			uint createIndex, uint decompressIndex, uint closeIndex
		) {
			// CreateDecompressor(COMPRESS_ALGORITHM_XPRESS, NULL, &handle)
			emit(0x19); // ldc.i4.3
			emit(0x16); // ldc.i4.0
			ldcAddress(handleBuf);
			callMethod(createIndex);
			emit(0x26); // pop

			// Decompress(handle, compressed, compressedSize, output, outputSize, &resultSize)
			ldcAddress(handleBuf);
			emit(0x4D); // ldind.i
			ldcAddress(payload);
			ldcInt(compressedLength);
			ldcAddress(outputBuf);
			ldcInt(uncompressedSize);
			ldcAddress(resultSizeBuf);
			callMethod(decompressIndex);
			emit(0x26); // pop

			// CloseDecompressor(handle)
			ldcAddress(handleBuf);
			emit(0x4D); // ldind.i
			callMethod(closeIndex);
			emit(0x26); // pop
		}

		/// <summary>用 12 字节 fat method header 包装 CIL 代码流，并应用绝对地址 patch（偏移 +12）。</summary>
		private static ISegment BuildFatMethodBody(byte[] code, int maxStack, Dictionary<int, ISegment> patches) {
			byte[] header = {
				0x03, 0x30, (byte)maxStack, 0,
				(byte)code.Length, (byte)(code.Length >> 8), (byte)(code.Length >> 16), (byte)(code.Length >> 24),
				0, 0, 0, 0
			};
			byte[] fullMethod = new byte[header.Length + code.Length];
			Buffer.BlockCopy(header, 0, fullMethod, 0, header.Length);
			Buffer.BlockCopy(code, 0, fullMethod, header.Length, code.Length);
			var body = new DataSegment(fullMethod).AsPatchedSegment();
			foreach (var patch in patches)
				body = body.Patch((uint)(12 + patch.Key), AddressFixupType.Absolute32BitAddress, new Symbol(patch.Value.ToReference()));
			return body;
		}

		/// <summary>构建最小 PE，显示 MessageBoxW(text, caption) 后退出。用于 -noConsole 常量输出。标题在运行时解析：先尝试 Win32 版本资源的 FileDescription（title），回退到 GetModuleFileNameW+PathFindFileNameW（文件名），因此两者都能在 exe 改名后继续生效并遵循 -title。</summary>
		private static Program CompileMessageBox(string targetRuntime, string architecture, string outputValue, int ExitCode) {
			var module = new ModuleDefinition("Dummy");
			// 文本恒为 UTF-16；较大且可压缩时改存 XPRESS 压缩字节，运行时先解压再弹窗。
			byte[] rawText = Encoding.Unicode.GetBytes(outputValue + '\0');
			byte[] compressedText = TryCompressXpress(rawText);
			if (compressedText != null && compressedText.Length + CompressionOverhead >= rawText.Length)
				compressedText = null;
			bool useCompressed = compressedText != null;
			DataSegment textSegment = new DataSegment(useCompressed ? compressedText : rawText);
			// VerQueryValueW 子块路径——与 AsmResolver StringTable(language:0, codepage:0x4b0) 匹配
			DataSegment subBlockStr = new DataSegment(Encoding.Unicode.GetBytes("\\StringFileInfo\\000004b0\\FileDescription\0"));

			// 可写 BSS 段（由 OS 加载器零初始化，不占文件字节）
			var pathBufSeg   = new VirtualSegment(null, 260 * 2u); // GetModuleFileNameW 路径缓冲区
			var viBufSeg     = new VirtualSegment(null, 4096u);    // GetFileVersionInfoW 数据缓冲区
			var pValueBufSeg = new VirtualSegment(null, 8u);       // VerQueryValueW 输出指针（x64 最多 8 字节）
			var lenBufSeg    = new VirtualSegment(null, 4u);       // VerQueryValueW 输出长度（UINT）
			// 压缩时：解压输出缓冲 + 解压器句柄 + 解压后长度
			var outputBufSeg    = useCompressed ? new VirtualSegment(null, (uint)rawText.Length) : null;
			var handleBufSeg    = useCompressed ? new VirtualSegment(null, 8u) : null;
			var resultSizeBufSeg = useCompressed ? new VirtualSegment(null, 8u) : null;

			var PEKind = OptionalHeaderMagic.PE64;
			var ArchType = MachineType.Amd64;
			if (architecture != "x64") {
				PEKind = OptionalHeaderMagic.PE32;
				ArchType = MachineType.I386;
			}
			var image = new PEImage {
				ImageBase = 0x00000000004e0000,
				PEKind = PEKind,
				MachineType = ArchType
			};
			image.DllCharacteristics &= ~DllCharacteristics.DynamicBase;

			var tablesStream = new TablesStream();
			var blobStreamBuffer = new BlobStreamBuffer();
			var stringsStreamBuffer = new StringsStreamBuffer();
			tablesStream.GetTable<ModuleDefinitionRow>().Add(new ModuleDefinitionRow());
			tablesStream.GetTable<TypeDefinitionRow>().Add(new TypeDefinitionRow(0, 0, 0, 0, 1, 1));

			var methodTable = tablesStream.GetTable<MethodDefinitionRow>();
			// 1: MessageBoxW (user32)
			methodTable.Add(new MethodDefinitionRow(
				SegmentReference.Null, MethodImplAttributes.PreserveSig,
				MethodAttributes.Static | MethodAttributes.PInvokeImpl,
				stringsStreamBuffer.GetStringIndex("MessageBoxW"),
				blobStreamBuffer.GetBlobIndex(module, new DummyProvider(),
					MethodSignature.CreateStatic(module.CorLibTypeFactory.Void, new[] {
						module.CorLibTypeFactory.IntPtr, module.CorLibTypeFactory.IntPtr,
						module.CorLibTypeFactory.IntPtr, module.CorLibTypeFactory.UInt32
					}), ThrowErrorListener.Instance), 1));
			// 2: GetModuleFileNameW (kernel32): (hModule, lpFilename, nSize) -> void
			methodTable.Add(new MethodDefinitionRow(
				SegmentReference.Null, MethodImplAttributes.PreserveSig,
				MethodAttributes.Static | MethodAttributes.PInvokeImpl,
				stringsStreamBuffer.GetStringIndex("GetModuleFileNameW"),
				blobStreamBuffer.GetBlobIndex(module, new DummyProvider(),
					MethodSignature.CreateStatic(module.CorLibTypeFactory.Void, new[] {
						module.CorLibTypeFactory.IntPtr, module.CorLibTypeFactory.IntPtr,
						module.CorLibTypeFactory.UInt32
					}), ThrowErrorListener.Instance), 1));
			// 3: PathFindFileNameW (shlwapi): (pszPath) -> IntPtr
			methodTable.Add(new MethodDefinitionRow(
				SegmentReference.Null, MethodImplAttributes.PreserveSig,
				MethodAttributes.Static | MethodAttributes.PInvokeImpl,
				stringsStreamBuffer.GetStringIndex("PathFindFileNameW"),
				blobStreamBuffer.GetBlobIndex(module, new DummyProvider(),
					MethodSignature.CreateStatic(module.CorLibTypeFactory.IntPtr,
						new[] { module.CorLibTypeFactory.IntPtr }), ThrowErrorListener.Instance), 1));
			// 4: GetFileVersionInfoW (version): (lpszFileName, dwHandle, dwLen, lpData) -> Int32 (BOOL)
			methodTable.Add(new MethodDefinitionRow(
				SegmentReference.Null, MethodImplAttributes.PreserveSig,
				MethodAttributes.Static | MethodAttributes.PInvokeImpl,
				stringsStreamBuffer.GetStringIndex("GetFileVersionInfoW"),
				blobStreamBuffer.GetBlobIndex(module, new DummyProvider(),
					MethodSignature.CreateStatic(module.CorLibTypeFactory.Int32, new[] {
						module.CorLibTypeFactory.IntPtr, module.CorLibTypeFactory.UInt32,
						module.CorLibTypeFactory.UInt32, module.CorLibTypeFactory.IntPtr
					}), ThrowErrorListener.Instance), 1));
			// 5: VerQueryValueW (version): (pBlock, lpSubBlock, lplpBuffer, puLen) -> Int32 (BOOL)
			methodTable.Add(new MethodDefinitionRow(
				SegmentReference.Null, MethodImplAttributes.PreserveSig,
				MethodAttributes.Static | MethodAttributes.PInvokeImpl,
				stringsStreamBuffer.GetStringIndex("VerQueryValueW"),
				blobStreamBuffer.GetBlobIndex(module, new DummyProvider(),
					MethodSignature.CreateStatic(module.CorLibTypeFactory.Int32, new[] {
						module.CorLibTypeFactory.IntPtr, module.CorLibTypeFactory.IntPtr,
						module.CorLibTypeFactory.IntPtr, module.CorLibTypeFactory.IntPtr
					}), ThrowErrorListener.Instance), 1));

			Func<string, MethodSignature, uint> addPInvoke = (name, signature) =>
				AddPInvoke(tablesStream, module, stringsStreamBuffer, blobStreamBuffer, name, signature);

			// 压缩时追加 cabinet 解压方法（6/7/8）和一个返回文本指针的辅助方法（9），main 顺延为 10
			uint createDecompressorIndex = 0, decompressIndex = 0, closeDecompressorIndex = 0, textHelperIndex = 0;
			if (useCompressed) {
				var c = module.CorLibTypeFactory;
				createDecompressorIndex = addPInvoke("CreateDecompressor", MethodSignature.CreateStatic(c.Int32, new[] {
					c.UInt32, c.IntPtr, c.IntPtr
				}));
				decompressIndex = addPInvoke("Decompress", MethodSignature.CreateStatic(c.Int32, new[] {
					c.IntPtr, c.IntPtr, c.IntPtr, c.IntPtr, c.IntPtr, c.IntPtr
				}));
				closeDecompressorIndex = addPInvoke("CloseDecompressor", MethodSignature.CreateStatic(c.Int32, new[] {
					c.IntPtr
				}));

				// helper: 解压后返回文本指针（IntPtr），main 的两个标题分支都通过 call 它取文本
				textHelperIndex = (uint)methodTable.Count + 1;
				using (var helperStream = new MemoryStream()) {
					var helperPatches = new Dictionary<int, ISegment>();
					Action<byte> hEmit = b => helperStream.WriteByte(b);
					Action<ISegment> hLdcAddress = seg => {
						helperStream.WriteByte(0x20);
						helperPatches[(int)helperStream.Position] = seg;
						helperStream.Write(new byte[4], 0, 4);
					};
					Action<int> hLdcInt = v => {
						helperStream.WriteByte(0x20);
						var bytes = BitConverter.GetBytes(v);
						helperStream.Write(bytes, 0, 4);
					};
					Action<uint> hCall = idx => {
						helperStream.WriteByte(0x28);
						var bytes = BitConverter.GetBytes(0x06000000u | idx);
						helperStream.Write(bytes, 0, 4);
					};
					EmitDecompress(hEmit, hLdcAddress, hLdcInt, hCall,
						textSegment, compressedText.Length, outputBufSeg, rawText.Length, handleBufSeg, resultSizeBufSeg,
						createDecompressorIndex, decompressIndex, closeDecompressorIndex);
					hLdcAddress(outputBufSeg); // 返回解压后的文本指针
					hEmit(0x2A); // ret
					var helperBody = BuildFatMethodBody(helperStream.ToArray(), 6, helperPatches);
					methodTable.Add(new MethodDefinitionRow(
						helperBody.ToReference(), 0, MethodAttributes.Static, 0,
						blobStreamBuffer.GetBlobIndex(module, new DummyProvider(),
							MethodSignature.CreateStatic(c.IntPtr), ThrowErrorListener.Instance), 1));
				}
			}

			// Main（方法 #6）——fat CIL 方法体（代码超过 63 字节，超出 tiny 格式限制）。
			// IL 布局（代码流偏移，位于 12 字节 fat 头之前）：
			//   [0]  ldc.i4 0            hModule=0，用于 GetModuleFileNameW
			//   [5]  ldc.i4 [pathBuf]    PATCH @6
			//   [10] ldc.i4 260
			//   [15] call  Method#2      GetModuleFileNameW(0, pathBuf, 260)
			//   [20] ldc.i4 [pathBuf]    PATCH @21
			//   [25] ldc.i4 0            dwHandle=0
			//   [30] ldc.i4 4096         dwLen
			//   [35] ldc.i4 [viBuf]      PATCH @36
			//   [40] call  Method#4      GetFileVersionInfoW → BOOL 入栈
			//   [45] brfalse.s 55        → FALLBACK @102 (next=47, 102-47=55)
			//   [47] ldc.i4 [viBuf]      PATCH @48
			//   [52] ldc.i4 [subBlock]   PATCH @53
			//   [57] ldc.i4 [pValueBuf]  PATCH @58
			//   [62] ldc.i4 [lenBuf]     PATCH @63
			//   [67] call  Method#5      VerQueryValueW → BOOL 入栈
			//   [72] brfalse.s 28        → FALLBACK @102 (next=74, 102-74=28)
			//   [74] ldc.i4 0            hWnd
			//   [79] ldc.i4 [text]       PATCH @80
			//   [84] ldc.i4 [pValueBuf]  PATCH @85
			//   [89] ldind.i             *pValueBuf → title 指针
			//   [90] ldc.i4 0            uType
			//   [95] call  Method#1      MessageBoxW(0, text, titlePtr, 0)
			//   [100] br.s DONE          → @132 或 @137（偏移 30 或 35）
			//   FALLBACK @102:
			//   [102] ldc.i4 0           hWnd
			//   [107] ldc.i4 [text]      PATCH @108
			//   [112] ldc.i4 [pathBuf]   PATCH @113
			//   [117] call  Method#3     PathFindFileNameW(pathBuf) → filenamePtr
			//   [122] ldc.i4 0           uType
			//   [127] call  Method#1     MessageBoxW(0, text, filenamePtr, 0)
			//   DONE @132 [或 @137（当 ExitCode!=0）]:
			//   [132] ldc.i4 ExitCode    （仅在 ExitCode != 0 时）
			//   [132|137] ret
			// 段内 patch 偏移 = 代码偏移 + 12（fat 头大小）。
			using (var codeStream = new MemoryStream()) {
				Action<int> writeLdc = v => {
					codeStream.WriteByte(0x20);
					var b = BitConverter.GetBytes(v);
					codeStream.Write(b, 0, 4);
				};
				Action<int> writeCall = m => {
					codeStream.WriteByte(0x28);
					var b = BitConverter.GetBytes(0x06000000 | m);
					codeStream.Write(b, 0, 4);
				};
				// 文本指针：压缩时 call helper 解压取得，未压缩时 ldc.i4 [text]（两者等长，偏移不变）
				Action writeText = () => {
					if (useCompressed) writeCall((int)textHelperIndex);
					else writeLdc(0);
				};

			writeLdc(0);       // [0]  hModule=0
			writeLdc(0);       // [5]  pathBuf  PATCH@6
			writeLdc(260);     // [10] nSize
			writeCall(2);      // [15] GetModuleFileNameW

			writeLdc(0);       // [20] pathBuf  PATCH@21
			writeLdc(0);       // [25] dwHandle=0
			writeLdc(4096);    // [30] dwLen
			writeLdc(0);       // [35] viBuf    PATCH@36
			writeCall(4);      // [40] GetFileVersionInfoW → BOOL

			codeStream.WriteByte(0x2C); // [45] brfalse.s
			codeStream.WriteByte(55);   // [46] → FALLBACK @102 (next=47, 102-47=55)

			writeLdc(0);       // [47] viBuf       PATCH@48
			writeLdc(0);       // [52] subBlockStr  PATCH@53
			writeLdc(0);       // [57] pValueBuf    PATCH@58
			writeLdc(0);       // [62] lenBuf       PATCH@63
			writeCall(5);      // [67] VerQueryValueW → BOOL

			codeStream.WriteByte(0x2C); // [72] brfalse.s
			codeStream.WriteByte(28);   // [73] → FALLBACK @102 (next=74, 102-74=28)

			writeLdc(0);       // [74] hWnd
			writeText();       // [79] text      （未压缩时 PATCH@80）
			writeLdc(0);       // [84] pValueBuf PATCH@85
			codeStream.WriteByte(0x4D); // [89] ldind.i → 解引用 pValueBuf
			writeLdc(0);       // [90] uType=0
			writeCall(1);      // [95] MessageBoxW(0, text, titlePtr, 0)

			codeStream.WriteByte(0x2B); // [100] br.s
			codeStream.WriteByte((byte)(30 + (ExitCode != 0 ? 5 : 0))); // [101] → DONE

			// FALLBACK @102
			writeLdc(0);       // [102] hWnd
			writeText();       // [107] text     （未压缩时 PATCH@108）
			writeLdc(0);       // [112] pathBuf  PATCH@113
			writeCall(3);      // [117] PathFindFileNameW(pathBuf) → filenamePtr
			writeLdc(0);       // [122] uType=0
			writeCall(1);      // [127] MessageBoxW(0, text, filenamePtr, 0)

			// DONE @132
			if (ExitCode != 0) writeLdc(ExitCode); // [132] 仅在需要时
				codeStream.WriteByte(0x2A); // ret

				byte[] code = codeStream.ToArray();
				// Fat 方法头：flags=0x3003（fat，hdrSize=3 个双字），MaxStack=4，CodeSize，LocalVarSigTok=0
				byte[] header = {
					0x03, 0x30, 4, 0,
					(byte)code.Length, (byte)(code.Length >> 8), (byte)(code.Length >> 16), (byte)(code.Length >> 24),
					0, 0, 0, 0
				};
				byte[] fullMethod = new byte[header.Length + code.Length];
				Buffer.BlockCopy(header, 0, fullMethod, 0, header.Length);
				Buffer.BlockCopy(code, 0, fullMethod, header.Length, code.Length);

				// Patch 偏移 = int32 操作数的代码流偏移 + 12（fat 头）
				var body = new DataSegment(fullMethod).AsPatchedSegment();
				body = body.Patch(12 +  6, AddressFixupType.Absolute32BitAddress, new Symbol(pathBufSeg.ToReference()));
				body = body.Patch(12 + 21, AddressFixupType.Absolute32BitAddress, new Symbol(pathBufSeg.ToReference()));
				body = body.Patch(12 + 36, AddressFixupType.Absolute32BitAddress, new Symbol(viBufSeg.ToReference()));
				body = body.Patch(12 + 48, AddressFixupType.Absolute32BitAddress, new Symbol(viBufSeg.ToReference()));
				body = body.Patch(12 + 53, AddressFixupType.Absolute32BitAddress, new Symbol(subBlockStr.ToReference()));
				body = body.Patch(12 + 58, AddressFixupType.Absolute32BitAddress, new Symbol(pValueBufSeg.ToReference()));
				body = body.Patch(12 + 63, AddressFixupType.Absolute32BitAddress, new Symbol(lenBufSeg.ToReference()));
				if (!useCompressed) {
					body = body.Patch(12 + 80, AddressFixupType.Absolute32BitAddress, new Symbol(textSegment.ToReference()));
					body = body.Patch(12 + 108, AddressFixupType.Absolute32BitAddress, new Symbol(textSegment.ToReference()));
				}
				body = body.Patch(12 + 85, AddressFixupType.Absolute32BitAddress, new Symbol(pValueBufSeg.ToReference()));
				body = body.Patch(12 + 113, AddressFixupType.Absolute32BitAddress, new Symbol(pathBufSeg.ToReference()));

				var retype = module.CorLibTypeFactory.Void;
				if (ExitCode != 0) retype = module.CorLibTypeFactory.Int32;
				methodTable.Add(new MethodDefinitionRow(
					body.ToReference(), 0, MethodAttributes.Static, 0,
					blobStreamBuffer.GetBlobIndex(module, new DummyProvider(),
						MethodSignature.CreateStatic(retype), ThrowErrorListener.Instance), 1));
			}

			// 模块引用：1=user32，2=kernel32，3=shlwapi，4=version，5=cabinet（压缩时）
			tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex("user32")));
			tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex("kernel32")));
			tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex("shlwapi")));
			tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex("version")));
			if (useCompressed)
				tablesStream.GetTable<ModuleReferenceRow>().Add(new ModuleReferenceRow(stringsStreamBuffer.GetStringIndex("cabinet")));

			tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
				ImplementationMapAttributes.CallConvStdcall,
				tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 1)),
				stringsStreamBuffer.GetStringIndex("MessageBoxW"), 1));
			tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
				ImplementationMapAttributes.CallConvStdcall,
				tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 2)),
				stringsStreamBuffer.GetStringIndex("GetModuleFileNameW"), 2));
			tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
				ImplementationMapAttributes.CallConvStdcall,
				tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 3)),
				stringsStreamBuffer.GetStringIndex("PathFindFileNameW"), 3));
			tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
				ImplementationMapAttributes.CallConvStdcall,
				tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 4)),
				stringsStreamBuffer.GetStringIndex("GetFileVersionInfoW"), 4));
			tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
				ImplementationMapAttributes.CallConvStdcall,
				tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, 5)),
				stringsStreamBuffer.GetStringIndex("VerQueryValueW"), 4));
			if (useCompressed) {
				tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
					ImplementationMapAttributes.CallConvStdcall,
					tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, createDecompressorIndex)),
					stringsStreamBuffer.GetStringIndex("CreateDecompressor"), 5));
				tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
					ImplementationMapAttributes.CallConvStdcall,
					tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, decompressIndex)),
					stringsStreamBuffer.GetStringIndex("Decompress"), 5));
				tablesStream.GetTable<ImplementationMapRow>().Add(new ImplementationMapRow(
					ImplementationMapAttributes.CallConvStdcall,
					tablesStream.GetIndexEncoder(CodedIndex.MemberForwarded).EncodeToken(new MetadataToken(TableIndex.Method, closeDecompressorIndex)),
					stringsStreamBuffer.GetStringIndex("CloseDecompressor"), 5));
			}

			tablesStream.GetTable<AssemblyDefinitionRow>().Add(new AssemblyDefinitionRow(
				0, 1, 0, 0, 0, 0, 0,
				stringsStreamBuffer.GetStringIndex("user32"), 0));

			var metadataDirectory = new MetadataDirectory {
				VersionString = ClrVersionString(targetRuntime)
			};
			metadataDirectory.Streams.Add(tablesStream);
			metadataDirectory.Streams.Add(blobStreamBuffer.CreateStream());
			metadataDirectory.Streams.Add(stringsStreamBuffer.CreateStream());
			image.DotNetDirectory = new DotNetDirectory {
				// 方法：1=MessageBoxW,2=GetModuleFileNameW,3=PathFindFileNameW,4=GetFileVersionInfoW,5=VerQueryValueW,6/7/8=cabinet 解压(压缩时),9=文本指针 helper(压缩时),末位=Main
				EntryPoint = new MetadataToken(TableIndex.Method, useCompressed ? 10u : 6u),
				Metadata = metadataDirectory
			};
			if (architecture == "anycpu")
				image.DotNetDirectory.Flags &= ~DotNetDirectoryFlags.Bit32Required;

			var result = new Program();
			result.Image = image;
			// 只读数据：text + VerQueryValueW 子块路径（都在 .text）
			var roData = new SegmentBuilder();
			roData.Add(textSegment);
			roData.Add(subBlockStr, 2);
			result.OutSegment = roData;
			// 可写 BSS 数据（在 .data）
			var bssData = new SegmentBuilder();
			bssData.Add(pathBufSeg);
			bssData.Add(viBufSeg);
			bssData.Add(pValueBufSeg);
			bssData.Add(lenBufSeg);
			if (useCompressed) {
				bssData.Add(outputBufSeg);
				bssData.Add(handleBufSeg);
				bssData.Add(resultSizeBufSeg);
			}
			result.WritableSegment = bssData;
			return result;
		}

		private ISegment OutSegment;
		private ISegment WritableSegment;
		private PEImage Image;
		public void Build(string OutFile) {
			// 不要替换成 ManagedPEFileBuilder：体积会变大（见文件顶部的 DESIGN）。OutSegment（只读常量）→ .text；WritableSegment（如路径缓冲区）→ .data
			var file = new MinimalPEFileBuilder(OutSegment, WritableSegment).CreateFile(this.Image);
			file.Write(OutFile);
		}
		public void SetWin32Icon(string IconFile) {
			byte[] header = File.ReadAllBytes(IconFile);
			if (header.Length < 22) return;
			ushort count = BitConverter.ToUInt16(header, 4);
			if (count == 0) return;
			// 第一个图标目录项：width、height、colors、reserved、planes、bpp、size、offset
			byte w = header[6], h = header[7];
			ushort planes = BitConverter.ToUInt16(header, 10);
			ushort bpp = BitConverter.ToUInt16(header, 12);
			uint size = BitConverter.ToUInt32(header, 14);
			uint offset = BitConverter.ToUInt32(header, 18);
			if (offset + size > (uint)header.Length) return;
			byte[] iconBytes = new byte[size];
			Buffer.BlockCopy(header, (int)offset, iconBytes, 0, (int)size);

			var entry = new IconEntry(1, 0) {
				Width = w,
				Height = h,
				ColorCount = 0,
				Planes = planes,
				BitsPerPixel = bpp,
				PixelData = new DataSegment(iconBytes)
			};
			var group = new IconGroup(1u, 0u) { Type = IconType.Icon };
			group.Icons.Add(entry);
			var iconResource = new IconResource(IconType.Icon);
			iconResource.Groups.Add(group);

			if (this.Image.Resources == null) this.Image.Resources = new ResourceDirectory(0u);
			iconResource.InsertIntoDirectory(this.Image.Resources);
		}
		public void SetAssemblyInfo(string description, string company, string title, string product, string copyright, string trademark, string version) {
			// 创建新的版本资源。
			var versionResource = new VersionInfoResource();
			if (string.IsNullOrEmpty(version)) version = "0.0.0.0";

			var TypedVersion = new System.Version(version);
			version = TypedVersion.ToString();

			// 添加信息。
			var fixedVersionInfo = new FixedVersionInfo {
				FileVersion = TypedVersion,
				ProductVersion = TypedVersion,
				FileDate = (ulong)DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
				FileType = FileType.App,
				FileOS = FileOS.Windows32,
				FileSubType = FileSubType.DriverInstallable,
			};
			versionResource.FixedVersionInfo = fixedVersionInfo;

			// 添加字符串。
			var stringFileInfo = new StringFileInfo();
			var stringTable = new StringTable(0, 0x4b0){
				{ StringTable.ProductNameKey, product },
				{ StringTable.FileVersionKey, version },
				{ StringTable.ProductVersionKey, version },
				{ StringTable.FileDescriptionKey, title },
				{ StringTable.CommentsKey, description },
				{ StringTable.LegalCopyrightKey, copyright }
			};

			stringFileInfo.Tables.Add(stringTable);
			versionResource.AddEntry(stringFileInfo);

			// 注册翻译。
			var varFileInfo = new VarFileInfo();
			var varTable = new VarTable();
			varTable.Values.Add(0x4b00000);
			varFileInfo.Tables.Add(varTable);
			versionResource.AddEntry(varFileInfo);

			// 添加到资源。
			if (this.Image.Resources == null) this.Image.Resources = new ResourceDirectory(0u);
			versionResource.InsertIntoDirectory(this.Image.Resources);
		}
	}

	internal class DummyProvider: ITypeCodedIndexProvider {
		public uint GetTypeDefOrRefIndex(ITypeDefOrRef type, object extraArgument) {
			throw new NotImplementedException();
		}
	}

	/// <summary>
	/// 最小 PE：FileAlignment 512，SectionAlignment 4096（加载器要求 4K 段对齐），仅 .text，最小数据目录。可达到 1024 字节。输出字符串传给 builder 并布局在 .text 内，以便 CIL patch 获得正确的 RVA。总计 512 字节不可能（PE 头 + 一个段 >= 1024）。
	/// </summary>
	internal sealed class MinimalPEFileBuilder : ManagedPEFileBuilder {
		private readonly ISegment _extraSectionData;
		private readonly ISegment _writableData;

		public MinimalPEFileBuilder(ISegment extraSectionData = null, ISegment writableData = null) {
			_extraSectionData = extraSectionData;
			_writableData = writableData;
		}

		protected override uint GetFileAlignment(PEFileBuilderContext context, PEFile outputFile) { return 512; }
		// 段对齐 4096 供加载器使用；文件对齐 512 以减小体积。
		protected override uint GetSectionAlignment(PEFileBuilderContext context, PEFile outputFile) { return 4096; }

		protected override IEnumerable<PESection> CreateSections(PEFileBuilderContext context) {
			yield return CreateTextSection(context);
			// 可写数据（如 GetModuleFileNameW 路径缓冲区）放独立 .data 段，避免 .text 带写权限
			if (_writableData != null) {
				yield return new PESection(".data",
					SectionFlags.ContentUninitializedData | SectionFlags.MemoryRead | SectionFlags.MemoryWrite,
					_writableData);
			}
			// 有 Win32 资源（如 icon、版本信息）时添加 .rsrc 段
			if (context.Image.Resources != null) {
				yield return new PESection(".rsrc",
					SectionFlags.ContentInitializedData | SectionFlags.MemoryRead,
					context.ResourceDirectory);
			}
		}

		protected override void AssignDataDirectories(PEFileBuilderContext context, PEFile outputFile) {
			base.AssignDataDirectories(context, outputFile);
			outputFile.OptionalHeader.SetDataDirectory(DataDirectoryIndex.BaseRelocationDirectory, (ISegment)null);
			outputFile.OptionalHeader.SetDataDirectory(DataDirectoryIndex.DebugDirectory, (ISegment)null);
			if (context.Image.Resources == null)
				outputFile.OptionalHeader.SetDataDirectory(DataDirectoryIndex.ResourceDirectory, (ISegment)null);
			outputFile.OptionalHeader.SetDataDirectory(DataDirectoryIndex.ExportDirectory, (ISegment)null);
		}

		protected override PESection CreateTextSection(PEFileBuilderContext context) {
			var contents = new SegmentBuilder();
			if (!context.ImportDirectory.IsEmpty) {
				contents.Add(context.ImportDirectory.ImportAddressDirectory);
			}
			var dotNet = context.Image.DotNetDirectory;
			if (dotNet != null) {
				contents.Add(dotNet);
				contents.Add(context.FieldRvaTable);
				contents.Add(context.MethodBodyTable);
				if (dotNet.Metadata != null) contents.Add(dotNet.Metadata, 4);
				if (dotNet.DotNetResources != null) contents.Add(dotNet.DotNetResources, 4);
				if (dotNet.StrongName != null) contents.Add(dotNet.StrongName, 4);
				if (dotNet.VTableFixups != null && dotNet.VTableFixups.Count > 0) contents.Add(dotNet.VTableFixups);
				if (dotNet.ExportAddressTable != null) contents.Add(dotNet.ExportAddressTable, 4);
				if (dotNet.ManagedNativeHeader != null) contents.Add(dotNet.ManagedNativeHeader, 4);
			}
			if (!context.ImportDirectory.IsEmpty) {
				contents.Add(context.ImportDirectory);
			}
			if (_extraSectionData != null)
				contents.Add(_extraSectionData);
			return new PESection(".text",
				SectionFlags.ContentCode | SectionFlags.MemoryExecute | SectionFlags.MemoryRead,
				contents);
		}
	}
}
