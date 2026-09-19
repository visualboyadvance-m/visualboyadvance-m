# 常量输出上限：以 TinySharp 实际内嵌字节数为预算（ASCII 1x / UTF-16 2x）；原文超预算时，若 TinySharp 会对负载做 XPRESS 压缩且压缩后能落回预算内，则放行——大而可压缩的常量输出因此也能用 ~1KB 的壳。仅 TinySharp 可用时适用（Core / requireAdmin 直接回退到普通编译）。
#
# 预算 = 「非 const hello world」与「const hello world」的体积差，按目标架构 / 是否 noConsole 查表（Framework4.0 实测；Framework2.0 同值）。含义：常量壳一旦大过这个差值，就不比普通编译更小了。
#   console   anycpu/x86 14848-1024=13824   x64 13824-1024=12800
#   noConsole anycpu/x86 18432-1536=16896   x64 17920-1536=16384
$ConstBudgetTable = @{
	'console_anycpu'   = 13824
	'console_x86'      = 13824
	'console_x64'      = 12800
	'noConsole_anycpu' = 16896
	'noConsole_x86'    = 16896
	'noConsole_x64'    = 16384
}
# 压缩壳比未压缩壳多一个 512B 文件块（cabinet 解压 P/Invoke + 解压 CIL），须与 TinySharp.cs 的 CompressionOverhead 一致
$ConstCompressedOverhead = 512

# 注意：预算值为 TinySharp 实测量；requireAdmin/Core 实际走 constexpr.cs（内嵌转义的 $ConstResult），无法用 TinySharp 壳，这里仅用同一预算做保守门槛，超出即回退普通编译。
function Test-ConstResultTooLong([string]$Output) {
	$mode = if ($noConsole) { 'noConsole' } else { 'console' }
	$archKey = if ($architecture -in 'x86', 'x64') { $architecture } else { 'anycpu' }
	$rawBudget = $ConstBudgetTable["${mode}_${archKey}"]

	# MessageBox(noConsole) 恒用 UTF-16；控制台按 ASCII/UTF-16 自适应（与 TinySharp.cs 保持一致）
	$payloadIsAscii = (-not $noConsole) -and (-not ($Output -match '[^\x00-\x7F]'))
	$isNonAsciiConsole = (-not $noConsole) -and ($Output -match '[^\x00-\x7F]')
	if ($payloadIsAscii) {
		$payload = [Text.Encoding]::ASCII.GetBytes($Output + [char]0)
	}
	else {
		$payload = [Text.Encoding]::Unicode.GetBytes($Output + [char]0)
	}
	# 预算衡量的是 TinySharp 实际内嵌的字节数（未压缩原文，或压缩后）
	if ($payload.Length -le $rawBudget) { return $false }
	if ($requireAdmin -or $isCoreTarget) { return $true }
	# 非 ASCII 控制台常量壳不做 XPRESS 压缩（见 TinySharp.CompileUnicode），预算按未压缩原文衡量。
	if ($isNonAsciiConsole) { return $true }
	$estimated = Get-XpressCompressedSize $payload
	return (($estimated -lt 0) -or (($estimated + $ConstCompressedOverhead) -gt $rawBudget))
}

# 调用 cabinet.dll 的 XPRESS 压缩，返回压缩后字节数；API 不可用或失败返回 -1。
function Get-XpressCompressedSize([byte[]]$Bytes) {
	try {
		if (-not ('ps12exeConstCompressor' -as [type])) {
			Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class ps12exeConstCompressor {
	[DllImport("cabinet.dll", SetLastError=true)] private static extern bool CreateCompressor(uint a, IntPtr al, out IntPtr c);
	[DllImport("cabinet.dll", SetLastError=true)] private static extern bool Compress(IntPtr c, byte[] i, IntPtr il, byte[] o, IntPtr ol, out IntPtr r);
	[DllImport("cabinet.dll", SetLastError=true)] private static extern bool CloseCompressor(IntPtr c);
	public static int CompressedSize(byte[] input) {
		IntPtr h;
		if (!CreateCompressor(3u, IntPtr.Zero, out h)) return -1;
		try {
			byte[] buf = new byte[input.Length + input.Length / 2 + 1024];
			IntPtr r;
			if (!Compress(h, input, (IntPtr)input.Length, buf, (IntPtr)buf.Length, out r)) return -1;
			return (int)r;
		} finally { CloseCompressor(h); }
	}
}
"@ *> $null
		}
		return [ps12exeConstCompressor]::CompressedSize($Bytes)
	}
	catch { return -1 }
}

