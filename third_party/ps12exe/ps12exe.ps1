#Requires -Version 5.0

<#
.SYNOPSIS
Converts powershell scripts to standalone executables or preprocesses PowerShell scripts.
.DESCRIPTION
Converts powershell scripts to standalone executables. GUI output and input is activated with one switch,
real windows executables are generated. You may use the graphical front end ps12exeGUI for convenience.
Alternatively, preprocesses a PowerShell script, handling directives like `#_if`, `#_else`, `#_endif`, and `#_include`.

Please see Remarks on project page for topics "GUI mode output formatting", "Config files", "Password security",
"Script variables" and "Window in background in -noConsole mode".

.PARAMETER inputFile
Powershell script file path or url to convert to executable (file has to be UTF8 or UTF16 encoded)

.PARAMETER Content
The content of the PowerShell script to convert to executable

.PARAMETER outputFile
destination executable file name or folder, defaults to inputFile with extension '.exe'

.PARAMETER CompilerOptions
additional compiler options (see https://msdn.microsoft.com/en-us/library/78f4aasd.aspx)

.PARAMETER TempDir
directory for storing temporary files (default is random generated temp directory in %temp%)

.PARAMETER minifyer
scriptblock to minify the script before compiling

.PARAMETER lcid
location ID for the compiled executable. Current user culture if not specified

.PARAMETER noConsole
the resulting executable will be a Windows Forms app without a console window.
You might want to pipe your output to Out-String to prevent a message box for every line of output
(example: dir C:\ | Out-String)

.PARAMETER prepareDebug
create helpful information for debugging of generated executable. See parameter -debug there

.PARAMETER architecture
compile for specific runtime only. Possible values are 'x64' and 'x86' and 'anycpu'

.PARAMETER threadingModel
Threading model for the compiled executable. Possible values are 'STA' and 'MTA'

.PARAMETER resourceParams
A hashtable that contains resource parameters for the compiled executable. Possible keys are 'iconFile', 'title', 'description', 'company', 'product', 'copyright', 'trademark', 'version'
iconFile can be a file path or url to an icon file. All other values are strings.
see https://msdn.microsoft.com/en-us/library/system.reflection.assemblytitleattribute(v=vs.110).aspx for details

.PARAMETER UNICODEEncoding
encode output as UNICODE in console mode, useful to display special encoded chars

.PARAMETER credentialGUI
use GUI for prompting credentials in console mode instead of console input

.PARAMETER configFile
write a config file (<outputfile>.exe.config)

.PARAMETER noOutput
the resulting executable will generate no standard output (includes verbose and information channel)

.PARAMETER noError
the resulting executable will generate no error output (includes warning and debug channel)

.PARAMETER noVisualStyles
disable visual styles for a generated windows GUI application. Only applicable with parameter -noConsole

.PARAMETER exitOnCancel
exits program when Cancel or "X" is selected in a Read-Host input box. Only applicable with parameter -noConsole

.PARAMETER DPIAware
if display scaling is activated, GUI controls will be scaled if possible.

.PARAMETER winFormsDPIAware
creates an entry in the config file for WinForms to use DPI scaling. Forces -configFile and -supportOS

.PARAMETER requireAdmin
if UAC is enabled, compiled executable will run only in elevated context (UAC dialog appears if required)

.PARAMETER supportOS
use functions of newest Windows versions (execute [Environment]::OSVersion to see the difference)

.PARAMETER virtualize
application virtualization is activated (forcing x86 runtime)

.PARAMETER longPaths
enable long paths ( > 260 characters) if enabled on OS (works only with Windows 10 or up)

.PARAMETER targetRuntime
the target runtime to compile for. Possible values are 'Framework4.0' or 'Framework2.0', default is 'Framework4.0'

.PARAMETER GuestMode
Compile scripts with additional protection, prevent native files from being accessed

.PARAMETER Localize
The language code to be used for server-side logging

.PARAMETER SkipVersionCheck
Do not check for updates

.PARAMETER help
Display localized help message

.PARAMETER PreprocessOnly
only preprocesses the input PowerShell script and outputs the preprocessed code. No executable is generated.

.PARAMETER GolfMode
Enables golf mode, adding abbreviations and common functions to the script.

.EXAMPLE
ps12exe C:\Data\MyScript.ps1
Compiles C:\Data\MyScript.ps1 to C:\Data\MyScript.exe as console executable
.EXAMPLE
ps12exe -inputFile C:\Data\MyScript.ps1 -outputFile C:\Data\MyScriptGUI.exe -iconFile C:\Data\Icon.ico -noConsole -title "MyScript" -version 0.0.0.1
Compiles C:\Data\MyScript.ps1 to C:\Data\MyScriptGUI.exe as graphical executable, icon and meta data
.EXAMPLE
ps12exe -inputFile C:\Data\MyScript.ps1 -PreprocessOnly
Preprocesses C:\Data\MyScript.ps1 and outputs the preprocessed code.
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
	[String]$outputFile = $NULL, [String]$CompilerOptions = '/o+ /debug-', [String]$TempDir = $NULL,
	[scriptblock]$minifyer = $null, [Switch]$noConsole, [Switch]$prepareDebug, [int]$lcid,
	[ValidateSet('x64', 'x86', 'anycpu')]
	[String]$architecture = 'anycpu',
	[ValidateSet('STA', 'MTA')]
	[String]$threadingModel = 'STA',
	[HashTable]$resourceParams = @{},
	[Switch]$UNICODEEncoding,
	[Switch]$credentialGUI,
	[Switch]$configFile,
	[Switch]$noOutput,
	[Switch]$noError,
	[Switch]$noVisualStyles,
	[Switch]$exitOnCancel,
	[Switch]$DPIAware,
	[Switch]$winFormsDPIAware,
	[Switch]$requireAdmin,
	[Switch]$supportOS,
	[Switch]$virtualize,
	[Switch]$longPaths,
	[ValidateSet('Framework2.0', 'Framework4.0')]
	[String]$targetRuntime = 'Framework4.0',
	[Switch]$SkipVersionCheck,
	[Switch]$GuestMode,
	[Switch]$PreprocessOnly,
	[Switch]$GolfMode,
	#_if PSScript
		[ArgumentCompleter({
			Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
			. "$PSScriptRoot\src\LocaleArgCompleter.ps1" @PSBoundParameters
		})]
	#_endif
	[string]$Localize,
	[Switch]$help,
	# TODO
	# in dev, not support yet
	[Parameter(DontShow)]
	[switch]$UseWindowsPowerShell = $true,
	# deprecated. use `-noConfigFile` instead.
	[Parameter(DontShow)]
	[Switch]$noConfigFile,
	# deprecated. use `-Architecture x86` instead.
	[Parameter(DontShow)]
	[Switch]$x86,
	# deprecated. use `-Architecture x64` instead.
	[Parameter(DontShow)]
	[Switch]$x64,
	# deprecated. use `-ThreadModel STA` instead.
	[Parameter(DontShow)]
	[Switch]$STA,
	# deprecated. use `-ThreadModel MTA` instead.
	[Parameter(DontShow)]
	[Switch]$MTA,
	# deprecated. use `-resourceParams {iconFile = $iconFile}` instead.
	[Parameter(DontShow)]
	[String]$iconFile,
	# deprecated. use `-resourceParams {title = $title}` instead.
	[Parameter(DontShow)]
	[String]$title,
	# deprecated. use `-resourceParams {description = $description}` instead.
	[Parameter(DontShow)]
	[String]$description,
	# deprecated. use `-resourceParams {company = $company}` instead.
	[Parameter(DontShow)]
	[String]$company,
	# deprecated. use `-resourceParams {product = $product}` instead.
	[Parameter(DontShow)]
	[String]$product,
	# deprecated. use `-resourceParams {copyright = $copyright}` instead.
	[Parameter(DontShow)]
	[String]$copyright,
	# deprecated. use `-resourceParams {trademark = $trademark}` instead.
	[Parameter(DontShow)]
	[String]$trademark,
	# deprecated. use `-resourceParams {version = $version}` instead.
	[Parameter(DontShow)]
	[String]$version,
	# deprecated. use `-targetRuntime Framework2.0` instead.
	[Parameter(DontShow)]
	[Switch]$runtime20,
	# deprecated. use `-targetRuntime Framework4.0` instead.
	[Parameter(DontShow)]
	[Switch]$runtime40,
	# internal. do not use it unless you know what you are doing.
	[Parameter(DontShow)]
	[Switch]$nested,
	# internal. do not use it unless you know what you are doing.
	[Parameter(DontShow)]
	[string]$DllExportList
)
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
				} catch { $Error.RemoveAt(0) }
			}
		}
	}
}
if ($Debug) { $DebugPreference = 'Continue' } # fix -debug sets it to 'Inquire'
#_if PSScript
	$LocaleLoaderArg = @{ Localize = $Localize }
	if ($nested) { $LocaleLoaderArg.FaildLoadLocaleData = {} }
