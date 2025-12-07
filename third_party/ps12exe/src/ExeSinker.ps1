[CmdletBinding()]
param (
	[string]$inputFile,
	[switch]$removeResources,
	[switch]$removeVersionInfo
)

Get-ChildItem $PSScriptRoot\bin\AsmResolver -Recurse -Filter AsmResolver.PE*.dll | ForEach-Object {
	try {
		Add-Type -LiteralPath $_.FullName -ErrorVariable $null
	} catch {
		$_.Exception.LoaderExceptions | Out-String | Write-Verbose
		$Error.Remove($_)
	}
}

$file = [AsmResolver.PE.PEImage]::FromFile($inputFile)
if ($removeResources) {
	$file.Resources = $null
}
elseif ($removeVersionInfo) {
	$file.Resources.Entries.Remove(($file.Resources.Entries | Where-Object { $_.Type -eq 'Version' })) | Out-Null
}
$file.DllCharacteristics = $file.DllCharacteristics -band -not [AsmResolver.PE.File.Headers.DllCharacteristics]::DynamicBase
$Builder = New-Object AsmResolver.PE.DotNet.Builder.ManagedPEFileBuilder
$file = $builder.CreateFile($file)
$file.Write($inputFile)
