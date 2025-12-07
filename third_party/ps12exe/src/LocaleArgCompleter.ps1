Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
Get-ChildItem $PSScriptRoot\Locale -Filter *.fbs | ForEach-Object { $_.BaseName } | Where-Object {
	$_ -like "$($wordToComplete.Trim('"', "'"))*"
} | ForEach-Object {
	if ($wordToComplete.StartsWith('"')) { "`"$_`"" }
	elseif ($wordToComplete.StartsWith("'")) { "'$_'" }
	else { $_ }
}
