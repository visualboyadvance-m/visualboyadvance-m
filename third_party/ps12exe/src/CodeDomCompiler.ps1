$type = ('System.Collections.Generic.Dictionary`2') -as "Type"
$type = $type.MakeGenericType(@([String], [String]) )
$o = [Activator]::CreateInstance($type)
if ($targetRuntime -eq 'Framework2.0') {
	$o.Add("CompilerVersion", "v3.5")
}
else { $o.Add("CompilerVersion", "v4.0") }

$cop = (New-Object Microsoft.CSharp.CSharpCodeProvider($o))
$cp = New-Object System.CodeDom.Compiler.CompilerParameters($referenceAssembies, $outputFile)
$cp.GenerateInMemory = $FALSE
$cp.GenerateExecutable = $TRUE

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

[string[]]$CompilerOptions = @($CompilerOptions)

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

$cp.TempFiles = New-Object System.CodeDom.Compiler.TempFileCollection($TempDir)

if ($iconFile) {
	$CompilerOptions += "`"/win32icon:$iconFile`""
}

$cp.IncludeDebugInformation = $prepareDebug

if ($prepareDebug) {
	$cp.TempFiles.KeepFiles = $TRUE
}

$CompilerOptions += "/define:$($Constants -join ';')"
$cp.CompilerOptions = $CompilerOptions -ne '' -join ' '
Write-Debug "Using Compiler Options: $($cp.CompilerOptions)"

if (!$AstAnalyzeResult.IsConst) {
	[VOID]$cp.EmbeddedResources.Add("$TempDir\main.ps1")
}
$cr = $cop.CompileAssemblyFromSource($cp, $programFrame)
if ($cr.Errors.Count -gt 0) {
	throw $cr.Errors -join "`n"
}

if (
#_if PSEXE
	#_!! $AstAnalyzeResult.IsConst -or
#_endif
$requireAdmin -or $DPIAware -or $supportOS -or $longPaths) {
	if (Test-Path $($outputFile + ".win32manifest")) {
		Remove-Item $($outputFile + ".win32manifest") -Verbose:$FALSE
	}
}
