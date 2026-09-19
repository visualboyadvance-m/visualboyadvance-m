using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;

// 单文件自宿主的 Core launcher 没有 pwsh 的启动流程，这里在模块初始化时把 PowerShell 引擎与模块目录接上：探测本机 $PSHOME、接上 PSModulePath、挂 AssemblyResolve，好让内存里的 payload 找到没打包的 SMA 与内置模块。
internal static class PS12ExeCoreHost {
	const string PshomeMissingMessage = "PowerShell Core (pwsh) not found. Install PowerShell 7, or add it to PATH / set the PSHOME environment variable.";
	static string pshome;

	[ModuleInitializer]
	internal static void Init() {
		pshome = DetectPshome();
		if (pshome == null) {
			#if noConsole
				System.Windows.Forms.MessageBox.Show(PshomeMissingMessage, ":(");
			#else
				Console.Error.WriteLine(PshomeMissingMessage);
			#endif
			Environment.Exit(1);
			return;
		}
		string modules = Path.Combine(pshome, "Modules");
		string existing = Environment.GetEnvironmentVariable("PSModulePath");
		Environment.SetEnvironmentVariable("PSModulePath", string.IsNullOrEmpty(existing) ? modules : modules + ";" + existing);
		AppDomain.CurrentDomain.AssemblyResolve += ResolveFromPshome;
	}

	static Assembly ResolveFromPshome(object sender, ResolveEventArgs args) {
		string candidate = Path.Combine(pshome, new AssemblyName(args.Name).Name + ".dll");
		return File.Exists(candidate) ? Assembly.LoadFrom(candidate) : null;
	}

	static string DetectPshome() {
		List<string> candidates = new List<string>();
		string fromEnv = Environment.GetEnvironmentVariable("PSHOME");
		if (!string.IsNullOrEmpty(fromEnv)) candidates.Add(fromEnv);
		string pathEnv = Environment.GetEnvironmentVariable("PATH");
		if (!string.IsNullOrEmpty(pathEnv)) {
			string pwsh = OperatingSystem.IsWindows() ? "pwsh.exe" : "pwsh";
			foreach (string dir in pathEnv.Split(Path.PathSeparator)) {
				if (!string.IsNullOrWhiteSpace(dir) && File.Exists(Path.Combine(dir, pwsh))) candidates.Add(dir);
			}
		}
		candidates.AddRange(KnownPshomePaths());
		foreach (string candidate in candidates) {
			if (File.Exists(Path.Combine(candidate, "System.Management.Automation.dll"))) return candidate;
		}
		return null;
	}

	static IEnumerable<string> KnownPshomePaths() {
		if (OperatingSystem.IsWindows()) {
			string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
			yield return Path.Combine(programFiles, "PowerShell", "7");
			yield return Path.Combine(programFiles, "PowerShell", "7-preview");
			yield return Path.Combine(programFiles, "PowerShell", "6");
		} else {
			yield return "/opt/microsoft/powershell/7";
			yield return "/opt/microsoft/powershell/7-preview";
			yield return "/usr/local/microsoft/powershell/7";
		}
	}
}
