$type = ('System.Collections.Generic.Dictionary`2') -as "Type"
$type = $type.MakeGenericType(@([String], [String]) )
$o = [Activator]::CreateInstance($type)
if ($isPwsh20Sma) {
	$o.Add("CompilerVersion", "v3.5")
}
else { $o.Add("CompilerVersion", "v4.0") }

$cop = (New-Object Microsoft.CSharp.CSharpCodeProvider($o))
[string[]]$BaseCompilerOptions = @($CompilerOptions)

$manifestParam = if (($AstAnalyzeResult.IsConst -or $virtualize) -and -not $requireAdmin) {
	"/nowin32manifest"
}
elseif ($requireAdmin -or $DPIAware -or $supportOS -or $longPaths) {
	"`"/win32manifest:$($outputFile+".win32manifest")`""
	@"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
$(if ($DPIAware -or $longPaths) {@"
	<application xmlns="urn:schemas-microsoft-com:asm.v3">
		<windowsSettings>
	$(if ($DPIAware) {@"
			<dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
			<dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
"@})$(if ($longPaths) {@"
			<longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
"@})
		</windowsSettings>
	</application>
"@})$(if ($requireAdmin) {@"
	<trustInfo xmlns="urn:schemas-microsoft-com:asm.v2">
		<security>
			<requestedPrivileges xmlns="urn:schemas-microsoft-com:asm.v3">
				<requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>
			</requestedPrivileges>
		</security>
	</trustInfo>
"@})$(if ($supportOS) {@"
	<compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
		<application>
			<supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
			<supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>
			<supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}"/>
			<supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}"/>
			<supportedOS Id="{e2011457-1546-43c5-a5fe-008deee3d3f0}"/>
		</application>
	</compatibility>
"@})
</assembly>
"@ | Set-Content ($outputFile + ".win32manifest") -Encoding UTF8
}

[string[]]$CompilerOptions = $BaseCompilerOptions

if ($virtualize) {
	Write-I18n Host ForceX86byVirtualization
	$architecture = "x86"
}
$CompilerOptions += "/platform:$architecture"
$CompilerOptions += "/target:$( if ($noConsole){'winexe'}else{'exe'})"
$CompilerOptions += $manifestParam

$configFileForEXE3 = @"
<?xml version="1.0" encoding="utf-8" ?>
<configuration>
	<startup>
		$(if ($winFormsDPIAware) {'<supportedRuntime version="v4.0" sku=".NETFramework,Version=v4.7"/>'}
		else {'<supportedRuntime version="v4.0" sku=".NETFramework,Version=v4.0"/>'})
	</startup>
$(if ($longPaths) {@'
	<runtime>
		<AppContextSwitchOverrides value="Switch.System.IO.UseLegacyPathHandling=false;Switch.System.IO.BlockLongPaths=false"/>
	</runtime>
'@})$(
	if ($winFormsDPIAware) {@'
	<System.Windows.Forms.ApplicationConfigurationSection>
		<add key="DpiAwareness" value="PerMonitorV2"/>
	</System.Windows.Forms.ApplicationConfigurationSection>
'@})
</configuration>
"@

if ($iconFile) {
	$CompilerOptions += "`"/win32icon:$iconFile`""
}

$CompilerOptions += "/define:$($Constants -join ';')"

function New-PS12ExeCompilerParameters([string]$outFile, [string[]]$opts, [bool]$debug) {
	$p = New-Object System.CodeDom.Compiler.CompilerParameters($referenceAssembies, $outFile)
	$p.GenerateInMemory = $FALSE
	$p.GenerateExecutable = $TRUE
	$p.IncludeDebugInformation = $debug
	$p.CompilerOptions = ($opts -ne '') -join ' '
	$p.TempFiles = New-Object System.CodeDom.Compiler.TempFileCollection($TempDir)
	if ($debug) { $p.TempFiles.KeepFiles = $TRUE }
	Write-Debug "Using Compiler Options: $($p.CompilerOptions)"
	return $p
}

