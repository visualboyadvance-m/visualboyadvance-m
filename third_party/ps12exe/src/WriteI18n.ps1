# 用于 ps12exe 和 exe21sp 的统一 i18n。
# - Set-I18nData：为 Write-I18n 设置查找表（在加载 locale 后调用）。
# - Write-I18n：按键输出到管道；Write-I18n Host|Error|Warning|... Mid [FormatArgs] [...]
# - Write-SymboledI18n：按键输出符号 + 消息；Write-SymboledI18n Symbol MessageKey [SequenceKey] [-MessageFormatArgs] [-SymbolColor] [...]
# 语义包装器：由 $script:SymboledI18nWrapperDefs 生成（表驱动，见下文）。

$script:I18nData = $null

function Set-I18nData {
	[CmdletBinding()]
	param(
		[Parameter(Mandatory = $true)]
		[hashtable]$I18nData
	)
	$script:I18nData = $I18nData
}

function Write-I18n {
	[CmdletBinding()]
	param(
		[Parameter(Position = 0, Mandatory = $true)]
		[ValidateSet('Info', 'Warning', 'Error', 'Debug', 'Verbose', 'Host', 'Output')]
		[string]$PipeLineType,
		[Parameter(Position = 1, Mandatory = $true)]
		[string]$Mid,
		[Parameter(Position = 2)]
		[object[]]$FormatArgs,
		[string]$ErrorId,
		[System.Management.Automation.ErrorCategory]$Category = 'NotSpecified',
		$TargetObject,
		[System.Exception]$Exception,
		[ConsoleColor]$ForegroundColor = $(try { [ConsoleColor]($Host.UI.RawUI.ForegroundColor) } catch { 'White' })
	)
	$formatForF = if ($null -eq $FormatArgs) { @() } else { @($FormatArgs) }
	$template = if ($null -ne $script:I18nData -and $script:I18nData.ContainsKey($Mid)) {
		$script:I18nData[$Mid]
	}
	else {
		"fatal error: No i18n data for $Mid, Rest format args: $($formatForF -join ', ')"
	}
	$value = if ($formatForF.Count -gt 0) { $template -f $formatForF } else { $template }
	if (-not $ForegroundColor) { $ForegroundColor = 'White' }

	switch ($PipeLineType) {
		'Info' { Write-Information $value }
		'Warning' { Write-Warning $value }
		'Error' {
			$errId = if ($PSBoundParameters.ContainsKey('ErrorId')) { $ErrorId } else { $Mid }
			if (-not $Exception) { $Exception = [System.Exception]::new($value) }
			try { $Host.UI.RawUI.ForegroundColor = 'Red' } catch {}
			$Host.UI.WriteErrorLine($value)
			try { $Host.UI.RawUI.ForegroundColor = $ForegroundColor } catch {}
			Write-Error -Exception $Exception -Message $value -Category $Category -ErrorId $errId -TargetObject $TargetObject -ErrorAction SilentlyContinue
		}
		'Debug' { Write-Debug $value }
		'Host' { Write-Host $value -ForegroundColor $ForegroundColor }
		'Output' { $value }
		'Verbose' { Write-Verbose $value }
	}
}

function Write-SymboledI18n {
	[CmdletBinding()]
	param(
		[Parameter(Position = 0, Mandatory = $true)]
		[string]$Symbol,
		[Parameter(Position = 1, Mandatory = $true)]
		[string]$MessageKey,
		[Parameter(Position = 2)]
		[string]$SequenceKey,
		[Parameter(Position = 3)]
		[object[]]$MessageFormatArgs,
		[ConsoleColor]$SymbolColor,
		[ConsoleColor]$MessageColor = 'White',
		[ConsoleColor]$SequenceColor = 'White'
	)
	$msgTemplate = if ($null -ne $script:I18nData -and $script:I18nData.ContainsKey($MessageKey)) { $script:I18nData[$MessageKey] } else { "?$MessageKey?" }
	$message = if ($null -ne $MessageFormatArgs -and $MessageFormatArgs.Count -gt 0) { $msgTemplate -f @($MessageFormatArgs) } else { $msgTemplate }
	$sequence = if ($SequenceKey -and $null -ne $script:I18nData -and $script:I18nData.ContainsKey($SequenceKey)) { $script:I18nData[$SequenceKey] } else { '' }
	Write-Host -NoNewline " "
	Write-Host -ForegroundColor $SymbolColor $Symbol -NoNewline
	Write-Host -ForegroundColor $MessageColor " $message" -NoNewline
	Write-Host -ForegroundColor $SequenceColor " $sequence"
}

# 表：Name、Symbol、SymbolColor、MessageColor、SequenceColor。包装器由此生成（单一实现，无重复）。
$script:SymboledI18nWrapperDefs = @(
	@{ Name = 'Write-SymboledErrorI18n'; Symbol = '[!]'; SymbolColor = 'Red'; MessageColor = 'White'; SequenceColor = 'White' },
	@{ Name = 'Write-SymboledQuestionI18n'; Symbol = '[?]'; SymbolColor = 'Blue'; MessageColor = 'White'; SequenceColor = 'DarkGray' },
	@{ Name = 'Write-SymboledInfoI18n'; Symbol = '[*]'; SymbolColor = 'Green'; MessageColor = 'White'; SequenceColor = 'White' },
	@{ Name = 'Write-SymboledWelcomeI18n'; Symbol = '[*]'; SymbolColor = 'Cyan'; MessageColor = 'White'; SequenceColor = 'White' },
	@{ Name = 'Write-SymboledProgressI18n'; Symbol = '[~]'; SymbolColor = 'Gray'; MessageColor = 'White'; SequenceColor = 'White' },
	@{ Name = 'Write-SymboledExitI18n'; Symbol = '[/]'; SymbolColor = 'Yellow'; MessageColor = 'White'; SequenceColor = 'White' },
	@{ Name = 'Write-SymboledSuccessI18n'; Symbol = '[+]'; SymbolColor = 'Green'; MessageColor = 'White'; SequenceColor = 'White' },
	@{ Name = 'Write-SymboledInvalidI18n'; Symbol = '[-]'; SymbolColor = 'Red'; MessageColor = 'White'; SequenceColor = 'White' }
)

function New-SymboledI18nWrapperScriptBlock($Def) {
	$body = @"
	[CmdletBinding()]
	param(
		[Parameter(Position = 0, Mandatory = `$true)]
		[string]`$MessageKey,
		[Parameter(Position = 1)]
		[string]`$SequenceKey,
		[Parameter(Position = 2)]
		[object[]]`$MessageFormatArgs,
		[ConsoleColor]`$SymbolColor = '$($Def.SymbolColor)',
		[ConsoleColor]`$MessageColor = '$($Def.MessageColor)',
		[ConsoleColor]`$SequenceColor = '$($Def.SequenceColor)'
	)
	Write-SymboledI18n '$($Def.Symbol -replace "'", "''")' `$MessageKey `$SequenceKey -MessageFormatArgs `$MessageFormatArgs -SymbolColor `$SymbolColor -MessageColor `$MessageColor -SequenceColor `$SequenceColor
"@
	$code = "function $($Def.Name) { $body }"
	$errors = $null
	$result = [System.Management.Automation.Language.Parser]::ParseInput($code, $null, [ref]$null, [ref]$errors)
	$errors | ForEach-Object { Write-Warning $_ }
	$result.GetScriptBlock()
}

foreach ($def in $script:SymboledI18nWrapperDefs) {
	. (New-SymboledI18nWrapperScriptBlock $def)
}
