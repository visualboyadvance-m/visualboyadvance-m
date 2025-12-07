param(
	[scriptblock]$CheckLocaleData = {
		$null -ne $Script:LocalizeData
	},
	[scriptblock]$FaildLoadLocaleData = {
		param (
			[string]$Localize
		)
		Write-Warning "Failed to load locale data $Localize`nSee $LocalizeDir/README.md for how to add custom locale."
	},
	[scriptblock]$LoadLocaleData = {
		param (
			[string]$Localize
		)
		$file = "$LocalizeDir\$Localize.ps1"
		if (Test-Path $file) { $Script:LocalizeData = try { &$file } catch { $null } }
	},
	[string]$Localize
)

if (!$Localize) {
	# 本机语言
	$Localize = $env:LANG
	if (!$Localize) { $Localize = $env:LANGUAGE }
	if (!$Localize) { $Localize = $env:LC_ALL }
	if (!$Localize -and (Get-Command locale -ErrorAction Ignore)) {
		$Localize = try {
			&locale -uU
		} catch { $null }
	}
	if ($Localize) {
		$Localize = $Localize.Split('.')[0].Replace('_', '-')
	}
	else {
		$Localize = (Get-Culture).Name
	}
}

$LocalizeDir = "$PSScriptRoot/locale"

&$LoadLocaleData $Localize
if (!(&$CheckLocaleData)) {
	$LocalizeList = Get-ChildItem $LocalizeDir | Where-Object { $_.Name -like '*.fbs' } | ForEach-Object { $_.BaseName }
	$LocalizeHead = $Localize.Split('-')[0]
	$SimilarLocalize = $LocalizeList | Where-Object { $_.StartsWith($LocalizeHead) }
	if ($LocalizeHead -ne $Localize) { &$FaildLoadLocaleData $Localize }
	foreach ($Localize in $SimilarLocalize) {
		&$LoadLocaleData $Localize
		if (&$CheckLocaleData) {
			break
		}
	}
	if (!(&$CheckLocaleData)) {
		if ($LocalizeHead -eq $Localize) { &$FaildLoadLocaleData $Localize }
		&$LoadLocaleData 'en-UK'
	}
}
if (!(&$CheckLocaleData)) {
	foreach ($Localize in $LocalizeList) {
		&$LoadLocaleData $Localize
		if (&$CheckLocaleData) {
			break
		}
	}
}
$result = $Script:LocalizeData
Remove-Variable -Name LocalizeData -Scope Script
$result
