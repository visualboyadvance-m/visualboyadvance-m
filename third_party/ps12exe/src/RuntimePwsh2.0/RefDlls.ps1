[CmdletBinding()]
param ([switch]$noConsole)

function GetAssemblyLocation($assemblyName) {
	([System.AppDomain]::CurrentDomain.GetAssemblies() | Where-Object { $_.ManifestModule.Name -ieq $assemblyName } | Select-Object -First 1).Location
}
function LoadAssemblyAndGetLocation($verinfo) {
	$n = New-Object System.Reflection.AssemblyName($verinfo)
	[System.AppDomain]::CurrentDomain.Load($n).Location
}
$referenceAssembies = @((GetAssemblyLocation "System.dll"))
if (!$noConsole) { $referenceAssembies += GetAssemblyLocation "Microsoft.PowerShell.ConsoleHost.dll" }
$referenceAssembies += GetAssemblyLocation "System.Management.Automation.dll"
# PS3+ 的 PSObject 实现 IDynamicMetaObjectProvider；-version 2.0 在引擎缺失时会落到 5.1，不引用 System.Core 会 CS0012
Add-Type -AssemblyName System.Core
$referenceAssembies += GetAssemblyLocation "System.Core.dll"

if ($noConsole) {
	$fx = if ($PSVersionTable.PSVersion.Major -le 2) { '2.0.0.0' } else { '4.0.0.0' }
	$referenceAssembies += LoadAssemblyAndGetLocation "System.Windows.Forms, Version=$fx, Culture=neutral, PublicKeyToken=b77a5c561934e089"
	$referenceAssembies += LoadAssemblyAndGetLocation "System.Drawing, Version=$fx, Culture=neutral, PublicKeyToken=b03f5f7f11d50a3a"
}

$referenceAssembies -ne $null