#_endif
$LocalizeData =
#_if PSScript
	. $PSScriptRoot\src\LocaleLoader.ps1 @LocaleLoaderArg
#_else
	#_include "$PSScriptRoot/src/locale/en-UK.ps1"
#_endif
function Show-Help {
	. $PSScriptRoot\src\HelpShower.ps1 -HelpData $LocalizeData.ConsoleHelpData | Write-Host
}
function Write-I18n(
	[ValidateSet('Info', 'Warning', 'Error', 'Debug', 'Verbose', 'Host', 'Output')]
	$PipeLineType,
	$Mid,
	$FormatArgs,
	$ErrorId = $Mid,
	[System.Management.Automation.ErrorCategory]$Category = 'NotSpecified',
	$TargetObject,
	$Exception,
	$ForegroundColor = $Host.UI.RawUI.ForegroundColor
) {
	$value = $LocalizeData.CompilingI18nData[$Mid] -f $FormatArgs
	if (!$value) { $value = "fatal error: No i18n data for $Mid, Rest format args: $FormatArgs" }
	if (!$ForegroundColor) { $ForegroundColor = 'White' }
	switch ($PipeLineType) {
		'Info' { Write-Information $value }
		'Warning' { Write-Warning $value }
		'Error' {
			if (!$Exception) { $Exception = [System.Exception]::new($value) }
			try{$Host.UI.RawUI.ForegroundColor = "Red"}catch{}
			$Host.UI.WriteErrorLine($value)
			try{$Host.UI.RawUI.ForegroundColor = $ForegroundColor}catch{}
			Write-Error -Exception $Exception -Message $value -Category $Category -ErrorId $ErrorId -TargetObject $TargetObject -ErrorAction SilentlyContinue
		}
		'Debug' { Write-Debug $value }
		'Host' { Write-Host $value -ForegroundColor $ForegroundColor }
		'Output' { $value }
		'Verbose' { Write-Verbose $value }
	}
}
#_if PSScript
	$versionNow = (Get-Module -ListAvailable ps12exe | Sort-Object -Property Version -Descending | Select-Object -First 1).Version
	if ($versionNow -ne '0.0.0') { # not dev version
		if (Test-Path $env:TEMP/ps12exe_version.txt) {
			$versionOnline = Get-Content $env:TEMP/ps12exe_version.txt -Encoding utf8 | Select-Object -First 1
			if ((-not $nested) -and (-not $SkipVersionCheck) -and ($versionNow -ne $versionOnline)) {
				try {
					$ForegroundColor = $Host.UI.RawUI.ForegroundColor
					$Host.UI.RawUI.ForegroundColor = "Yellow"
				} catch {}
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
		& "$PSScriptRoot\src\Interact\main.ps1" -Localize $Localize # start interactive mode if no input
	}
	return
}

$Params = $PSBoundParameters
$ParamList = $MyInvocation.MyCommand.Parameters
$Params.Remove('Content') | Out-Null #防止回滚覆盖
$Params.Remove('DllExportList') | Out-Null
$Params.Remove('PreprocessOnly') | Out-Null # Remove PreprocessOnly from params for compilation step

function bytesOfString([string]$str) {
	if ($str) { [system.Text.Encoding]::UTF8.GetBytes($str).Count } else { 0 }
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
			# get caller's stackframe
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
	if ($DllExportList) {
		[System.Collections.ArrayList]$DllExportList = $DllExportList | ConvertFrom-Json
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

# 处理兼容旧版参数列表
if ($x86 -and $x64) {
	Write-I18n Error CombinedArg_x86_x64 -Category InvalidArgument
	$global:LastExitCode = 2 # 调用格式错误
	return
}
if ($x86) { $architecture = 'x86' }
if ($x64) { $architecture = 'x64' }
$Params.architecture = $architecture
[void]$Params.Remove("x86"); [void]$Params.Remove("x64")
if ($runtime20) {
	foreach ($a in @("runtime40", "longPaths", "winFormsDPIAware")) {
		if ($Params[$a]) {
			Write-I18n Error "CombinedArg_Runtime20_$a" -Category InvalidArgument
			$global:LastExitCode = 2 # 调用格式错误
			return
		}
	}
}
if ($runtime20) { $targetRuntime = 'Framework2.0' }
if ($runtime40) { $targetRuntime = 'Framework4.0' }
$Params.targetRuntime = $targetRuntime
[void]$Params.Remove("runtime20"); [void]$Params.Remove("runtime40")
if ($STA -and $MTA) {
	Write-I18n Error CombinedArg_STA_MTA -Category InvalidArgument
	$global:LastExitCode = 2 # 调用格式错误
	return
}
if ($STA) { $threadingModel = 'STA' }
if ($MTA) { $threadingModel = 'MTA' }
$Params.threadingModel = $threadingModel
[void]$Params.Remove("STA"); [void]$Params.Remove("MTA")
$resourceParamKeys = @('iconFile', 'title', 'description', 'company', 'product', 'copyright', 'trademark', 'version')
$resourceParamKeys | ForEach-Object {
	if ($Params.ContainsKey($_)) {
		$resourceParams[$_] = $Params[$_]
	}
	[void]$Params.Remove($_)
}
$resourceParams.GetEnumerator() | ForEach-Object {
	if (-not $resourceParamKeys.Contains($_.Key)) {
		Write-I18n Warning InvalidResourceParam $_.Key
	}
}
if ($configFile -and $noConfigFile) {
	Write-I18n Error CombinedArg_ConfigFileYes_No -Category InvalidArgument
	$global:LastExitCode = 2 # 调用格式错误
	return
}
if ($noConfigFile) { $configFile = $FALSE }
$NoResource = -not $resourceParams.Count
# 由于其他的resourceParams参数需要转义，iconFile参数不需要转义，所以提取出来单独处理
$iconFile = $resourceParams['iconFile']
$resourceParams.Remove('iconFile')

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
	$errorData = & $PSScriptRoot/src/SyntaxErrorI18nDataBuilder.ps1 -SyntaxErrors $SyntaxErrors -CodeContent $Content -Localize:$LocalizeData.LangID
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
				Split = ''
				StartLine = $StartLine
				ScriptLine = $errinfo.Text
				HighlightLine = (" "*([Math]::Max(0, $errinfo.Spoce.Column-1)) + '^'*([Math]::Max($errinfo.Spoce.ColumnEnd-$errinfo.Spoce.Column,1)))
				Messages = @()
			}
		}
		$ErrMessage[-1].Messages += $errinfo.Message
	}
	Write-I18n Error -Category ParserError -TargetObject @{
		Errors = $SyntaxErrors
		Text = ($ErrMessage | ForEach-Object{ $_.GetEnumerator() | ForEach-Object {$_.Value} }) -join "`n"
		MessageTree = $ErrMessage
	} InputSyntaxError
	$ErrMessage | ForEach-Object{
		Write-Host $_.Split
		if ($_.StartLine) { Write-Host $_.StartLine -ForegroundColor Cyan }
		if ($_.ScriptLine) {
			Write-Host $_.ScriptLine
			Write-Host $_.HighlightLine -ForegroundColor Red
		}
		Write-Host ($_.Messages -join "`n")
	}
	$global:LastExitCode = 1 # 脚本语法错误
	return
}
elseif (!$AST) {
	$AST = [System.Management.Automation.Language.Parser]::ParseInput($Content, [ref]$null, [ref]$null)
}

