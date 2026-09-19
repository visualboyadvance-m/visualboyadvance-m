#Requires -Version 5.0

<#
.SYNOPSIS
Converts powershell scripts to standalone executables or preprocesses PowerShell scripts.
.DESCRIPTION
Converts powershell scripts to standalone executables. GUI output and input is activated with one switch,
real windows executables are generated. You may use the graphical front end ps12exeGUI for convenience.
Alternatively, preprocesses a PowerShell script, handling directives like `#_if`, `#_else`, `#_endif`, and `#_include`.

Please see Remarks on project page for topics "GUI mode output formatting", "Config files", "Password security",
"Script variables" and "Window in background in App.Windowed mode".

.PARAMETER inputFile
Powershell script file path or url to convert to executable (file has to be UTF8 or UTF16 encoded)

.PARAMETER Content
The content of the PowerShell script to convert to executable

.PARAMETER outputFile
destination executable file name or folder, defaults to inputFile with extension '.exe'

.PARAMETER App
A hashtable describing how the produced application behaves. Supported keys:
- Windowed: build a Windows Forms application without a console window (default: $false)
- Silence: stream names to suppress; one or more of 'Output', 'Verbose', 'Error', 'Warning', 'Debug', or '*' (default: empty)
- OutputEncoding: console output encoding; 'Default', 'UTF8' or 'UTF16LE' (default: 'Default')
- VisualStyles: enable visual styles for GUI applications (default: $true)
- ExitOnCancel: exit when Cancel or "X" is selected in a Read-Host input box (default: $false)
- CredentialGUI: use a GUI for prompting credentials in console mode (default: $false)
- DpiAware: mark the compiled executable as DPI aware (default: $false)
- WinFormsDpiAware: let WinForms use DPI scaling; forces -ConfigFile (default: $false)

.PARAMETER Os
A hashtable of OS integration options. Supported keys:
- Admin: require an elevated context, i.e. UAC (default: $false)
- ModernOS: declare support for the newest Windows versions (default: $false)
- LongPaths: enable paths longer than 260 characters (default: $false)
- Virtualize: activate application virtualization; forces x86 (default: $false)

.PARAMETER Build
A hashtable of build/toolchain options. Supported keys:
- Target: target runtime; 'Framework4.0' (default), 'Framework2.0' or 'Core'.
  'Core' compiles a PowerShell Core (.NET) executable: both the build machine and the target machine must have
  PowerShell Core and a matching .NET runtime installed, and the generated executable is much larger.
