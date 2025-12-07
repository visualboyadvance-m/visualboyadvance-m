function GetAssembly($name, $otherinfo) {
	$n = New-Object System.Reflection.AssemblyName(@($name, $otherinfo) -ne $null -join ",")
	try {
		[System.AppDomain]::CurrentDomain.Load($n).Location
	} catch {
		$Error.Remove(0)
	}
}
$referenceAssembies = if ($targetRuntime -eq 'Framework2.0') {
	#_if PSScript
		powershell -version 2.0 -NoProfile -OutputFormat xml -file $PSScriptRoot/RuntimePwsh2.0/RefDlls.ps1 $(if($noConsole){'-noConsole'})
	#_else
		#_include_as_value Pwsh2RefDllsGetterCodeStr $PSScriptRoot/RuntimePwsh2.0/RefDlls.ps1
		#_!! powershell -version 2.0 -NoProfile -OutputFormat xml -Command "&{$Pwsh2RefDllsGetterCodeStr}$(if($noConsole){' -noConsole'})"
	#_endif
}
else {
	# 绝不要直接使用 System.Private.CoreLib.dll，因为它是netlib的内部实现，而不是公共API
	# [int].Assembly.Location 等基础类型的程序集也是它。
	GetAssembly "mscorlib"
	if ($PSVersionTable.PSEdition -eq "Core") { GetAssembly "System.Runtime" }
	GetAssembly "System.Management.Automation"

	# If noConsole is true, add System.Windows.Forms.dll and System.Drawing.dll to the reference assemblies
	if ($noConsole) {
		GetAssembly "System.Windows.Forms" $(if ($PSVersionTable.PSEdition -ne "Core") { "Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089" })
		GetAssembly "System.Drawing" $(if ($PSVersionTable.PSEdition -ne "Core") { "Version=4.0.0.0, Culture=neutral, PublicKeyToken=b03f5f7f11d50a3a" })
	}
	elseif ($PSVersionTable.PSEdition -eq "Core") {
		GetAssembly "System.Console"
		GetAssembly "Microsoft.PowerShell.ConsoleHost"
	}

	# If in winpwsh, add System.Core.dll to the reference assemblies
	if ($PSVersionTable.PSEdition -ne "Core") {
		GetAssembly "System.Core" "Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089"
		"System.dll" # some furking magic
	}
}

. $PSScriptRoot\BuildFrame.ps1

[string[]]$Constants = @()

$Constants += $threadingModel
if ($lcid) { $Constants += "culture" }
if ($noError) { $Constants += "noError" }
if ($noConsole) { $Constants += "noConsole" }
if ($noOutput) { $Constants += "noOutput" }
if ($resourceParams.version) { $Constants += "version" }
if ($resourceParams.Count) { $Constants += "Resources" }
if ($credentialGUI) { $Constants += "credentialGUI" }
if ($noVisualStyles) { $Constants += "noVisualStyles" }
if ($exitOnCancel) { $Constants += "exitOnCancel" }
if ($UNICODEEncoding) { $Constants += "UNICODEEncoding" }
if ($winFormsDPIAware) { $Constants += "winFormsDPIAware" }
if ($targetRuntime -eq 'Framework2.0') { $Constants += "Pwsh20" }

if (-not $TempDir) {
	$TempDir = $TempTempDir = [System.IO.Path]::GetTempPath() + [System.IO.Path]::GetRandomFileName()
	New-Item -Path $TempTempDir -ItemType Directory | Out-Null
}
$TempDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($TempDir)

$Content | Set-Content $TempDir\main.ps1 -Encoding UTF8 -NoNewline
if ($iconFile -match "^(https?|ftp)://") {
	try {
		if ($GuestMode) {
			if ((Invoke-WebRequest $iconFile -Method Head -ErrorAction SilentlyContinue).Headers.'Content-Length' -gt 1mb) {
				Write-I18n Error GuestModeIconFileTooLarge $iconFile -Category LimitsExceeded
				throw
			}
			if ($File -match "^ftp://") {
				Write-I18n Error GuestModeFtpNotSupported -Category ReadError
				throw
			}
		}
		Invoke-WebRequest -ErrorAction Stop -Uri $iconFile -OutFile $TempDir\icon.ico
	}
	catch {
		Write-I18n Error IconFileNotFound $iconFile -Category ReadError
		throw
	}
	$iconFile = "$TempDir\icon.ico"
}
elseif ($iconFile) {
	# retrieve absolute path independent if path is given relative oder absolute
	$iconFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($iconFile)

	if (!(Test-Path $iconFile -PathType Leaf)) {
		Write-I18n Error IconFileNotFound $iconFile -Category ReadError
		throw
	}
}
