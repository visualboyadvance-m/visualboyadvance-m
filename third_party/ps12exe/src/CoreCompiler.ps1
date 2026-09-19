# PowerShell Core / .NET 编译路径。非常量：把程序框架编成托管负载程序集（payload.dll，内含 main.ps1 资源），Brotli 压缩成 "main" 资源塞进 launcher（pack.cs + CoreHost.cs 引导），dotnet publish 成单文件 exe；SMA 不打包，运行时由 CoreHost 从 $PSHOME 解析。常量：constexpr.cs 自包含、不引用 SMA，直接 publish 成单文件 exe，不走 payload/launcher。产物是框架依赖：目标机器需要有 PowerShell Core（提供引擎与模块）以及匹配的 .NET 运行时。

# 只有 pwsh 宿主才能正确推导 Core 的目标框架、$PSHOME 与 RID；Windows PowerShell 下应先交接给 pwsh。
if ($PSVersionTable.PSEdition -ne 'Core') {
	Write-I18n Error CoreCompileNeedPwshHost -Category InvalidOperation
	throw 'ps12exe:core-host'
}

$dotnet = Get-Command dotnet -ErrorAction Ignore
if (-not $dotnet) {
	Write-I18n Error CoreCompileNeedDotnet -Category NotInstalled
	throw 'ps12exe:core-need-dotnet'
}

$unsupported = @()
foreach ($a in @('requireAdmin', 'DPIAware', 'supportOS', 'longPaths', 'virtualize', 'winFormsDPIAware')) {
	if ((Get-Variable -Name $a -ValueOnly -ErrorAction Ignore)) { $unsupported += $a }
}
if ($DllExportList) { $unsupported += 'Build.DllExports' }
if ($unsupported.Count) {
	Write-I18n Error CoreCompileUnsupported ($unsupported -join ', ') -Category InvalidArgument
	throw 'ps12exe:core-unsupported'
}

# 目标框架版本跟随当前 PowerShell 所用的 .NET 运行时。
$runtimeVersion = [System.Environment]::Version
$tfm = "net$($runtimeVersion.Major).$($runtimeVersion.Minor)"
if ($noConsole) { $tfm += '-windows' }

$ridArch = $architecture
if ($ridArch -eq 'anycpu') {
	$ridArch = switch ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
		'X64' { 'x64' }
		'X86' { 'x86' }
		'Arm64' { 'arm64' }
		default { 'x64' }
	}
}
$ridOs = if ($IsWindows) { 'win' } elseif ($IsMacOS) { 'osx' } else { 'linux' }
if ($ridOs -ne 'win' -and $ridArch -eq 'x86') { $ridArch = 'x64' }
$rid = "$ridOs-$ridArch"

$assemblyName = [System.IO.Path]::GetFileNameWithoutExtension($outputFile)
$assemblyName = ($assemblyName -replace '[^\w\.\-]', '_')
if (-not $assemblyName) { $assemblyName = 'PS12ExeOutput' }

$isConst = $AstAnalyzeResult.IsConst
$outputType = if ($noConsole) { 'WinExe' } else { 'Exe' }
$debugType = if ($prepareDebug) { 'portable' } else { 'none' }
$iconElement = if ($iconFile) { "<ApplicationIcon>$([System.Security.SecurityElement]::Escape($iconFile))</ApplicationIcon>" } else { '' }
$winForms = if ($noConsole) { '<UseWindowsForms>true</UseWindowsForms>' } else { '' }

# 资源/版本元数据由 SDK 生成，因此 DefineConstants 里剔除 Resources/version。
$coreConstants = @($Constants | Where-Object { $_ -and $_ -ne 'Resources' -and $_ -ne 'version' })
$defineConstants = ($coreConstants | Sort-Object -Unique) -join ';'

$resourceMap = @{
	title       = 'AssemblyTitle'
	description = 'Description'
	company     = 'Company'
	product     = 'Product'
	copyright   = 'Copyright'
	trademark   = 'Trademark'
}
$resourceElements = ($resourceMap.Keys | Where-Object { $resourceParams.ContainsKey($_) -and $resourceParams[$_] } | ForEach-Object {
	"<$($resourceMap[$_])>$([System.Security.SecurityElement]::Escape($resourceParams[$_]))</$($resourceMap[$_])>"
}) -join "`n`t`t"
$versionElements = if ($resourceParams.version) {
	$v = [System.Security.SecurityElement]::Escape($resourceParams.version)
	"<Version>$v</Version>`n`t`t<FileVersion>$v</FileVersion>`n`t`t<AssemblyVersion>$v</AssemblyVersion>"
}
else { '' }

