param(
	[scriptblock]$CheckLocaleData = {
		$null -ne $Script:LocalizeData
	},
	[scriptblock]$FailedLoadLocaleData = {
		param (
			[string]$Locale
		)
		Write-Warning "Failed to load locale data $Locale`nSee $LocalizeDir/README.md for how to add custom locale."
	},
	[scriptblock]$LoadLocaleData = {
		param (
			[string]$Locale
		)
		$file = "$LocalizeDir\$Locale.ps1"
		if (Test-Path $file) { $Script:LocalizeData = try { &$file } catch { $null } }
	},
	[string]$Locale
)

if (!$Locale) {
	# 本机语言
	$Locale = $env:LANG
	if (!$Locale) { $Locale = $env:LANGUAGE }
	if (!$Locale) { $Locale = $env:LC_ALL }
	if (!$Locale -and (Get-Command locale -ErrorAction Ignore)) {
		$Locale = try {
			&locale -uU
		}
		catch { $null }
	}
	if ($Locale) {
		$Locale = $Locale.Split('.')[0].Replace('_', '-')
	}
	else {
		$Locale = (Get-Culture).Name
	}
}

$LocalizeDir = "$PSScriptRoot/locale"

&$LoadLocaleData $Locale
if (!(&$CheckLocaleData)) {
	$LocalizeList = Get-ChildItem $LocalizeDir | Where-Object { $_.Name -like '*.fbs' } | ForEach-Object { $_.BaseName }
	$LocalizeHead = $Locale.Split('-')[0]
	$SimilarLocalize = $LocalizeList | Where-Object { $_.StartsWith($LocalizeHead) }
	if ($LocalizeHead -ne $Locale) { &$FailedLoadLocaleData $Locale }
	foreach ($Locale in $SimilarLocalize) {
		&$LoadLocaleData $Locale
		if (&$CheckLocaleData) {
			break
		}
	}
	if (!(&$CheckLocaleData)) {
		if ($LocalizeHead -eq $Locale) { &$FailedLoadLocaleData $Locale }
		&$LoadLocaleData 'en-UK'
	}
}
if (!(&$CheckLocaleData)) {
	foreach ($Locale in $LocalizeList) {
		&$LoadLocaleData $Locale
		if (&$CheckLocaleData) {
			break
		}
	}
}
$result = $Script:LocalizeData
Remove-Variable -Name LocalizeData -Scope Script
$result
