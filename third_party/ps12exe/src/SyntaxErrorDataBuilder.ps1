param ($SyntaxErrors)
$SyntaxErrors | ForEach-Object {
	$Extent = $_.Extent
	if ($Extent.StartLineNumber) {
		$LineStr = $Extent.StartLineNumber.ToString()
	}
	if ($Extent.StartLineNumber -ne $Extent.EndLineNumber) {
		$LineStr += "-$($Extent.EndLineNumber)"
	}
	if ($Extent.StartColumnNumber) {
		$ColumnStr = $Extent.StartColumnNumber.ToString()
	}
	if ($Extent.StartColumnNumber -ne $Extent.EndColumnNumber) {
		$ColumnStr += "-$($Extent.EndColumnNumber)"
	}
	$SpoceText = $Extent.StartLineNumber, $Extent.StartColumnNumber
	if($Extent.StartScriptPosition) {
		$FullText = $Extent.StartScriptPosition.GetFullScript()
	}
	@{
		Text = $FullText
		Message = $_.Message
		Spoce = @{
			Line = $Extent.StartLineNumber
			Column = $Extent.StartColumnNumber
			LineEnd = $Extent.EndLineNumber
			ColumnEnd = $Extent.EndColumnNumber
		}
		SpoceText = $SpoceText
		ErrorId = $_.ErrorId
	}
}
