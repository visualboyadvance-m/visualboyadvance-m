param ($SyntaxErrors,$CodeContent,$Localize)
$result = & $PSScriptRoot/SyntaxErrorDataBuilder.ps1 $SyntaxErrors

#_if PSScript
if ($PSVersionTable.PSEdition -eq "Core" -and (Get-Command powershell -ErrorAction Ignore)) {
	$I18NResult = powershell -NoProfile -OutputFormat xml -File $PSScriptRoot/SyntaxErrorI18nDataGetter.ps1 -Content $CodeContent -Localize $Localize
	foreach ($errinfo in $result) {
		$Cross = $I18NResult | Where-Object { $_.ErrorId -eq $errinfo.ErrorId -and ($_.SpoceText -join ';') -eq ($errinfo.SpoceText -join ';') }
		if ($Cross) {
			$errinfo.Message = $Cross.Message
		}
	}
}
#_endif
$result
