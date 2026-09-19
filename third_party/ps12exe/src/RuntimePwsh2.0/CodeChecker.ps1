[CmdletBinding()]
param([string]$scriptText)

$errors = $null
[System.Management.Automation.PSParser]::Tokenize($scriptText, [ref]$errors) | Out-Null
$errors