# 默认路径：先编出普通托管程序集作为负载，gzip 后塞进一个极小的 launcher 里。launcher 启动时在内存中解压并用 Assembly.Load 载入负载，因此负载不会落到磁盘。仅当无法打包时才退化为普通编译（Build.KeepSource 需要负载源码/PDB、Build.DllExports、真实 PS2 SMA）。常量脚本的 constexpr.cs 入口是无参 Main()，与 pack launcher 的 Main(string[]) 调用约定不符，故不走 pack。
$packEnabled = (
	-not $prepareDebug -and
	-not $isPwsh20Sma -and
	-not $DllExportList -and
	-not $AstAnalyzeResult.IsConst -and
	$TempDir
)

if ($packEnabled) {
	$payloadPath = Join-Path $TempDir 'PS12ExePayload.exe'
	$payloadOptions = @($BaseCompilerOptions) + @(
		"/platform:$architecture",
		"/target:exe",
		"/nowin32manifest",
		"/define:$($Constants -join ';')"
	)
	$pcp = New-PS12ExeCompilerParameters $payloadPath $payloadOptions $FALSE
	if (!$AstAnalyzeResult.IsConst) {
		[VOID]$pcp.EmbeddedResources.Add("$TempDir\main.ps1")
	}
	$pcr = $cop.CompileAssemblyFromSource($pcp, $programFrame)
	if ($pcr.Errors.Count -gt 0) {
		throw $pcr.Errors -join "`n"
	}

	# csc 默认会塞进 manifest/版本信息资源；负载用不到这些，先剥掉再压缩省一点。
	$exeSinker = Join-Path $PSScriptRoot 'ExeSinker.ps1'
	if (Test-Path $exeSinker) {
		& $exeSinker $payloadPath -removeResources
	}

	$gzPath = Join-Path $TempDir 'main'
	[byte[]]$payloadBytes = [System.IO.File]::ReadAllBytes($payloadPath)
	$fileStream = [System.IO.File]::Create($gzPath)
	try {
		$gzip = New-Object System.IO.Compression.GZipStream($fileStream, [System.IO.Compression.CompressionMode]::Compress)
		try {
			$gzip.Write($payloadBytes, 0, $payloadBytes.Length)
		}
		finally {
			$gzip.Dispose()
		}
	}
	finally {
		$fileStream.Dispose()
	}

	# 和 default.cs 一样：编译进 ps12exe.exe 时内嵌 pack.cs，脚本模式从磁盘读取。
	#_if PSEXE
		#_include_as_value launcherSource "$PSScriptRoot/programFrames/pack.cs"
	#_else
		[string]$launcherSource = Get-Content $PSScriptRoot/programFrames/pack.cs -Raw -Encoding UTF8
	#_endif
	# 资源参数走 #if + $placeholder 替换，pack.cs 自身保持纯 C#。
	$launcherSource = $launcherSource.Replace("`$TargetFramework", $TargetFramework)
	$resourceParamKeys | ForEach-Object {
		$launcherSource = $launcherSource.Replace("`$$_", $resourceParams[$_])
	}

	[string[]]$LauncherCompilerOptions = $CompilerOptions
	if (-not $manifestParam) {
		# 没有自定义清单需求时，launcher 也不需要默认清单。
		$LauncherCompilerOptions += "/nowin32manifest"
	}
	$lcp = New-PS12ExeCompilerParameters $outputFile $LauncherCompilerOptions $FALSE
	[VOID]$lcp.EmbeddedResources.Add($gzPath)
	$cr = $cop.CompileAssemblyFromSource($lcp, $launcherSource)
	if ($cr.Errors.Count -gt 0) {
		throw $cr.Errors -join "`n"
	}
}
else {
	$cp = New-PS12ExeCompilerParameters $outputFile $CompilerOptions $prepareDebug
	if (!$AstAnalyzeResult.IsConst) {
		[VOID]$cp.EmbeddedResources.Add("$TempDir\main.ps1")
	}
	$cr = $cop.CompileAssemblyFromSource($cp, $programFrame)
	if ($cr.Errors.Count -gt 0) {
		throw $cr.Errors -join "`n"
	}
}

if (
	#_if PSEXE
		#_!! $AstAnalyzeResult.IsConst -or
	#_endif
	$requireAdmin -or $DPIAware -or $supportOS -or $longPaths
) {
	if (Test-Path $($outputFile + ".win32manifest")) {
		Remove-Item $($outputFile + ".win32manifest") -Verbose:$FALSE
	}
}
