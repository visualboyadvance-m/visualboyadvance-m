# Reorder locale .ps1 files to match zh-CN.ps1 top-level key order.
# Usage: .\reorder_locale.ps1 [path to locale folder]
# Default: script's directory

$ErrorActionPreference = 'Stop'
$LocaleDir = if ($args[0]) { $args[0] } else { $PSScriptRoot }

# Reference key order (from zh-CN.ps1)
$RefKeyOrder = @(
	'LangName', 'LangID',
	'CompileTitle', 'OpenInGUI', 'GUICfgFileDesc',
	'VSCodeExtensionInstalling', 'VSCodeExtensionInstallFailed',
	'VSCodeExtensionUninstalling', 'VSCodeExtensionUninstallFailed',
	'ErrorHead', 'CompileResult', 'DefaultResult', 'AskSaveCfg', 'AskSaveCfgTitle', 'CfgFileLabelHead',
	'ServerStarted', 'ServerStopped', 'ServerStartFailed', 'TryRunAsRoot', 'ServerListening', 'ExitServerTip',
	'ConsoleHelpData', 'GUIHelpData', 'SetContextMenuHelpData', 'WebServerHelpData', 'exe21spHelpData',
	'CompilingI18nData', 'WebServerI18nData', 'InteractI18nData', 'exe21spInteractI18nData', 'exe21spI18nData'
)
$RefKeySet = [System.Collections.Generic.HashSet[string]]::new([string[]]$RefKeyOrder)

function Get-TopLevelKeyBlocks {
	param([string]$Path)
	$lines = [System.IO.File]::ReadAllLines($Path, [System.Text.Encoding]::UTF8)
	$keyStarts = [System.Collections.ArrayList]::new()  # (lineIndex, keyName)
	for ($i = 0; $i -lt $lines.Count; $i++) {
		$line = $lines[$i]
		if (-not $line.StartsWith("`t") -or $line.StartsWith("`t`t")) { continue }
		if ($line -match '^\t([A-Za-z][A-Za-z0-9]*)\s*=') {
			$keyName = $Matches[1]
			if ($RefKeySet.Contains($keyName)) {
				$keyStarts.Add([PSCustomObject]@{ LineIndex = $i; KeyName = $keyName }) | Out-Null
			}
		}
	}
	$keyStarts = $keyStarts | Sort-Object LineIndex
	$blocks = @{}
	for ($j = 0; $j -lt $keyStarts.Count; $j++) {
		$start = $keyStarts[$j].LineIndex
		$end = if ($j + 1 -lt $keyStarts.Count) { $keyStarts[$j + 1].LineIndex } else { $lines.Count }
		$keyName = $keyStarts[$j].KeyName
		$blockLines = $lines[$start..($end - 1)]
		$blocks[$keyName] = $blockLines -join "`n"
	}
	$headerLines = @()
	if ($keyStarts.Count -gt 0) {
		$firstKeyLine = $keyStarts[0].LineIndex
		for ($i = 0; $i -lt $firstKeyLine; $i++) { $headerLines += $lines[$i] }
	}
	return @{ Header = ($headerLines -join "`n"); Blocks = $blocks }
}

function Write-ReorderedFile {
	param([string]$Path, [hashtable]$Parsed)
	$sb = [System.Text.StringBuilder]::new()
	if ($Parsed.Header) {
		[void]$sb.AppendLine($Parsed.Header.TrimEnd())
	}
	foreach ($key in $RefKeyOrder) {
		if ($Parsed.Blocks.ContainsKey($key)) {
			[void]$sb.Append($Parsed.Blocks[$key])
			if (-not $Parsed.Blocks[$key].EndsWith("`n")) { [void]$sb.AppendLine() }
		}
	}
	$content = $sb.ToString().TrimEnd()
	if (-not $content.EndsWith("`n")) { $content += "`n" }
	[System.IO.File]::WriteAllText($Path, $content, [System.Text.UTF8Encoding]::new($true))  # $true = with BOM
}

$ToProcess = Get-ChildItem -LiteralPath $LocaleDir -Filter '*.ps1' -File |
Where-Object { $_.Name -match '^[a-z]{2}-[A-Z]{2}\.ps1$' -and $_.Name -ne 'zh-CN.ps1' }

foreach ($f in $ToProcess) {
	$fullName = $f.FullName
	try {
		$parsed = Get-TopLevelKeyBlocks -Path $fullName
		Write-ReorderedFile -Path $fullName -Parsed $parsed
		Write-Host "Reordered: $($f.Name)"
	}
	catch {
		Write-Warning "Skip $($f.Name): $_"
	}
}
Write-Host "Done."
