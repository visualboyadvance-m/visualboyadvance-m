param ($Content, $Locale)

[cultureinfo]::CurrentUICulture = $Locale

$SyntaxErrors = $Tokens = $null
$null = [System.Management.Automation.Language.Parser]::ParseInput($Content, [ref]$Tokens, [ref]$SyntaxErrors)

& $PSScriptRoot/SyntaxErrorDataBuilder.ps1 $SyntaxErrors