if ($AstAnalyzeResult.IsConst) {
	$timeoutSeconds = 7  # 设置超时限制（秒）

	#   #_pragma Build.ConstEval.Enabled 0   显式声明本脚本不是常量，直接跳过常量求值
	#   #_pragma Build.ConstEval.Timeout 1   显式声明本次常量求值已超时，按超时回退处理
	# 由 ps12exe.ps1 的适配层（Build.ConstEval）解析后以 $noConstEval / $constEvalTimeout 传入。
	$NoConstEvalPragma = $noConstEval
	$ConstEvalTimeoutPragma = $constEvalTimeout

	if ($NoConstEvalPragma) {
		Write-I18n Verbose ConstEvalNotConstFallback
		$AstAnalyzeResult.IsConst = $false
	}
	else {
		Write-I18n Verbose ConstEvalStart

		# 一个自定义host以便挂钩SetShouldExit
		Add-Type @"
using System;
using System.Globalization;
using System.Management.Automation;
using System.Management.Automation.Host;

public class ps12exeConstEvalHost : PSHost {
    public static int LastExitCode = 0;
    public override void SetShouldExit(int exitCode) {
        LastExitCode = exitCode;
    }
    public override PSHostUserInterface UI { get { return null; } }
    public override string Name { get { return "ps12exeConstEvalHost"; } }
    public override Version Version { get { return new Version("72.7"); } }
    public override Guid InstanceId { get { return Guid.NewGuid(); } }
    public override CultureInfo CurrentCulture { get { return new CultureInfo(72); } }
    public override CultureInfo CurrentUICulture { get { return new CultureInfo(72); } }
    public override void EnterNestedPrompt() { }
    public override void ExitNestedPrompt() { }
    public override void NotifyBeginApplication() { }
    public override void NotifyEndApplication() { }
}
"@ *> $null
		$myhost = [ps12exeConstEvalHost]::New()
		$runspace = [runspacefactory]::CreateRunspace($myhost)
		$runspace.Open()
		$pwsh = [System.Management.Automation.PowerShell]::Create()
		$pwsh.Runspace = $runspace
		$runspace.SessionStateProxy.SetVariable("PSEXEScript", $Content)

		$ConstEvalDone = $false
		if ($ConstEvalTimeoutPragma) {
			# 已声明超时：不实际求值，直接走与真实超时相同的回退路径
			Write-I18n Verbose ConstEvalTimeoutFallback $timeoutSeconds
		}
		else {
			$null = $pwsh.AddScript("function PSEXEMainFunction{$Content};PSEXEMainFunction")

			$asyncResult = $pwsh.BeginInvoke()

			$timeoutTicks = [int]($timeoutSeconds * 20)
			for ($i = 0; $i -lt $timeoutTicks; $i++) {
				if ($asyncResult.IsCompleted) {
					break
				}
				Start-Sleep -Milliseconds 50
			}

			if ($asyncResult.IsCompleted) {
				try {
					$RowResult = $pwsh.EndInvoke($asyncResult) | Where-Object { $_ -ne $null }
					# TinySharp 实际内嵌的负载（各行按换行拼接，与 TinySharpCompiler.ps1 保持一致）
					$ConstOutput = $RowResult | ForEach-Object { (($_ | Out-String) -replace '\r\n$', '') }
					$ConstOutput = $ConstOutput -join "`n"
					$ConstResult = $RowResult | ForEach-Object {
						(($_ | Out-String) -replace '\r\n$', '').Replace('\', '\\').Replace('"', '\"').Replace("`n", "\n").Replace("`r", "\r")
					}
					$ConstResult = $ConstResult -join $(if ($noConsole) { '","' }else { "`n" })
					Write-I18n Verbose ConstEvalDone $(bytesOfString $ConstResult)
					if (Test-ConstResultTooLong $ConstOutput) {
						Write-I18n Verbose ConstEvalTooLongFallback
					}
					else {
						#_if PSEXE #这是该脚本被ps12exe编译时使用的预处理代码
							#_include_as_value programFrame "$PSScriptRoot/programFrames/constexpr.cs" #将constexpr.cs中的内容内嵌到该脚本中
						#_else #否则正常读取cs文件
							[string]$programFrame = Get-Content $PSScriptRoot/programFrames/constexpr.cs -Raw -Encoding UTF8
						#_endif
						$programFrame = $programFrame.Replace("`$ConstResult", $ConstResult)
						$programFrame = $programFrame.Replace("`$ConstExitCodeResult", [ps12exeConstEvalHost]::LastExitCode)
						if ($RowResult.Count -eq 0) {
							$noOutput = $true
						}
						$ConstEvalDone = $true
					}
				}
				catch {
					Write-I18n Verbose ConstEvalThrowErrorFallback
				}
			}
			else {
				Write-I18n Verbose ConstEvalTimeoutFallback $timeoutSeconds
				$pwsh.Stop()
			}
		}

		# 所有回退路径（超时/超长/异常/显式声明）都汇聚到这一处，避免遗漏 IsConst 复位
		if (-not $ConstEvalDone) {
			$AstAnalyzeResult.IsConst = $false
		}

		$runspace.Close()
		$pwsh.Dispose()
		$runspace.Dispose()
	}
}