- Platform: 'AnyCpu' (default), 'x64' or 'x86'
- Apartment: 'STA' (default) or 'MTA'
- Culture: culture name used at runtime by the compiled executable (default: current user culture)
- Options: additional compiler options (see https://msdn.microsoft.com/en-us/library/78f4aasd.aspx)
- KeepSource: keep the generated C# source for debugging (default: $false)
- Minify: scriptblock to minify the script before compiling
- TempDir: directory for storing temporary files (default: random directory in %temp%)

.PARAMETER Resources
A hashtable of version resources embedded into the compiled executable.
Supported keys are 'Icon', 'Title', 'Description', 'Company', 'Product', 'Copyright', 'Trademark', 'Version'.
Icon can be a file path or URL to an icon file. All other values are strings.
see https://msdn.microsoft.com/en-us/library/system.reflection.assemblytitleattribute(v=vs.110).aspx for details

.PARAMETER Signing
A hashtable of code signing options. Supported keys:
- Certificate: path to a PFX certificate file
- Password: SecureString password for the PFX file
- Thumbprint: certificate thumbprint from the Windows Certificate Store
- Timestamp: timestamp server URL (default: http://timestamp.digicert.com)
Either Certificate or Thumbprint must be specified.

.PARAMETER PreprocessOnly
only preprocesses the input PowerShell script and outputs the preprocessed code. No executable is generated.

.PARAMETER Golf
Enables golf mode, adding abbreviations and common functions to the script.

.PARAMETER Sandbox
Compile scripts with additional protection, prevent native files from being accessed.

.PARAMETER NoUpdateCheck
Do not check for updates.

.PARAMETER Locale
The language code to be used for compiler messages.

.PARAMETER ConfigFile
write a config file (<outputfile>.exe.config)

.PARAMETER help
Display localized help message

.EXAMPLE
ps12exe C:\Data\MyScript.ps1
Compiles C:\Data\MyScript.ps1 to C:\Data\MyScript.exe as console executable
.EXAMPLE
ps12exe -inputFile C:\Data\MyScript.ps1 -outputFile C:\Data\MyScriptGUI.exe -Resources @{Icon='C:\Data\Icon.ico'; Title='MyScript'; Version='0.0.0.1'} -App @{Windowed=$true}
Compiles C:\Data\MyScript.ps1 to C:\Data\MyScriptGUI.exe as graphical executable, icon and meta data
.EXAMPLE
ps12exe -inputFile C:\Data\MyScript.ps1 -PreprocessOnly
Preprocesses C:\Data\MyScript.ps1 and outputs the preprocessed code.
.EXAMPLE
ps12exe -inputFile C:\Data\MyScript.ps1 -outputFile C:\Data\MyScript.exe -Signing @{Certificate='C:\Cert\mycert.pfx'; Password=(ConvertTo-SecureString 'password' -AsPlainText -Force); Timestamp='http://timestamp.digicert.com'}
Compiles C:\Data\MyScript.ps1 and signs it with a PFX certificate.
.EXAMPLE
ps12exe -inputFile C:\Data\MyScript.ps1 -outputFile C:\Data\MyScript.exe -Signing @{Thumbprint='ABC123DEF456'}
Compiles C:\Data\MyScript.ps1 and signs it with a certificate from Windows Certificate Store.
#>
[CmdletBinding(DefaultParameterSetName = 'InputFile')]
Param(
	[Parameter(ParameterSetName = 'InputFile', Position = 0)]
	[ValidatePattern("^(https?|ftp)://.*|.*\.(ps1|psd1|tmp)$")]
	[String]$inputFile,
	[Parameter(ParameterSetName = 'Content', ValueFromPipeline = $TRUE)]
	[String]$Content,
	[Parameter(ParameterSetName = 'InputFile', Position = 1)]
	[Parameter(ParameterSetName = 'Content', Position = 0)]
	[ValidatePattern(".*\.(exe|com|scr|bin|bat|cmd)$")]
	[String]$outputFile = $NULL,
	[ArgumentCompleter({
		Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
		$validKeys = @('Windowed', 'Silence', 'OutputEncoding', 'VisualStyles', 'ExitOnCancel', 'CredentialGUI', 'DpiAware', 'WinFormsDpiAware')
		if (-not $wordToComplete) { return "@{}" }
		$wordToComplete = $wordToComplete.Trim('"', "'", ' ', '`t', '{', '}')
		if ($wordToComplete -match '=') { return }
		$validKeys | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object { "$_=" }
	})]
	[HashTable]$App = @{},
	[ArgumentCompleter({
		Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
		$validKeys = @('Admin', 'ModernOS', 'LongPaths', 'Virtualize')
		if (-not $wordToComplete) { return "@{}" }
		$wordToComplete = $wordToComplete.Trim('"', "'", ' ', '`t', '{', '}')
		if ($wordToComplete -match '=') { return }
		$validKeys | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object { "$_=" }
	})]
	[HashTable]$Os = @{},
	[ArgumentCompleter({
		Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
		$validKeys = @('Target', 'Platform', 'Apartment', 'Culture', 'Options', 'KeepSource', 'Minify', 'TempDir')
		if (-not $wordToComplete) { return "@{}" }
		$wordToComplete = $wordToComplete.Trim('"', "'", ' ', '`t', '{', '}')
		if ($wordToComplete -match '=') { return }
		$validKeys | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object { "$_=" }
	})]
	[HashTable]$Build = @{},
	[ArgumentCompleter({
		Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
		$validKeys = @('Icon', 'Title', 'Description', 'Company', 'Product', 'Copyright', 'Trademark', 'Version')
		if (-not $wordToComplete) { return "@{}" }
		$wordToComplete = $wordToComplete.Trim('"', "'", ' ', '`t', '{', '}')
		if ($wordToComplete -match '=') { return }
		$validKeys | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object { "$_=" }
	})]
	[HashTable]$Resources = @{},
	[ArgumentCompleter({
		Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
		if (-not $wordToComplete) { return "@{}" }
		$validKeys = @('Certificate', 'Password', 'Thumbprint', 'Timestamp')
		$wordToComplete = $wordToComplete.Trim('"', "'", ' ', '`t', '{', '}')
		if ($wordToComplete -match '=') { return }
		$validKeys | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object { "$_=" }
	})]
	[Hashtable]$Signing,
	[Switch]$PreprocessOnly,
	[Switch]$Golf,
	[Switch]$Sandbox,
	[Switch]$NoUpdateCheck,
	#_if PSScript
		[ArgumentCompleter({
			Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
			. "$PSScriptRoot\src\LocaleArgCompleter.ps1" @PSBoundParameters
		})]
	#_endif
	[string]$Locale,
	[Switch]$ConfigFile,
	[Switch]$help
)
# 对象 API → 内部规范变量。模式类参数必须在预处理之前就绪。
# 是否由父 ps12exe 交接而来（子宿主）通过环境变量传递，不再占用参数；读到即删，避免污染子进程的后续调用。
$nested = [bool]$env:PS12EXE_NESTED
Remove-Item Env:PS12EXE_NESTED -ErrorAction Ignore
$GuestMode = [bool]$Sandbox
$GolfMode = [bool]$Golf
$SkipVersionCheck = [bool]$NoUpdateCheck
$minifyer = $Build.Minify
$global:LastExitCode = 0 # 无错误
$Verbose = $PSCmdlet.MyInvocation.BoundParameters["Verbose"].IsPresent
$Debug = $DebugPreference -ne 'SilentlyContinue'
$UICultureBackup = [cultureinfo]::CurrentUICulture
function RollUp {
	param ($num = 1, [switch]$InVerbose)
	if (-not ($Verbose -or $InVerbose -or $Debug)) {
		if ($Host.UI.SupportsVirtualTerminal) {
			Write-Host $([char]27 + '[' + $num + 'A') -NoNewline
		}
		elseif (-not $nested) {
			if ($CousorPos = $Host.UI.RawUI.CursorPosition) {
				try {
					$CousorPos.Y = $CousorPos.Y - $num
					$Host.UI.RawUI.CursorPosition = $CousorPos
				}
				catch { $Error.RemoveAt(0) }
			}
		}
	}
}
if ($Debug) { $DebugPreference = 'Continue' } # 修复 -debug 会把它设为 'Inquire' 的问题
#_if PSScript
	$LocaleLoaderArg = @{ Locale = $Locale }
	if ($nested) { $LocaleLoaderArg.FailedLoadLocaleData = {} }
#_endif
$LocalizeData =
#_if PSScript
	. $PSScriptRoot\src\LocaleLoader.ps1 @LocaleLoaderArg
#_else
	#_include "$PSScriptRoot/src/locale/en-UK.ps1"
#_endif
. $PSScriptRoot\src\WriteI18n.ps1
Set-I18nData -I18nData $LocalizeData.CompilingI18nData
function Show-Help {
	. $PSScriptRoot\src\HelpShower.ps1 -HelpData $LocalizeData.ConsoleHelpData | Write-Host
}
#_if PSScript
	$versionNow = (Get-Module -ListAvailable ps12exe | Sort-Object -Property Version -Descending | Select-Object -First 1).Version
	if ($versionNow -ne '0.0.0') { # 非开发版本
		if (Test-Path $env:TEMP/ps12exe_version.txt) {
			$versionOnline = Get-Content $env:TEMP/ps12exe_version.txt -Encoding utf8 | Select-Object -First 1
			if ((-not $nested) -and (-not $SkipVersionCheck) -and ($versionNow -ne $versionOnline)) {
				try {
					$ForegroundColor = try { $Host.UI.RawUI.ForegroundColor } catch { 'White' }
					$Host.UI.RawUI.ForegroundColor = "Yellow"
				}
				catch {}
				Write-I18n Host NewVersionAvailable $versionOnline
				try { $Host.UI.RawUI.ForegroundColor = $ForegroundColor } catch {}
			}
		}
		if ((-not $nested) -and (-not $SkipVersionCheck) -and -not (Get-Job -Name ps12exe_version_check -ErrorAction Ignore)) {
			Start-Job {
				$versionOnline = (Find-Module ps12exe | Sort-Object -Property Version -Descending | Select-Object -First 1).Version
				Set-Content $env:TEMP/ps12exe_version.txt -Value $versionOnline -Encoding utf8
			} -Name ps12exe_version_check | Out-Null
		}
	}
#_endif
if ($help) {
	Show-Help
	return
}
if (-not ($inputFile -or $Content)) {
	Show-Help
	Write-Host
	Write-I18n Error NoneInput -Category InvalidArgument
	if ([System.Console]::IsOutputRedirected -or [System.Console]::IsInputRedirected -or [System.Console]::IsErrorRedirected) {
		$global:LastExitCode = 2 # 调用格式错误
	}
	else {
		& "$PSScriptRoot\src\Interact\main.ps1" -Locale $Locale # 没有输入时启动交互模式
	}
	return
}

$Params = $PSBoundParameters
$ParamList = $MyInvocation.MyCommand.Parameters
$Params.Remove('Content') | Out-Null #防止回滚覆盖
$Params.Remove('PreprocessOnly') | Out-Null # 从参数中移除 PreprocessOnly，供编译步骤使用

function bytesOfString([string]$str) {
	if ($str) { [system.Text.Encoding]::UTF8.GetBytes($str).Count } else { 0 }
}
function Test-StdoutRedirected {
	# 控制台重定向（管道 / 1>文件）。在交互式 ConsoleHost 中，`$exe = ps12exe` 会在不设置 IsOutputRedirected 的情况下捕获 stdout——仍属 stdout 捕获，而非 stderr（仅 2>$null）。
	if ([System.Console]::IsOutputRedirected) { return $true }
	$line = (Get-PSCallStack)[1].InvocationInfo.Line
	return $line -match '\$\w+\s*='
}
#_if PSScript #在PSEXE中主机永远是winpwsh，所以不会内嵌
if (!$nested) {
#_endif
	[System.Collections.ArrayList]$DllExportList = @()
	if ($inputFile -and $Content) {
		Write-I18n Error BothInputAndContentSpecified -Category InvalidArgument
		$global:LastExitCode = 2 # 调用格式错误
		return
	}
	. $PSScriptRoot\src\ReadScriptFile.ps1
	try {
		if ($inputFile) {
			$Content = ReadScriptFile $inputFile
			if ((bytesOfString $Content) -ne (Get-Item $inputFile -ErrorAction Ignore).Length) {
				Write-I18n Host PreprocessedScriptSize $(bytesOfString $Content)
			}
		}
		else {
			$NewContent = Preprocessor ($Content -split '\r?\n') "$PWD\a.ps1"
			Write-I18n Verbose PreprocessDone
			if ((bytesOfString $NewContent) -ne (bytesOfString $Content)) {
				Write-I18n Host PreprocessedScriptSize $(bytesOfString $NewContent)
			}
			$Content = $NewContent
		}
		$isGolf = $true
		if ($Content -match '^C\|') { $Content = '$CI' + $Content.Substring(1) }
		elseif ($Content -match '^S\|') { $Content = '$SI' + $Content.Substring(1) }
		elseif ($Content -match '^N\|') { $Content = '$NI' + $Content.Substring(1) }
		elseif ($Content -match '^\|') { $Content = '$I' + $Content }
		elseif ($Content -match '^C%') { $Content = '$CA|' + $Content.Substring(2) }
		elseif ($Content -match '^S%') { $Content = '$SA|' + $Content.Substring(2) }
		elseif ($Content -match '^N%') { $Content = '$NA|' + $Content.Substring(2) }
		elseif ($Content -match '^%') { $Content = '$A|' + $Content.Substring(1) }
		elseif (!$GolfMode) { $isGolf = $false }
		if ($isGolf) {
			#_if PSScript
				$GolfModeHeader = Get-Content $PSScriptRoot\src\GolfModeHeader.ps1 -Encoding UTF8 -Raw
			#_else
				#_include_as_value GolfModeHeader $PSScriptRoot\src\GolfModeHeader.ps1
			#_endif
			$Content = $GolfModeHeader + "`n" + $Content
		}
	}
	catch {
		$global:LastExitCode = 1 # 脚本预处理失败
		if ($_.Exception.Message -ne 'ScriptHalted') { Write-Error $_.Exception }
		return
	}
	if ($minifyer -is [string]) {
		if (Get-Command $minifyer -ErrorAction Ignore) {
			$minifyer = "$minifyer `$_"
		}
		$minifyer = [scriptblock]::Create($minifyer)
	}
	if ($minifyer) {
		Write-I18n Host MinifyingScript
		try {
			# 获取调用方的堆栈帧
			$Stack = Get-PSCallStack
			$Frame = $Stack[1]
			$Variables = $Frame.GetFrameVariables()
			$Variables._ = [System.Management.Automation.PSVariable]::New('_', $Content)
			$MinifyedContent = $minifyer.InvokeWithContext(@{}, $Variables.Values, $Variables.args.Value)
			RollUp
			Write-I18n Host MinifyedScriptSize $(bytesOfString $MinifyedContent)
		}
		catch {
			Write-I18n Error MinifyerError $_ -Exception $_.Exception
		}
		if (-not $MinifyedContent -and $Content) {
			Write-I18n Warning MinifyerFailedUsingOriginalScript
		}
		else {
			$Content = $MinifyedContent
		}
	}
#_if PSScript
}
else {
	$Content = Get-Content -Raw -LiteralPath $inputFile -Encoding UTF8 -ErrorAction SilentlyContinue
	if (!$Content) {
		Write-I18n Error TempFileMissing $inputFile -Category ResourceUnavailable
		$global:LastExitCode = 3 # 资源丢失
		return
	}
	if (!$TempDir) {
		Remove-Item $inputFile -ErrorAction SilentlyContinue
	}
}
#_endif

if ($PreprocessOnly) {
	Write-I18n Host PreprocessOnlyDone
	$global:LastExitCode = 0
	return $Content
}

# pragma预处理命令可能会修改参数，所以现在开始参数更新
$Params.GetEnumerator() | ForEach-Object {
	Set-Variable -Name $_.Key -Value $_.Value
}

# 对象 API（可能已被 pragma 覆盖）→ 内部规范变量；下游编译器与 C# 帧使用约定的内部变量名。
function Get-Opt([System.Collections.IDictionary]$Table, [string]$Key, $Default) {
	if ($Table -and ($Table.Keys -contains $Key)) {
		$v = $Table[$Key]
		if ($null -ne $v -and "$v" -ne '') { return $v }
	}
	return $Default
}
function ConvertTo-OptBool($Value, [bool]$Default) {
	if ($null -eq $Value) { return $Default }
	if ($Value -is [bool]) { return $Value }
	if ($Value -is [switch]) { return [bool]$Value }
	$s = "$Value".Trim().ToLowerInvariant()
	if ($s -in @('off', 'false', 'n', 'no', 'unset', '0', 'disable')) { return $false }
	if ($s -in @('on', 'true', 'y', 'yes', 'set', '1', 'enable')) { return $true }
	return [bool]$Value
}

# App
$noConsole = ConvertTo-OptBool (Get-Opt $App 'Windowed' $false) $false
$outputEncodingName = "$(Get-Opt $App 'OutputEncoding' 'Default')"
$UNICODEEncoding = $outputEncodingName -ieq 'UTF16LE'
$UTF8Encoding = $outputEncodingName -ieq 'UTF8'
$noVisualStyles = -not (ConvertTo-OptBool (Get-Opt $App 'VisualStyles' $true) $true)
$exitOnCancel = ConvertTo-OptBool (Get-Opt $App 'ExitOnCancel' $false) $false
$credentialGUI = ConvertTo-OptBool (Get-Opt $App 'CredentialGUI' $false) $false
$DPIAware = ConvertTo-OptBool (Get-Opt $App 'DpiAware' $false) $false
$winFormsDPIAware = ConvertTo-OptBool (Get-Opt $App 'WinFormsDpiAware' $false) $false

# Os
$requireAdmin = ConvertTo-OptBool (Get-Opt $Os 'Admin' $false) $false
$supportOS = ConvertTo-OptBool (Get-Opt $Os 'ModernOS' $false) $false
$longPaths = ConvertTo-OptBool (Get-Opt $Os 'LongPaths' $false) $false
$virtualize = ConvertTo-OptBool (Get-Opt $Os 'Virtualize' $false) $false

# Build
$targetRuntime = "$(Get-Opt $Build 'Target' 'Framework4.0')"
$architecture = "$(Get-Opt $Build 'Platform' 'AnyCpu')"
$threadingModel = "$(Get-Opt $Build 'Apartment' 'STA')"
$lcid = "$(Get-Opt $Build 'Culture' '')"
$CompilerOptions = "$(Get-Opt $Build 'Options' '/o+ /debug-')"
$prepareDebug = ConvertTo-OptBool (Get-Opt $Build 'KeepSource' $false) $false
if (Get-Opt $Build 'Minify' $null) { $minifyer = Get-Opt $Build 'Minify' $null }
$TempDir = "$(Get-Opt $Build 'TempDir' '')"
if (-not $TempDir) { $TempDir = $NULL }
# 常量求值控制：ConstEval.Enabled 为 $false 时跳过常量求值；ConstEval.Timeout 为 $true 时按已超时回退
$ConstEvalOption = Get-Opt $Build 'ConstEval' $null
$noConstEval = -not (ConvertTo-OptBool (Get-Opt $ConstEvalOption 'Enabled' $true) $true)
$constEvalTimeout = ConvertTo-OptBool (Get-Opt $ConstEvalOption 'Timeout' $false) $false
# 内部/开发用（暂不写入文档）
if (Get-Opt $Build 'DllExports' $null) {
	[System.Collections.ArrayList]$DllExportList = @(Get-Opt $Build 'DllExports' $null)
}
$StartupTiming = ConvertTo-OptBool (Get-Opt $Build 'StartupTiming' $false) $false

# 归一化枚举大小写，保证下游比较与取值一致
switch -Regex ($architecture) {
	'(?i)^x64$' { $architecture = 'x64' }
	'(?i)^x86$' { $architecture = 'x86' }
	'(?i)^anycpu$' { $architecture = 'anycpu' }
	default {
		Write-I18n Warning InvalidBuildPlatform $architecture
		$architecture = 'anycpu'
	}
}
$threadingModel = if ($threadingModel -ieq 'MTA') { 'MTA' } else { 'STA' }
switch -Regex ($targetRuntime) {
	'(?i)^Framework2\.0$' { $targetRuntime = 'Framework2.0' }
	'(?i)^Framework4\.0$' { $targetRuntime = 'Framework4.0' }
	'(?i)^Core$' { $targetRuntime = 'Core' }
	default {
		Write-I18n Warning InvalidBuildTarget $targetRuntime
		$targetRuntime = 'Framework4.0'
	}
}
$isCoreTarget = $targetRuntime -eq 'Core'

# App.Silence：静音流，细分到每个 PowerShell 流
$SilenceStreams = @(Get-Opt $App 'Silence' @())
if ($SilenceStreams.Count -eq 1 -and $SilenceStreams[0] -is [string]) {
	$SilenceStreams = $SilenceStreams[0] -split '[,;\s]+'
}
$SilenceStreams = @($SilenceStreams | ForEach-Object { "$_".Trim() } | Where-Object { $_ })
$noOutput = $SilenceStreams -contains 'Output' -or $SilenceStreams -contains '*'
$noVerbose = $SilenceStreams -contains 'Verbose' -or $SilenceStreams -contains '*'
$noError = $SilenceStreams -contains 'Error' -or $SilenceStreams -contains '*'
$noWarning = $SilenceStreams -contains 'Warning' -or $SilenceStreams -contains '*'
$noDebug = $SilenceStreams -contains 'Debug' -or $SilenceStreams -contains '*'

# Resources → 内部 resourceParams（键名与 C# 帧占位符一致）
$resourceParamKeys = @('iconFile', 'title', 'description', 'company', 'product', 'copyright', 'trademark', 'version')
$resourceAlias = @{
	icon = 'iconFile'; title = 'title'; description = 'description'; company = 'company'
	product = 'product'; copyright = 'copyright'; trademark = 'trademark'; version = 'version'
}
$resourceParams = @{}
foreach ($k in @($Resources.Keys)) {
	$lk = "$k".ToLowerInvariant()
	if ($resourceAlias.ContainsKey($lk)) { $resourceParams[$resourceAlias[$lk]] = $Resources[$k] }
	else { Write-I18n Warning InvalidResourceParam $k }
}
$NoResource = -not $resourceParams.Count
# 由于其他的resourceParams参数需要转义，iconFile参数不需要转义，所以提取出来单独处理
$iconFile = $resourceParams['iconFile']
$resourceParams.Remove('iconFile')

# Signing → 内部 CodeSigning
$CodeSigning = $null
if ($Signing -and $Signing.Count) {
	$CodeSigning = @{}
	if (Get-Opt $Signing 'Certificate' '') { $CodeSigning.Path = Get-Opt $Signing 'Certificate' '' }
	if (Get-Opt $Signing 'Password' '') { $CodeSigning.Password = Get-Opt $Signing 'Password' '' }
	if (Get-Opt $Signing 'Thumbprint' '') { $CodeSigning.Thumbprint = Get-Opt $Signing 'Thumbprint' '' }
	if (Get-Opt $Signing 'Timestamp' '') { $CodeSigning.TimestampServer = Get-Opt $Signing 'Timestamp' '' }
}

# 无论给定的是相对路径还是绝对路径，都获取绝对路径
if (-not $inputFile) {
	$inputFile = '.\a.ps1'
}
if ($inputFile -notmatch "^(https?|ftp)://") {
	$inputFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($inputFile)
}
if (-not $outputFile) {
	if ($inputFile -match "^https?://") {
		$outputFile = ([System.IO.Path]::Combine($PWD, [System.IO.Path]::GetFileNameWithoutExtension($inputFile) + ".exe"))
	}
	else {
		$outputFile = ([System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($inputFile), [System.IO.Path]::GetFileNameWithoutExtension($inputFile) + ".exe"))
	}
}
else {
	$outputFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($outputFile)
	if ((Test-Path $outputFile -PathType Container)) {
		$outputFile = ([System.IO.Path]::Combine($outputFile, [System.IO.Path]::GetFileNameWithoutExtension($inputFile) + ".exe"))
	}
}
#_if PSScript #在PSEXE中主机永远是winpwsh，可省略该部分
	. $PSScriptRoot/src/PSObjectToString.ps1
	function UsingHost($Boundparameters, $HostExe) {
		# 写临时脚本，把参数串成命令行交给另一个 PowerShell 宿主编译，再等它退出。
		$Params = ([hashtable]$Boundparameters).Clone()
		$Params.Remove("Content")
		$Params.Remove("inputFile")
		$Params.Remove("outputFile")
		# scriptblock 无法序列化，从 Build 里剔除 Minify 再交给子宿主
		if ($Params.Build -is [hashtable]) {
			$BuildForChild = @{}
			$Params.Build.GetEnumerator() | ForEach-Object { if ($_.Key -ne 'Minify') { $BuildForChild[$_.Key] = $_.Value } }
			if ($BuildForChild.Count) { $Params.Build = $BuildForChild } else { $Params.Remove('Build') }
		}
		$TempFile = if ($TempDir) {
			New-Item -ItemType Directory -Path $TempDir -ErrorAction SilentlyContinue | Out-Null
			[System.IO.Path]::Combine($TempDir, 'main.ps1')
		}
		else { [System.IO.Path]::GetTempFileName() }
		$Content | Set-Content $TempFile -Encoding UTF8 -NoNewline
		$Params.Add("outputFile", $outputFile)
		$Params.Add("inputFile", $TempFile)
		if ($TempDir) {
			if (-not ($Params.Build -is [hashtable])) { $Params.Build = @{} }
			$Params.Build.TempDir = $TempDir
		}
		if ($DllExportList.Length) {
			if (-not ($Params.Build -is [hashtable])) { $Params.Build = @{} }
			$Params.Build.DllExports = $DllExportList
		}
		$CallParam = Get-ArgsString $Params

		Write-Debug "Starting $HostExe ps12exe with parameters: $CallParam"

		# 子宿主通过环境变量得知自己是被交接来的，不再用 -nested 参数；子进程读到会自行删除，这里调用后也删掉自己的。
		$env:PS12EXE_NESTED = '1'
		& $HostExe -NoProfile -Command "&'$PSScriptRoot\ps12exe.ps1' $CallParam; exit `$LastExitCode" | Write-Host
		$global:LastExitCode = $LASTEXITCODE
		Remove-Item Env:PS12EXE_NESTED -ErrorAction Ignore
	}
	# Windows PowerShell 解析不了 Core 语法，因此 Core 目标必须在解析前交接给 pwsh。
	if (!$nested -and $isCoreTarget -and ($PSVersionTable.PSEdition -ne "Core")) {
		if (Get-Command pwsh -ErrorAction Ignore) {
			UsingHost $Params 'pwsh'
			if ((Test-Path -LiteralPath $outputFile) -and (Test-StdoutRedirected)) {
				Write-Output $outputFile
			}
			return
		}
		Write-I18n Error CoreCompileNeedPwsh -Category NotInstalled
		$global:LastExitCode = 2 # 调用格式错误
		return
	}
#_endif

# 语法检查
if ($targetRuntime -eq 'Framework2.0') {
	#_if PSScript
		$SyntaxErrors = powershell -version 2.0 -NoProfile -OutputFormat xml -file $PSScriptRoot/src/RuntimePwsh2.0/CodeChecker.ps1 -scriptText $Content
	#_else
		#_include_as_value Pwsh2CodeCheckerCodeStr $PSScriptRoot/src/RuntimePwsh2.0/CodeChecker.ps1
		#_!! powershell -version 2.0 -NoProfile -OutputFormat xml -Command "&{$Pwsh2CodeCheckerCodeStr} -scriptText '$($Content -replace "'","''")'"
	#_endif
}
else {
	[cultureinfo]::CurrentUICulture = $LocalizeData.LangID
	$SyntaxErrors = $Tokens = $null
	$AST = [System.Management.Automation.Language.Parser]::ParseInput($Content, [ref]$Tokens, [ref]$SyntaxErrors)
	[cultureinfo]::CurrentUICulture = $UICultureBackup
}
if ($SyntaxErrors) {
	$errorData = & $PSScriptRoot/src/SyntaxErrorI18nDataBuilder.ps1 -SyntaxErrors $SyntaxErrors -CodeContent $Content -Locale:$LocalizeData.LangID
	$lastFullText = $null
	$ErrMessage = @()
	foreach ($errinfo in $errorData) {
		$fullText = ($errinfo.SpoceText + $errinfo.Text) -join "`n"
		if ($fullText -ne $lastFullText) {
			$lastFullText = $fullText
			if (!$errinfo.SpoceText.contains($null)) {
				$StartLine = Write-I18n Output SyntaxErrorLineStart $errinfo.SpoceText
			}
			$ErrMessage += [ordered]@{
				Split         = ''
				StartLine     = $StartLine
				ScriptLine    = $errinfo.Text
				HighlightLine = (" " * ([Math]::Max(0, $errinfo.Spoce.Column - 1)) + '^' * ([Math]::Max($errinfo.Spoce.ColumnEnd - $errinfo.Spoce.Column, 1)))
				Messages      = @()
			}
		}
		$ErrMessage[-1].Messages += $errinfo.Message
	}
	Write-I18n Error -Category ParserError -TargetObject @{
		Errors      = $SyntaxErrors
		Text        = ($ErrMessage | ForEach-Object { $_.GetEnumerator() | ForEach-Object { $_.Value } }) -join "`n"
		MessageTree = $ErrMessage
	} InputSyntaxError
	$ErrMessage | ForEach-Object {
		Write-Host $_.Split
		if ($_.StartLine) { Write-Host $_.StartLine -ForegroundColor Cyan }
		if ($_.ScriptLine) {
			Write-Host $_.ScriptLine
			Write-Host $_.HighlightLine -ForegroundColor Red
		}
		Write-Host ($_.Messages -join "`n")
	}
	if (-not $isCoreTarget) {
		Write-I18n Host CoreCompileHint -ForegroundColor Yellow
	}
	$global:LastExitCode = 1 # 脚本语法错误
	return
}
elseif (!$AST) {
	$AST = [System.Management.Automation.Language.Parser]::ParseInput($Content, [ref]$null, [ref]$null)
}

#_if PSScript #在PSEXE中主机永远是winpwsh，可省略该部分
	# pwsh 下默认交给 Windows PowerShell + CodeDom；若没有 WinPS，只能报错让用户显式选 Core。
	if (!$nested -and -not $isCoreTarget -and ($PSVersionTable.PSEdition -eq "Core")) {
		if (Get-Command powershell -ErrorAction Ignore) {
			UsingHost $Params 'powershell'
			if ((Test-Path -LiteralPath $outputFile) -and (Test-StdoutRedirected)) {
				Write-Output $outputFile
			}
			return
		}
		Write-I18n Error CoreCompileNeedWindowsPowerShell -Category NotInstalled
		$global:LastExitCode = 2 # 调用格式错误
		return
	}
#_endif

if ($inputFile -eq $outputFile) {
	Write-I18n Error IdenticalInputOutput -Category InvalidArgument
	$global:LastExitCode = 2 # 调用格式错误
	return
}

if ($winFormsDPIAware) {
	$supportOS = $TRUE
}

if ($virtualize) {
	$VirtualizeConflicts = [ordered]@{ requireAdmin = $requireAdmin; supportOS = $supportOS; longPaths = $longPaths }
	foreach ($a in $VirtualizeConflicts.Keys) {
		if ($VirtualizeConflicts[$a]) {
			Write-I18n Error "CombinedArg_Virtualize_$a" -Category InvalidArgument
			$global:LastExitCode = 2 # 调用格式错误
			return
		}
	}
}

if (!$configFile) {
	$ConfigForcing = [ordered]@{ longPaths = $longPaths; winFormsDPIAware = $winFormsDPIAware }
	foreach ($a in $ConfigForcing.Keys) {
		if ($ConfigForcing[$a]) {
			Write-I18n Warning "CombinedArg_NoConfigFile_$a" -Category InvalidArgument
			$configFile = $true
		}
	}
}

# 转义版本信息中的转义序列
$resourceParamKeys | ForEach-Object {
	if ($resourceParams.ContainsKey($_)) {
		$resourceParams[$_] = $resourceParams[$_] -replace "\\", "\\"
	}
}


. $PSScriptRoot\src\AstAnalyze.ps1
. $PSScriptRoot\src\TaskbarProgress.ps1
$AstAnalyzeResult = AstAnalyze $Ast
Write-Debug "AstAnalyzeResult: $(($AstAnalyzeResult|ConvertTo-Json) -split "\r?\n" -ne '' -join "`n")"
$CommandNames = (Get-Command).Name + (Get-Alias).Name
$FoundCmdlets = @()
$NotFoundCmdlets = @()
$AstAnalyzeResult.UsedNonConstFunctions | ForEach-Object {
	if ($_ -match '\$' -or -not $_) { return }
	if ($CommandNames -notcontains $_) {
		if ($_ -match '^[\w\-_]+$' -and (Get-Command $_ -ErrorAction Ignore)) {
			$FoundCmdlets += $_
		}
		# 跳过成员函数，因为解析Add-Type太过复杂
		elseif (-not $_.Contains(']::')) {
			$NotFoundCmdlets += $_
		}
	}
}
if ($AST.ParamBlock) { $AstAnalyzeResult.IsConst = $false }
$NotFoundTypes = @()
$AstAnalyzeResult.UsedNonConstTypes | ForEach-Object {
	if (!($_ -as [Type])) {
		$NotFoundTypes += $_
	}
}
if ($FoundCmdlets) {
	Write-I18n Warning SomeCmdletsMayNotAvailable $($FoundCmdlets -join '、')
}
if ($NotFoundCmdlets) {
	Write-I18n Warning SomeNotFoundCmdlets $($NotFoundCmdlets -join '、')
}
if ($NotFoundTypes) {
	Write-I18n Warning SomeTypesMayNotAvailable $($NotFoundTypes -join '、')
}
if ($TempDir) {
	New-Item -ItemType Directory -Path $TempDir -ErrorAction SilentlyContinue | Out-Null
}
try {
	Write-TaskbarProgress -Percent 0
	. $PSScriptRoot\src\InitCompileThings.ps1
	Write-TaskbarProgress -Percent 10
	#_if PSScript
		# 常量脚本优先生成 TinySharp 壳（体积 ~1KB）；产物是 .NET Framework 托管 PE，Core 目标跳过它改走 CoreCompiler。
		if ($AstAnalyzeResult.IsConst -and -not $requireAdmin -and -not $isCoreTarget) {
			Write-I18n Verbose TryingTinySharpCompile
			Write-I18n Host CompilingFile
			Write-TaskbarProgress -Percent 20

			try {
				. $PSScriptRoot\src\TinySharpCompiler.ps1
				$TinySharpSuccess = $TRUE
			}
			catch {
				RollUp
				Write-I18n Verbose TinySharpFailedFallback
				Write-Error $_
			}
		}
	#_endif
	try {
		if (!$TinySharpSuccess) {
			Write-I18n Host CompilingFile
			Write-TaskbarProgress -Percent 25
			if ($isCoreTarget) {
				. $PSScriptRoot\src\CoreCompiler.ps1
			}
			else {
				. $PSScriptRoot\src\CodeDomCompiler.ps1
			}
		}
		RollUp
		Write-TaskbarProgress -Percent 70
	}
	catch {
		RollUp
		Write-TaskbarProgressError
		Write-I18n Host CompilationFailed -ForegroundColor Red
		throw $_
	}

	if (!(Test-Path $outputFile)) {
		Write-I18n Error OutputFileNotWritten -Category WriteError
		$global:LastExitCode = 3 # 无输出文件
		return
	}
	else {
		#_if PSScript
			if (-not $TinySharpSuccess -and -not $isCoreTarget) {
				Write-TaskbarProgress -Percent 75
				& $PSScriptRoot\src\ExeSinker.ps1 $outputFile -removeResources:$(
					$NoResource -and $AstAnalyzeResult.IsConst -and -not $requireAdmin
				) -removeVersionInfo:$($resourceParams.Count -eq 0)
			}
		#_endif
		Write-TaskbarProgressClear
		Write-I18n Host CompiledFileSize $((Get-Item $outputFile).Length)
		Write-I18n Verbose OutputPath $outputFile
		if ($configFile -and -not $isCoreTarget) {
			$configFileForEXE3 | Set-Content ($outputFile + ".config") -Encoding UTF8
			Write-I18n Host ConfigFileCreated
		}
		if ($prepareDebug -and -not $isCoreTarget) {
			$cr.TempFiles | Where-Object { $_ -ilike "*.cs" } | Select-Object -First 1 | ForEach-Object {
				$dstSrc = ([System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($outputFile), [System.IO.Path]::GetFileNameWithoutExtension($outputFile) + ".cs"))
				Write-I18n Host SourceFileCopied $dstSrc
				Copy-Item -Path $_ -Destination $dstSrc -Force
			}
			$cr.TempFiles | Remove-Item -Verbose:$FALSE -Force -ErrorAction SilentlyContinue
		}

		# 代码签名逻辑
		if ($CodeSigning) {
			Write-I18n Host SigningExecutable
			try {
				$cert = $null
				$timestampServer = if ($CodeSigning.TimestampServer) { $CodeSigning.TimestampServer } else { "http://timestamp.digicert.com" }

				if ($CodeSigning.Path) {
					if ($CodeSigning.Password) {
						$cert = Get-PfxCertificate -FilePath $CodeSigning.Path -Password $CodeSigning.Password
					}
					else {
						$cert = Get-PfxCertificate -FilePath $CodeSigning.Path
					}
				}
				elseif ($CodeSigning.Thumbprint) {
					$cert = Get-Item "Cert:\CurrentUser\My\$($CodeSigning.Thumbprint)" -ErrorAction SilentlyContinue
					if (!$cert) {
						$cert = Get-Item "Cert:\LocalMachine\My\$($CodeSigning.Thumbprint)" -ErrorAction SilentlyContinue
					}
				}

				if ($cert) {
					$signature = Set-AuthenticodeSignature -FilePath $outputFile -Certificate $cert -TimestampServer $timestampServer -HashAlgorithm SHA256
					if ($signature.Status -eq 'Valid') {
						Write-I18n Host ExecutableSignedSuccessfully
					}
					else {
						Write-I18n Warning SigningStatusNotValid $signature.Status $signature.StatusMessage
					}
				}
				else {
					Write-I18n Error CertificateNotFoundOrInvalidPassword
				}
			}
			catch {
				Write-TaskbarProgressError
				Write-I18n Error SigningFailed $_.Exception.Message
			}
		}
	}
	if (!$nested -and (Test-StdoutRedirected)) {
		Write-Output $outputFile
	}
}
catch {
	Write-TaskbarProgressError
	if (Test-Path $outputFile) {
		Remove-Item $outputFile -Verbose:$FALSE
	}
	$_ | Write-Error -ErrorAction Continue
	if ($_.CategoryInfo.Category -eq 'ReadError') {
		$global:LastExitCode = 1 # 读取错误
		return
	}
	#_if PSScript
		if (!$GuestMode) {
			$global:LastExitCode = 3 # 内部未知错误
			$githubfeedback = "https://github.com/steve02081504/ps12exe/issues/new?assignees=steve02081504&labels=bug&projects=&template=bug-report.yaml"
			$urlParams = @{
				title                = "$_"
				"latest-release"     = if (Get-Module -ListAvailable ps12exe) { "true" } else { "false" }
				"bug-description"    = 'Compilation failed'
				"expected-behavior"  = 'Compilation should succeed'
				"additional-context" = @"
Version infos:
``````
$($PSVersionTable | Format-List | Out-String)
``````
Error message:
``````
$($_ | Format-List | Out-String)
``````
"@
			}
			foreach ($key in $urlParams.Keys) {
				$githubfeedback += "&$key=$([system.uri]::EscapeDataString($urlParams[$key]))"
			}
			Write-I18n Host OopsSomethingWentWrong -ForegroundColor Yellow
			if ($versionNow -eq '0.0.0') {} # 开发版本，什么也不做
			elseif ($versionNow -ne $versionOnline) {
				Write-I18n Host TryUpgrade $versionOnline -ForegroundColor Yellow
			}
			elseif (-not (Test-StdoutRedirected)) {
				Write-I18n Host EnterToSubmitIssue -ForegroundColor Yellow
				Read-Host | Out-Null
				Start-Process $githubfeedback
			}
		}
	#_endif
}
finally {
	Write-TaskbarProgressClear
	if ($TempTempDir) {
		Remove-Item $TempTempDir -Recurse -Force -ErrorAction SilentlyContinue
	}
}
#_if PSEXE
	#_!! exit $LastExitCode
#_endif