# retrieve absolute paths independent if path is given relative oder absolute
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
function UsingWinPowershell($Boundparameters) {
	# starting Windows Powershell
	$Params = ([hashtable]$Boundparameters).Clone()
	$Params.Remove("minifyer")
	$Params.Remove("Content")
	$Params.Remove("inputFile")
	$Params.Remove("outputFile")
	$Params.Remove("resourceParams") #使用旧版参数列表传递hashtable参数更为保险
	$TempFile = if ($TempDir) {
		New-Item -ItemType Directory -Path $TempDir -ErrorAction SilentlyContinue | Out-Null
		[System.IO.Path]::Combine($TempDir, 'main.ps1')
	} else { [System.IO.Path]::GetTempFileName() }
	$Content | Set-Content $TempFile -Encoding UTF8 -NoNewline
	$Params.Add("outputFile", $outputFile)
	$Params.Add("inputFile", $TempFile)
	if ($TempDir) { $Params.TempDir = $TempDir }
	$resourceParamKeys | ForEach-Object {
		if ($resourceParams.ContainsKey($_) -and $resourceParams[$_]) {
			$Params[$_] = $resourceParams[$_]
		}
	}
	if ($iconFile) { $Params.iconFile = $iconFile }
	if ($DllExportList.Length) { $Params.DllExportList = ConvertTo-Json -depth 7 -Compress -InputObject $DllExportList }
	$CallParam = Get-ArgsString $Params

	Write-Debug "Starting WinPowershell ps12exe with parameters: $CallParam"

	powershell -NoProfile -Command "&'$PSScriptRoot\ps12exe.ps1' $CallParam -nested; exit `$LastExitCode" | Write-Host
}
if (!$nested -and ($PSVersionTable.PSEdition -eq "Core") -and $UseWindowsPowerShell -and (Get-Command powershell -ErrorAction Ignore)) {
	UsingWinPowershell $Params
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
	foreach ($a in @("requireAdmin", "supportOS", "longPaths")) {
		if ($Params[$a]) {
			Write-I18n Error "CombinedArg_Virtualize_$a" -Category InvalidArgument
			$global:LastExitCode = 2 # 调用格式错误
			return
		}
	}
}