# 常量帧与 launcher 两个发布项目共用的属性；DefineConstants 由调用处替换。
$publishedProps = @"
		<OutputType>$outputType</OutputType>
		<TargetFramework>$tfm</TargetFramework>
		<RuntimeIdentifier>$rid</RuntimeIdentifier>
		<SelfContained>false</SelfContained>
		<PublishSingleFile>true</PublishSingleFile>
		<NoWarn>`$(NoWarn);CA1416;IL3000;CS8073</NoWarn>
		<EnableDefaultCompileItems>false</EnableDefaultCompileItems>
		<AssemblyName>$assemblyName</AssemblyName>
		<Nullable>disable</Nullable>
		<ImplicitUsings>disable</ImplicitUsings>
		<DebugType>$debugType</DebugType>
		<GenerateDocumentationFile>false</GenerateDocumentationFile>
		<SatelliteResourceLanguages>en</SatelliteResourceLanguages>
		<AllowUnsafeBlocks>true</AllowUnsafeBlocks>
		<EnableWindowsTargeting>true</EnableWindowsTargeting>
		<DefineConstants>__DefineConstants__</DefineConstants>
		$winForms
		$iconElement
		$resourceElements
		$versionElements
"@

$projectDir = Join-Path $TempDir 'coreproj'
Remove-Item -LiteralPath $projectDir -Recurse -Force -ErrorAction Ignore
New-Item -ItemType Directory -Path $projectDir -Force | Out-Null

$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:DOTNET_NOLOGO = '1'

$publishDir = Join-Path $projectDir 'publish'

if ($isConst) {
	# ---------- 常量：constexpr.cs 直接 publish ----------
	[System.IO.File]::WriteAllText((Join-Path $projectDir 'frame.cs'), $programFrame, [System.Text.UTF8Encoding]::new($false))
	$constCsproj = @"
<Project Sdk="Microsoft.NET.Sdk">
	<PropertyGroup>
$($publishedProps.Replace('__DefineConstants__', [System.Security.SecurityElement]::Escape($defineConstants)))
	</PropertyGroup>
	<ItemGroup>
		<Compile Include="frame.cs" />
	</ItemGroup>
</Project>
"@
	[System.IO.File]::WriteAllText((Join-Path $projectDir 'const.csproj'), $constCsproj, [System.Text.UTF8Encoding]::new($false))
	Write-I18n Host CoreCompilePublishing
	Write-Debug "Core compiler: dotnet publish const frame (tfm=$tfm, rid=$rid)"
	$publishOutput = & $dotnet.Source publish (Join-Path $projectDir 'const.csproj') -c Release -o $publishDir --nologo -v quiet 2>&1
	if ($LASTEXITCODE -ne 0) {
		throw ($publishOutput -join "`n")
	}
}
else {
	# ---------- 非常量：payload 程序集 → Brotli → launcher ----------
	$payloadDir = Join-Path $projectDir 'payload'
	New-Item -ItemType Directory -Path $payloadDir -Force | Out-Null
	[System.IO.File]::WriteAllText((Join-Path $payloadDir 'frame.cs'), $programFrame, [System.Text.UTF8Encoding]::new($false))
	Copy-Item -LiteralPath (Join-Path $TempDir 'main.ps1') -Destination (Join-Path $payloadDir 'main.ps1') -Force

	$payloadDefineConstants = (($coreConstants + 'CoreHost') | Sort-Object -Unique) -join ';'
	$payloadCsproj = @"
<Project Sdk="Microsoft.NET.Sdk">
	<PropertyGroup>
		<OutputType>Exe</OutputType>
		<TargetFramework>$tfm</TargetFramework>
		<RuntimeIdentifier>$rid</RuntimeIdentifier>
		<SelfContained>false</SelfContained>
		<UseAppHost>false</UseAppHost>
		<NoWarn>`$(NoWarn);CA1416;IL3000;CS8073</NoWarn>
		<EnableDefaultCompileItems>false</EnableDefaultCompileItems>
		<AssemblyName>$assemblyName</AssemblyName>
		<Nullable>disable</Nullable>
		<ImplicitUsings>disable</ImplicitUsings>
		<DebugType>none</DebugType>
		<GenerateDocumentationFile>false</GenerateDocumentationFile>
		<SatelliteResourceLanguages>en</SatelliteResourceLanguages>
		<AllowUnsafeBlocks>true</AllowUnsafeBlocks>
		<EnableWindowsTargeting>true</EnableWindowsTargeting>
		<DefineConstants>$([System.Security.SecurityElement]::Escape($payloadDefineConstants))</DefineConstants>
		$winForms
	</PropertyGroup>
	<ItemGroup>
		<Compile Include="frame.cs" />
		<EmbeddedResource Include="main.ps1" LogicalName="main.ps1" />
		<Reference Include="System.Management.Automation">
			<HintPath>$([System.Security.SecurityElement]::Escape((Join-Path $PSHOME 'System.Management.Automation.dll')))</HintPath>
			<Private>false</Private>
		</Reference>
	</ItemGroup>
</Project>
"@
	[System.IO.File]::WriteAllText((Join-Path $payloadDir 'payload.csproj'), $payloadCsproj, [System.Text.UTF8Encoding]::new($false))

	$payloadOut = Join-Path $projectDir 'payloadout'
	Write-Debug "Core compiler: dotnet build payload (tfm=$tfm, rid=$rid)"
	$buildOutput = & $dotnet.Source build (Join-Path $payloadDir 'payload.csproj') -c Release -o $payloadOut --nologo -v quiet 2>&1
	if ($LASTEXITCODE -ne 0) {
		throw ($buildOutput -join "`n")
	}
	$payloadDll = Join-Path $payloadOut "$assemblyName.dll"
	if (-not (Test-Path -LiteralPath $payloadDll)) {
		throw "ps12exe: core payload not built: $payloadDll`n$($buildOutput -join "`n")"
	}

	# 负载整体 Brotli 压缩成 launcher 的 "main" 资源（Core 的 launcher 走 pack.cs 的 Brotli 分支）。
	$mainPath = Join-Path $projectDir 'main'
	$inStream = [System.IO.File]::OpenRead($payloadDll)
	$outStream = [System.IO.File]::Create($mainPath)
	$brotli = [System.IO.Compression.BrotliStream]::new($outStream, [System.IO.Compression.CompressionLevel]::SmallestSize, $true)
	try {
		$inStream.CopyTo($brotli)
	}
	finally {
		$brotli.Dispose(); $outStream.Dispose(); $inStream.Dispose()
	}

	# CoreHost.cs 是 launcher 侧引导：探测 $PSHOME、接 PSModulePath、挂 AssemblyResolve。和 pack.cs 一样，编译进 ps12exe.exe 时内嵌，脚本模式从磁盘读取。
	#_if PSEXE
		#_include_as_value bootstrapSource "$PSScriptRoot/programFrames/CoreHost.cs"
	#_else
		[string]$bootstrapSource = Get-Content $PSScriptRoot/programFrames/CoreHost.cs -Raw -Encoding UTF8
	#_endif
	[System.IO.File]::WriteAllText((Join-Path $projectDir 'CoreHost.cs'), $bootstrapSource, [System.Text.UTF8Encoding]::new($false))

	# 复用 WinPS pack 用的 pack.cs；CoreHost 定义让它走 Brotli 分支。
	#_if PSEXE
		#_include_as_value launcherSource "$PSScriptRoot/programFrames/pack.cs"
	#_else
		[string]$launcherSource = Get-Content $PSScriptRoot/programFrames/pack.cs -Raw -Encoding UTF8
	#_endif
	$threadingAttr = if ($threadingModel -eq 'MTA') { '[System.MTAThread]' } else { '[System.STAThread]' }
	$launcherSource = $launcherSource.Replace('[System.STAThread]', $threadingAttr)
	[System.IO.File]::WriteAllText((Join-Path $projectDir 'launcher.cs'), $launcherSource, [System.Text.UTF8Encoding]::new($false))

	# launcher 需要 noConsole，CoreHost 才能用 MessageBox 报错而不是写不存在的控制台。
	$launcherDefineConstants = if ($noConsole) { 'CoreHost;noConsole' } else { 'CoreHost' }
	$launcherCsproj = @"
<Project Sdk="Microsoft.NET.Sdk">
	<PropertyGroup>
$($publishedProps.Replace('__DefineConstants__', $launcherDefineConstants))
	</PropertyGroup>
	<ItemGroup>
		<Compile Include="launcher.cs" />
		<Compile Include="CoreHost.cs" />
		<EmbeddedResource Include="main" LogicalName="main" />
	</ItemGroup>
</Project>
"@
	[System.IO.File]::WriteAllText((Join-Path $projectDir 'launcher.csproj'), $launcherCsproj, [System.Text.UTF8Encoding]::new($false))

	Write-I18n Host CoreCompilePublishing
	Write-Debug "Core compiler: dotnet publish launcher (tfm=$tfm, rid=$rid)"
	$publishOutput = & $dotnet.Source publish (Join-Path $projectDir 'launcher.csproj') -c Release -o $publishDir --nologo -v quiet 2>&1
	if ($LASTEXITCODE -ne 0) {
		throw ($publishOutput -join "`n")
	}
}

$publishedExe = Join-Path $publishDir "$assemblyName.exe"
if (-not (Test-Path -LiteralPath $publishedExe)) {
	Write-I18n Error OutputFileNotWritten -Category WriteError
	throw 'ps12exe:core-no-output'
}
Copy-Item -LiteralPath $publishedExe -Destination $outputFile -Force

if ($prepareDebug) {
	$publishedPdb = Join-Path $publishDir "$assemblyName.pdb"
	if (Test-Path -LiteralPath $publishedPdb) {
		Copy-Item -LiteralPath $publishedPdb -Destination ($outputFile -replace '\.exe$', '.pdb') -Force
	}
}
