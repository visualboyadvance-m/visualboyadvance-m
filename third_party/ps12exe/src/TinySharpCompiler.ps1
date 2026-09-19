# 重新构建编译内容
$ConstResult = $RowResult | ForEach-Object {
	(($_ | Out-String) -replace '\r\n$', '')
}
$ConstResult = $ConstResult -join "`n"

# TinySharp 只在 Windows PowerShell 宿主编译，所以引用直接用 GAC 程序集名
$Refs = @(
	'System',
	'System.Core',
	'netstandard, Version=2.0.0.0, Culture=neutral, PublicKeyToken=cc7b13ffcd2ddd51'
)
# 再补上 $PSScriptRoot\bin\AsmResolver 下的所有 dll
Get-ChildItem $PSScriptRoot\bin\AsmResolver -Recurse -Filter *.dll | ForEach-Object {
	$Refs += $_.FullName
	try {
		Add-Type -LiteralPath $_.FullName -ErrorVariable $null
	}
	catch {
		$_.Exception.LoaderExceptions | Out-String | Write-Verbose
		$Error.Remove($_)
	}
}

# 添加c#代码
$TinySharpCode = Get-Content $PSScriptRoot/programFrames/TinySharp.cs -Raw -Encoding UTF8
Add-Type $TinySharpCode -ReferencedAssemblies $Refs

# 编译
$file = [TinySharp.Program]::Compile($targetRuntime, $architecture, $ConstResult, [ps12exeConstEvalHost]::LastExitCode, -not $noOutput, $noConsole)
if ($iconFile) {
	$file.SetWin32Icon($iconFile)
}
if ($resourceParams.description -or $resourceParams.company -or $resourceParams.title -or $resourceParams.product -or $resourceParams.copyright -or $resourceParams.trademark -or $resourceParams.version) {
	$file.SetAssemblyInfo($resourceParams.description, $resourceParams.company, $resourceParams.title, $resourceParams.product, $resourceParams.copyright, $resourceParams.trademark, $resourceParams.version)
}
$file.Build($outputFile)
