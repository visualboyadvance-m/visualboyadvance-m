function ShortPath($Path) {
	$Path = $Path.Replace($env:USERPROFILE, '~').Replace($PWD, '.').Replace('/./', '/')
	$Path
}
function BaseReadFile($File, $Encoding = 'UTF8') {
	$Content = try {
		if ($File -match "^(https?|ftp)://") {
			if ($GuestMode) {
				if ((Invoke-WebRequest $File -Method Head -ErrorAction SilentlyContinue).Headers.'Content-Length' -gt 1mb) {
					Write-I18n Error GuestModeFileTooLarge $File -Category LimitsExceeded
					throw
				}
				if ($File -match "^ftp://") {
					Write-I18n Error GuestModeFtpNotSupported -Category ReadError
					throw
				}
			}
			$result = (Invoke-WebRequest -Uri $File).Content
			if ($Encoding -ne 'byte') { $result = $result -replace '^[^\u0000-\u007F]+', '' }
			$result
		}
		elseif (-not $GuestMode) {
			# -Encoding Byte 似乎在某些机器中不起作用：https://github.com/steve02081504/ps12exe/issues/18
			if ($Encoding -ne 'Byte') {
				Get-Content -LiteralPath $File -Encoding $Encoding -Raw
			}
			else {
				[System.IO.File]::ReadAllBytes($File)
			}
		}
	}
	catch {
		Write-Verbose $_
	}
	if (-not $Content) {
		Write-I18n Error ReadFileFailed $(ShortPath $File) -Category ReadError
		throw
	}
	Write-I18n Host ReadingFile @([System.IO.Path]::GetFileName($File), $Content.Length)
	$Content
}
function ReadScriptFile($File) {
	$Content = BaseReadFile $File
	$Content = $Content -join "`n" -split '\r?\n'
	$FileName = [System.IO.Path]::GetFileName($File)
	Write-I18n Verbose ReadingScriptDone $FileName
	Preprocessor $Content $File
	Write-I18n Verbose PreprocessScriptDone $FileName
}
. $PSScriptRoot\predicate.ps1
. $PSScriptRoot\PSObjectToString.ps1
function Preprocessor($Content, $FilePath) {
	$Result = @()
	$requiredModules = @()
	$requireFlag = $False
	# 处理#_if <PSEXE/PSScript>、#_else、#_endif
	for ($index = 0; $index -lt $Content.Count; $index++) {
		$Line = $Content[$index]
		if ($Line -match "^\s*#_if\s+(?<condition>\S+)\s*(?!#.*)") {
			$condition = $Matches["condition"]
			$condition = switch ($condition) {
				'PSEXE' { $TRUE }
				'PSScript' { $False }
				default { Write-I18n Error PreprocessUnknownIfCondition $condition -Category InvalidData; $False }
			}
			while ($index -lt $Content.Count) {
				$index++
				$Line = $Content[$index]
				if ($Line -match "^\s*#_endif\s*(?!#.*)") {
					break
				}
				if ($condition) { $Result += $Line }
				if ($Line -match "^\s*#_else\s*(?!#.*)") {
					$condition = -not $condition
				}
			}
			if ($Line -notmatch "^\s*#_endif\s*(?!#.*)") {
				Write-I18n Error PreprocessMissingEndif -Category SyntaxError
				return
			}
		}
		else {
			$Result += $Line
		}
	}
	$ScriptRoot = $FilePath.Substring(0, $FilePath.LastIndexOfAny(@('\', '/')))
	function GetIncludeFilePath($rest) {
		if ($rest -match "((\'[^\']*\')+)\s*(?!#.*)") {
			$file = $Matches[1]
			$file = $file.Substring(1, $file.Length - 2).Replace("''", "'")
		}
		elseif ($rest -match '((\"[^\"]*\")+)\s*(?!#.*)') {
			$file = $Matches[1]
			$file = $file.Substring(1, $file.Length - 2).Replace('""', '"')
		}
		else { $file = $rest }
		$file = $file.Replace('$PSScriptRoot', $ScriptRoot)
		# 若是相对路径，则转换为基于$FilePath的绝对路径
		if ($file -notmatch "^[a-zA-Z]:" -and $file -notmatch "^(https|ftp)://") {
			$file = "$ScriptRoot/$file"
		}
		$file
	}
	$Content = $Result |
	# 处理#_pragma
	ForEach-Object {
		$_ # 对于#_pragma，我们不在预处理时移除它：考虑到它可能被用于$PSEXEscript中
		if ($_ -match "^\s*#_pragma\s+(?<pragmaname>[a-zA-Z_][a-zA-Z_0-9]+)\s*(?!#.*)$") {
			$pragmaname = $Matches["pragmaname"]
			$value = $true
			if ($pragmaname.StartsWith("no")) {
				$pragmaname = $pragmaname.Substring(2)
				$value = $false
			}
			if ($ParamList[$pragmaname].ParameterType -eq [Switch]) {
				$Params[$pragmaname] = [Switch]$value
				return
			}
			if ($ParamList["no$pragmaname"].ParameterType -eq [Switch]) {
				$Params["no$pragmaname"] = [Switch]-not $value
				return
			}
			Write-I18n Warning UnknownPragma $($Matches["pragmaname"])
		}
		elseif ($_ -match "^\s*#_pragma\s+(?<pragmaname>[a-zA-Z_][a-zA-Z_0-9]+)\s+(?<rest>.+)\s*$") {
			$pragmaname = $Matches["pragmaname"]
			$value = $Matches["rest"]
			if ($ParamList[$pragmaname].ParameterType -eq [Switch] -or $ParamList["no$pragmaname"].ParameterType -eq [Switch]) {
				if ($value.IndexOf("#") -ge 0) {
					$value = $value.Substring(0, $value.IndexOf("#"))
				}
				$value = $value.Trim()
				if (IsEnable $value) {
					$value = $true
				}
				elseif (IsDisable $value) {
					$value = $false
				}
				else {
					Write-I18n Warning UnknownPragmaBoolValue $value
					return
				}
				if ($pragmaname.StartsWith("no")) {
					$pragmaname = $pragmaname.Substring(2)
					$value = -not $value
				}
				if ($ParamList[$pragmaname].ParameterType -eq [Switch]) {
					$Params[$pragmaname] = [Switch]$value
				}
				if ($ParamList["no$pragmaname"].ParameterType -eq [Switch]) {
					$Params["no$pragmaname"] = [Switch]-not $value
				}
			}
			elseif ($ParamList[$pragmaname].ParameterType -eq [string] -or $ParamList[$pragmaname + "File"].ParameterType -eq [string]) {
				if ($value -match '^\"(?<value>[^\"]*)\"\s*(?!#.*)') {
					$value = $Matches["value"].Replace('$PSScriptRoot', $ScriptRoot)
				}
				elseif ($value -match "^\'(?<value>[^\']*)\'\s*(?!#.*)") {
					$value = $Matches["value"]
				}
				else {
					$value = $value.Replace('$PSScriptRoot', $ScriptRoot)
				}
				if ($ParamList[$pragmaname].ParameterType -eq [string]) {
					$Params[$pragmaname] = $value
				}
				elseif ($ParamList[$pragmaname + "File"].ParameterType -eq [string]) {
					$Params[$pragmaname + "File"] = $value
				}
			}
			elseif ($ParamList[$pragmaname].ParameterType) {
				Write-I18n Warning UnknownPragmaBadParameterType $($pragmaname, $ParamList[$pragmaname].ParameterType)
			}
			else {
				Write-I18n Warning UnknownPragma $pragmaname
			}
		}
	} |
	# 处理#_require
	ForEach-Object {
		if ($_ -match "^(\s*)#_require\s+(?<moduleList>[^#]+)\s*(?!#.*)") {
			$requiredModules += $Matches["moduleList"].Split(', |;、　') | Where-Object { $_.Trim('"''') -ne '' }
			if (!$requireFlag) {
				$requireFlag = $true
				[bigint]::Parse('72')
			}
		}
		else { $_ }
	} |
	# 处理#_DllExport
	ForEach-Object {
		$_ # 对于#_DllExport，我们不在预处理时移除它：考虑到它可能被用于$PSEXEscript中
		if ($_ -match "^(\s*)(#_DllExport\s+(?<callsign>[^#\(]+)\((?<callsignParams>[^#\)]*)\))\s*(?!#.*)") {
			$callsign = $Matches["callsign"] -split ' ' | ForEach-Object { $_.Trim() }
			$callsignParams = $Matches["callsignParams"] -split ',' | ForEach-Object { $_.Trim() }
		}
		elseif ($_ -match "^(\s*)(#_DllExport\s+(?<callsign>[^#\(]+))\s*(?!#.*)") {
			$callsign = $Matches["callsign"] -split ' ' | ForEach-Object { $_.Trim() }
			$callsignParams = @()
		}
		if ($callsign) {
			if (!$callsign[1]) { $callsign = @('void', $callsign[0]) }
			$DllExportData = @{
				returntype = $callsign[0]
				funcname   = $callsign[1]
				params     = @()
			}
			foreach ($param in $callsignParams) {
				$paramData = $param -split ' ' | ForEach-Object { $_.Trim() }
				if ($paramData.Count -eq 1) {
					Write-I18n Warning DllExportDelNoneTypeArg $($Matches[2], $paramData[0])
					$paramData = @('string', $paramData[0])
				}
				$DllExportData.params += @{ name = $paramData[1]; type = $paramData[0] }
			}
			$DllExportList.Add($DllExportData) | Out-Null
			Write-I18n Warning DllExportUsing
			Write-Debug "$($Matches[2]): FuncSign: [$($DllExportData.returntype)]$($DllExportData.funcname)($(($DllExportData.params|ForEach-Object{ $_.type + ' ' + $_.name }) -join ', '))"
		}
	} |
	# 处理#_!!<line>、#_balus <?exitcode>
	ForEach-Object {
		if ($_ -match "^(\s*)#_!!(?<line>.*)") {
			$Matches[1] + $Matches["line"]
		}
		elseif ($_ -match "^(\s*)#_balus\s+(?<exitcode>\`$?\w+)") {
			'Start-Process powershell @("-NoProfile";"-c";"sleep 1;rm `"$PSEXEpath`"") -WindowStyle hidden;exit ' + $Matches["exitcode"]
		}
		elseif ($_ -match "^(\s*)#_balus") {
			'Start-Process powershell @("-NoProfile";"-c";"sleep 1;rm `"$PSEXEpath`"") -WindowStyle hidden;exit 0'
		}
		else { $_ }
	} |
	# 处理#_include <file>、#_include_as_value <valuename> <file>、#_include_as_(base64|bytes) <valuename> <file>
	ForEach-Object {
		if ($_ -match "^\s*#_include\s+(?<rest>.+)\s*") {
			$file = GetIncludeFilePath $Matches["rest"]
			ReadScriptFile $file
		}
		elseif ($_ -match "^\s*#_include_as_value\s+(?<valuename>[a-zA-Z_][a-zA-Z_0-9]+)\s+(?<rest>.+)\s*") {
			$valuename = $Matches["valuename"]
			$file = GetIncludeFilePath $Matches["rest"]
			$IncludeContent = BaseReadFile $file
			$IncludeContent = $IncludeContent -join "`n"
			$IncludeContent = $IncludeContent.Replace("'", "''")
			"`$$valuename = '$IncludeContent'"
		}
		elseif ($_ -match "^\s*#_include_as_(?<type>base64|bytes)\s+(?<valuename>[a-zA-Z_][a-zA-Z_0-9]+)\s+(?<rest>.+)\s*") {
			$valuename = $Matches["valuename"]
			$file = GetIncludeFilePath $Matches["rest"]
			$IncludeContent = BaseReadFile $file Byte
			$IncludeContent = [System.Convert]::ToBase64String($IncludeContent)
			if ($Matches["type"] -eq 'bytes') {
				"`$$valuename = [System.Convert]::FromBase64String('$IncludeContent')"
			}
			else {
				"`$$valuename = '$IncludeContent'"
			}
		}
		else {
			if ($_ -match '^\s*(?<assign>\$\w+\s*\=\s*)?(?<callopt>\.|&)\s*(?<rest>(\"|)\$PSScriptRoot.+)\s*') {
				$assign = $Matches["assign"]
				$rest = $Matches["rest"]
				$callopt = $Matches["callopt"]
				if ($rest -match "^(?<file>\`"[^\`"]*\`")(?<args>.+)") {
					$callargs = $Matches["args"]
					$rest = $Matches["file"]
				}
				elseif ($rest.IndexOf(' ') -gt 0) {
					$callargs = $rest.Substring($rest.IndexOf(' ') + 1)
					$rest = $rest.Substring(0, $rest.IndexOf(' '))
				}
				$file = GetIncludeFilePath $rest
				if (Test-Path $file -PathType Leaf -ErrorAction Ignore) {
					if (!$callargs -and !$assign -and $callopt -eq '.') {
						return ReadScriptFile $file
					}
					return @("$assign$callopt{", $(ReadScriptFile $file), "}$callargs")
				}
			}
			$_
		}
	}
	$NuGetIniter = "try{Import-PackageProvider NuGet}catch{Install-PackageProvider NuGet -Scope CurrentUser -Force -ea Ignore;Import-PackageProvider NuGet -ea Ignore}"
	$LoadModuleScript = if ($requiredModules.Count -gt 1) {
		(PSObjectToString $requiredModules -OneLine) + '|%{if(!(gmo $_ -ListAvailable -ea SilentlyContinue)){' + $NuGetIniter + ';Install-Module $_ -Scope CurrentUser -Force -ea Stop}}'
	}
	elseif ($requiredModules.Count -eq 1) {
		"if(!(gmo $requiredModules -ListAvailable -ea SilentlyContinue)){$NuGetIniter;Install-Module $requiredModules -Scope CurrentUser -Force -ea Stop}"
	}
	$LoadModuleScript = $LoadModuleScript -join "`n"
	if ($LoadModuleScript) {
		$Content = $Content | ForEach-Object {
			# 在第一次#_require的前方加入$LoadModuleScript
			if ($_ -is [bigint]) {
				$LoadModuleScript
			}
			else { $_ }
		}
	}
	$Content -join "`n"
}
