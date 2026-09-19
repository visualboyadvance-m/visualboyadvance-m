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
. $PSScriptRoot\AstAnalyze.ps1
function Preprocessor($Content, $FilePath) {
	$Result = @()
	$requiredModules = @()
	$requireFlag = $False
	# here-string 函数体与完全处于块注释内的行不参与 `#_!!` 使用检查（与 VS Code 插件的 computeSkipMask 一致）。
	$OpaqueLines = [bool[]]::new($Content.Count)
	$HereTerminator = $null
	$InBlockComment = $false
	for ($skipIndex = 0; $skipIndex -lt $Content.Count; $skipIndex++) {
		$skipLine = [string]$Content[$skipIndex]
		$skipTrimmed = $skipLine.Trim()
		if ($HereTerminator) {
			$OpaqueLines[$skipIndex] = $true
			if ($skipTrimmed.StartsWith($HereTerminator)) { $HereTerminator = $null }
			continue
		}
		if ($InBlockComment) {
			$OpaqueLines[$skipIndex] = $true
			if ($skipLine.Contains('#>')) { $InBlockComment = $false }
			continue
		}
		if ($skipLine -match '@(["''])\s*$') {
			$HereTerminator = $Matches[1] + '@'
			continue
		}
		$rest = $skipLine
		$hasCode = $false
		while ($true) {
			$open = $rest.IndexOf('<#')
			if ($open -lt 0) {
				if ($rest.Trim() -ne '') { $hasCode = $true }
				break
			}
			if ($rest.Substring(0, $open).Trim() -ne '') { $hasCode = $true }
			$close = $rest.IndexOf('#>', $open + 2)
			if ($close -lt 0) { $InBlockComment = $true; break }
			$rest = $rest.Substring($close + 2)
		}
		if (-not $hasCode) { $OpaqueLines[$skipIndex] = $true }
	}
	# 处理#_if <PSEXE/PSScript>、#_else、#_endif（支持嵌套：只有栈上所有分支都为真时才输出）
	$conditionStack = [System.Collections.Generic.List[hashtable]]::new()
	for ($index = 0; $index -lt $Content.Count; $index++) {
		$Line = $Content[$index]
		if ($Line -match "^\s*#_if\s+(?<condition>\S+)\s*(?!#.*)") {
			$conditionName = $Matches["condition"]
			# 外层 #_if 已经决定了内层条件，内层必有一支是死代码
			if ($conditionStack.Count -gt 0) {
				Write-I18n Warning PreprocessNestedIfDeadCode @($conditionName, $conditionStack[$conditionStack.Count - 1].Name)
			}
			$conditionStack.Add(@{
				Name   = $conditionName
				Active = switch ($conditionName) {
					'PSEXE' { $TRUE }
					'PSScript' { $False }
					default { Write-I18n Error PreprocessUnknownIfCondition $conditionName -Category InvalidData; $False }
				}
			})
		}
		elseif ($Line -match "^\s*#_else\s*(?!#.*)") {
			if ($conditionStack.Count -eq 0) { $Result += $Line; continue }
			$top = $conditionStack[$conditionStack.Count - 1]
			$top.Active = -not $top.Active
		}
		elseif ($Line -match "^\s*#_endif\s*(?!#.*)") {
			if ($conditionStack.Count -eq 0) { $Result += $Line; continue }
			$conditionStack.RemoveAt($conditionStack.Count - 1)
		}
		else {
			# `#_if PSEXE` 分支只会编译进 EXE：直接运行脚本时该分支并未被注释掉，因此其中的普通代码也必须带 `#_!!`。
			# 反之 `#_if PSScript` 分支只在直接运行时存在：其中的 `#_!!` 会让它在直接运行时变成注释，属于误用。
			if ($conditionStack.Count -gt 0 -and -not $OpaqueLines[$index]) {
				$knownBranch = $true
				foreach ($conditionEntry in $conditionStack) {
					if ($conditionEntry.Name -ne 'PSEXE' -and $conditionEntry.Name -ne 'PSScript') { $knownBranch = $false; break }
				}
				if ($knownBranch) {
					# 一行只有在其所有外层分支都于编译期被选中时才会进入 EXE，否则只在直接运行时存在。
					$selectedInExe = -not ($conditionStack.Active -contains $false)
					$trimmedLine = ([string]$Line).TrimStart()
					if ($selectedInExe) {
						if ($trimmedLine -ne '' -and -not $trimmedLine.StartsWith('#')) {
							Write-I18n Warning PreprocessPsexeBranchCode
						}
					}
					elseif ($trimmedLine.StartsWith('#_!!')) {
						Write-I18n Warning PreprocessPsscriptBranchBang
					}
				}
			}
			if (-not ($conditionStack.Active -contains $false)) {
				$Result += $Line
			}
		}
	}
	if ($conditionStack.Count -ne 0) {
		Write-I18n Error PreprocessMissingEndif -Category SyntaxError
		return
	}
	# 被处理文件所在目录，用于解析 #_include 里的 $PSScriptRoot。本地路径取绝对目录，避免相对输入路径被反复前缀。
	$ScriptRoot = if ($FilePath -match "^(https?|ftp)://") {
		$FilePath -replace '/[^/]*$', ''
	}
	else {
		[System.IO.Path]::GetDirectoryName($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FilePath))
	}
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
		# 仍是相对路径（未引用 $PSScriptRoot）时，基于被处理文件所在目录解析。
		if (-not [System.IO.Path]::IsPathRooted($file) -and $file -notmatch "^(https?|ftp)://") {
			$file = "$ScriptRoot/$file"
		}
		$file
	}
	# 校验 pragma 子表达式是否只使用白名单内的 path 相关命令/变量。返回：$true 表示安全；否则返回一个含具体原因的字符串数组。
	function Test-PragmaExpressionSafe([string]$Expr) {
		$PragmaSafeCommands = @('gcm', 'get-command', 'join-path', 'split-path', 'resolve-path', 'convert-path', 'get-item', 'test-path', 'get-childitem')
		if (-not $GuestMode) { $PragmaSafeCommands += 'get-content' }
		$PragmaSafeVariables = @('PSScriptRoot', 'ScriptRoot', 'HOME', 'PWD', 'PSCommandPath')
		$Errors = [System.Collections.Generic.List[string]]::new()
		$Tokens = $null
		$ParseErrors = $null
		$Ast = [System.Management.Automation.Language.Parser]::ParseInput($Expr, [ref]$Tokens, [ref]$ParseErrors)
		if ($ParseErrors) {
			$Errors.Add("parse: $($ParseErrors[0].Message)")
			return $Errors.ToArray()
		}
		$Result = AstAnalyze $Ast
		if ($Result.ImporttedExternalScripts) { $Errors.Add('external script invocation') }
		if ($Result.UsedNonConstTypes) { $Errors.Add("type members: $($Result.UsedNonConstTypes -join ', ')") }
		if ($Result.UsedInstanceMethods) { $Errors.Add("instance methods: $($Result.UsedInstanceMethods -join ', ')") }
		foreach ($f in $Result.UsedNonConstFunctions) {
			if ($PragmaSafeCommands -notcontains $f.ToLowerInvariant()) { $Errors.Add("command: $f") }
		}
		foreach ($v in $Result.UsedNonConstVariables) {
			if ($PragmaSafeVariables -notcontains $v) { $Errors.Add("variable: `$$v") }
		}
		return $Errors.ToArray()
	}
	# 校验通过后对 pragma 值进行 PowerShell 字符串展开（求值 $(...) 子表达式）。展开时临时把 $PSScriptRoot 指向被编译脚本所在目录，使子表达式内能直接引用它。
	function Expand-PragmaExpression([string]$Value, [string]$PragmaName) {
		$Unsafe = Test-PragmaExpressionSafe ('"' + $Value + '"')
		if ($Unsafe) {
			Write-I18n Error PragmaUnsafeExpression $($PragmaName, ($Unsafe -join '; ')) -Category ReadError
			throw
		}
		$PSScriptRootBackup = $PSScriptRoot
		$PSScriptRoot = $ScriptRoot
		try {
			return $ExecutionContext.InvokeCommand.ExpandString($Value)
		}
		finally {
			$PSScriptRoot = $PSScriptRootBackup
		}
	}
	# 解析 pragma 的字符串值：支持双/单引号、$() 白名单子表达式展开、$PSScriptRoot 替换。
	function ConvertFrom-PragmaStringValue([string]$Value, [string]$PragmaName) {
		if ($Value -match '^\"(?<value>[^\"]*)\"\s*(?!#.*)') {
			$Value = $Matches["value"]
			if ($Value -match '\$\(') {
				$Value = Expand-PragmaExpression $Value $PragmaName
			}
			else {
				$Value = $Value.Replace('$PSScriptRoot', $ScriptRoot)
			}
		}
		elseif ($Value -match "^\'(?<value>[^\']*)\'\s*(?!#.*)") {
			$Value = $Matches["value"]
		}
		else {
			if ($Value -match '\$\(') {
				$Value = Expand-PragmaExpression $Value $PragmaName
			}
			else {
				$Value = $Value.Replace('$PSScriptRoot', $ScriptRoot)
			}
		}
		return $Value
	}
	# 嵌套设置：a.b 或 a.b.c...，根参数必须是哈希表。字符串值会做同样的 pragma 值转换。
	function Set-NestedPragma([string]$PragmaName, $Value) {
		$segments = $PragmaName -split '\.'
		$root = $segments[0]
		$rootType = $ParamList[$root].ParameterType
		if ($rootType -ne [hashtable] -and $rootType -ne [System.Collections.IDictionary]) {
			Write-I18n Warning UnknownPragma $PragmaName
			return $false
		}
		if ($Value -is [string]) {
			$Value = ConvertFrom-PragmaStringValue $Value $PragmaName
			if ($root -eq 'Signing' -and $segments[-1] -eq 'Password') {
				$Value = ConvertTo-SecureString -String $Value -AsPlainText -Force
			}
		}
		$target = if ($Params.ContainsKey($root) -and $Params[$root] -is [hashtable]) { $Params[$root] } else { @{} }
		$cursor = $target
		for ($i = 1; $i -lt $segments.Count - 1; $i++) {
			if (-not ($cursor[$segments[$i]] -is [hashtable])) { $cursor[$segments[$i]] = @{} }
			$cursor = $cursor[$segments[$i]]
		}
		$cursor[$segments[-1]] = $Value
		$Params[$root] = $target
		return $true
	}
	$Content = $Result |
	# 处理#_pragma
	ForEach-Object {
		$_ # 对于#_pragma，我们不在预处理时移除它：考虑到它可能被用于$PSEXEscript中
		if ($_ -match "^\s*#_pragma\s+(?<pragmaname>[a-zA-Z_][a-zA-Z_0-9]*(?:\.[a-zA-Z_][a-zA-Z_0-9]*)*)\s*(?!#.*)$") {
			$pragmaname = $Matches["pragmaname"]
			$value = $true
			if ($pragmaname.Contains('.')) {
				# 无值嵌套 pragma（如 #_pragma App.Windowed）等同于打开对应键
				Set-NestedPragma $pragmaname $true | Out-Null
				return
			}
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
		elseif ($_ -match "^\s*#_pragma\s+(?<pragmaname>[a-zA-Z_][a-zA-Z_0-9]*(?:\.[a-zA-Z_][a-zA-Z_0-9]*)*)\s+(?<rest>.+)\s*$") {
			$pragmaname = $Matches["pragmaname"]
			if ($ConstEvalPragmas -contains $pragmaname) { return }
			$value = $Matches["rest"]
			if ($pragmaname.Contains('.')) {
				Set-NestedPragma $pragmaname $value | Out-Null
				return
			}
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
				$value = ConvertFrom-PragmaStringValue $value $pragmaname
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
			'Start-Process powershell @("-NoProfile";"-c";"sleep 1;rm `"$PSCommandPath`"") -WindowStyle hidden;exit ' + $Matches["exitcode"]
		}
		elseif ($_ -match "^(\s*)#_balus") {
			'Start-Process powershell @("-NoProfile";"-c";"sleep 1;rm `"$PSCommandPath`"") -WindowStyle hidden;exit 0'
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