if (!$configFile) {
	foreach ($a in @("longPaths", "winFormsDPIAware")) {
		if ($Params[$a]) {
			Write-I18n Warning "CombinedArg_NoConfigFile_$a" -Category InvalidArgument
			$configFile = $true
		}
	}
}

# escape escape sequences in version info
$resourceParamKeys | ForEach-Object {
	if ($resourceParams.ContainsKey($_)) {
		$resourceParams[$_] = $resourceParams[$_] -replace "\\", "\\"
	}
}


. $PSScriptRoot\src\AstAnalyze.ps1
$AstAnalyzeResult = AstAnalyze $Ast
Write-Debug "AstAnalyzeResult: $(($AstAnalyzeResult|ConvertTo-Json) -split "\r?\n" -ne '' -join "`n")"
$CommandNames = (Get-Command).Name + (Get-Alias).Name
$FindedCmdlets = @()
$NotFindedCmdlets = @()
$AstAnalyzeResult.UsedNonConstFunctions | ForEach-Object {
	if ($_ -match '\$' -or -not $_) { return }
	if ($CommandNames -notcontains $_) {
		if ($_ -match '^[\w\-_]+$' -and (Get-Command $_ -ErrorAction Ignore)) {
			$FindedCmdlets += $_
		}
		# 跳过成员函数，因为解析Add-Type太过复杂
		elseif (-not $_.Contains(']::')) {
			$NotFindedCmdlets += $_
		}
	}
}
if ($AST.ParamBlock) { $AstAnalyzeResult.IsConst = $false }
$NotFindedTypes = @()
$AstAnalyzeResult.UsedNonConstTypes | ForEach-Object {
	if (!($_ -as [Type])) {
		$NotFindedTypes += $_
	}
}
if ($FindedCmdlets) {
	Write-I18n Warning SomeCmdletsMayNotAvailable $($FindedCmdlets -join '、')
}
if ($NotFindedCmdlets) {
	Write-I18n Warning SomeNotFindedCmdlets $($NotFindedCmdlets -join '、')
}
if ($NotFindedTypes) {
	Write-I18n Warning SomeTypesMayNotAvailable $($NotFindedTypes -join '、')
}
if ($TempDir) {
	New-Item -ItemType Directory -Path $TempDir -ErrorAction SilentlyContinue | Out-Null
}
try {
	. $PSScriptRoot\src\InitCompileThings.ps1
	#_if PSScript
	if (-not $noConsole -and $AstAnalyzeResult.IsConst -and -not $iconFile -and -not $requireAdmin) {
		# TODO: GUI（MassageBoxW）、icon
		Write-I18n Verbose TryingTinySharpCompile
		Write-I18n Host CompilingFile

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
			if ($targetRuntime -eq 'Framework2.0') { $TargetFramework = ".NETFramework,Version=v2.0" }
			if ($PSVersionTable.PSEdition -eq "Core") {
				# unfinished!
				if (!$TargetFramework) {
					$Info = [System.Environment]::Version
					$TargetFramework = ".NETCore,Version=v$($Info.Major).$($Info.Minor)"
				}
				. $PSScriptRoot\src\CodeAnalysisCompiler.ps1
			}
			else {
				if (!$TargetFramework) {
					$TargetFramework = ".NETFramework,Version=v4.7"
				}
				. $PSScriptRoot\src\CodeDomCompiler.ps1
			}
		}
		RollUp
	}
	catch {
		RollUp
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
		if (-not $TinySharpSuccess) {
			& $PSScriptRoot\src\ExeSinker.ps1 $outputFile -removeResources:$(
				$NoResource -and $AstAnalyzeResult.IsConst -and -not $requireAdmin
			) -removeVersionInfo:$($resourceParams.Count -eq 0)
		}
		#_endif
		Write-I18n Host CompiledFileSize $((Get-Item $outputFile).Length)
		Write-I18n Verbose OutputPath $outputFile
		if ($configFile) {
			$configFileForEXE3 | Set-Content ($outputFile + ".config") -Encoding UTF8
			Write-I18n Host ConfigFileCreated
		}
		if ($prepareDebug) {
			$cr.TempFiles | Where-Object { $_ -ilike "*.cs" } | Select-Object -First 1 | ForEach-Object {
				$dstSrc = ([System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($outputFile), [System.IO.Path]::GetFileNameWithoutExtension($outputFile) + ".cs"))
				Write-I18n Host SourceFileCopied $dstSrc
				Copy-Item -Path $_ -Destination $dstSrc -Force
			}
			$cr.TempFiles | Remove-Item -Verbose:$FALSE -Force -ErrorAction SilentlyContinue
		}
	}
}
catch {
	if (Test-Path $outputFile) {
		Remove-Item $outputFile -Verbose:$FALSE
	}
	$_ | Write-Error -ErrorAction Continue
	if ($_.CategoryInfo.Category -eq 'ReadError') {
		$global:LastExitCode = 1 # 读取错误
		return
	}
	if ($PSVersionTable.PSEdition -eq "Core" -and (Get-Command powershell -ErrorAction Ignore)) {
		Write-I18n Host RoslynFailedFallback -ForegroundColor Yellow
		UsingWinPowershell $Params
	}
	#_if PSScript
	elseif (!$GuestMode) {
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
		Write-I18n Host OppsSomethingWentWrong -ForegroundColor Yellow
		if ($versionNow -eq '0.0.0') {} # dev version, do noting
		if ($versionNow -ne $versionOnline) {
			Write-I18n Host TryUpgrade $versionOnline -ForegroundColor Yellow
		}
		else {
			Write-I18n Host EnterToSubmitIssue -ForegroundColor Yellow
			Read-Host | Out-Null
			Start-Process $githubfeedback
		}
	}
	#_endif
}
finally {
	if ($TempTempDir) {
		Remove-Item $TempTempDir -Recurse -Force -ErrorAction SilentlyContinue
	}
}
#_if PSEXE
	#_!! exit $LastExitCode
#_endif
