Add-Type -AssemblyName "Microsoft.CodeAnalysis"
Add-Type -AssemblyName "Microsoft.CodeAnalysis.CSharp"

$references = $referenceAssembies | ForEach-Object {
	[Microsoft.CodeAnalysis.MetadataReference]::CreateFromFile($_)
}

$compilationOptions = New-Object Microsoft.CodeAnalysis.CSharp.CSharpCompilationOptions($(
	if ($noConsole) {
		[Microsoft.CodeAnalysis.OutputKind]::WindowsApplication
	}
	else {
		[Microsoft.CodeAnalysis.OutputKind]::ConsoleApplication
	}
))

# Get a default CSharpParseOptions instance
$parseOptions = [Microsoft.CodeAnalysis.CSharp.CSharpParseOptions]::Default
# Set preprocessor symbols
$parseOptions = $parseOptions.WithPreprocessorSymbols($Constants)
$tree = [Microsoft.CodeAnalysis.CSharp.CSharpSyntaxTree]::ParseText($programFrame, $parseOptions)

if ($iconFile) {
	$compilationOptions = $compilationOptions.WithWin32Icon($iconFile)
}
if (!$virtualize) {
	$compilationOptions = $compilationOptions.WithPlatform($(
		switch ($architecture) {
			"x86" { [Microsoft.CodeAnalysis.Platform]::X86 }
			"x64" { [Microsoft.CodeAnalysis.Platform]::X64 }
			"anycpu" { [Microsoft.CodeAnalysis.Platform]::AnyCpu }
			default {
				Write-I18n Warning InvalidArchitecture $architecture
				[Microsoft.CodeAnalysis.Platform]::AnyCpu
			}
		})
	)
}
else {
	Write-I18n Host ForceX86byVirtualization
	$compilationOptions = $compilationOptions.WithPlatform([Microsoft.CodeAnalysis.Platform.X86])
}
$compilationOptions = $compilationOptions.WithOptimizationLevel($(
	if ($prepareDebug) {
		[Microsoft.CodeAnalysis.OptimizationLevel]::Debug
	}
	else {
		[Microsoft.CodeAnalysis.OptimizationLevel]::Release
	}
))

$treeArray = New-Object System.Collections.Generic.List[Microsoft.CodeAnalysis.SyntaxTree]
$treeArray.Add($tree)
$referencesArray = New-Object System.Collections.Generic.List[Microsoft.CodeAnalysis.MetadataReference]
$references | ForEach-Object { $referencesArray.Add($_) }

$compilation = [Microsoft.CodeAnalysis.CSharp.CSharpCompilation]::Create(
	"PSRunner",
	$treeArray.ToArray(),
	$referencesArray.ToArray(),
	$compilationOptions
)

if (!$AstAnalyzeResult.IsConst) {
	$resourceDescription = New-Object Microsoft.CodeAnalysis.Emit.EmbeddedResource("$TempDir\main.ps1", [Microsoft.CodeAnalysis.ResourceDescriptionKind]::Embedded)
	$compilation = $compilation.AddReferences($resourceDescription)
}

# Create a new EmitOptions instance
$emitOptions = New-Object Microsoft.CodeAnalysis.Emit.EmitOptions -ArgumentList @([Microsoft.CodeAnalysis.Emit.DebugInformationFormat]::PortablePdb)
$emitOptions = $emitOptions.WithRuntimeMetadataVersion("$([System.Environment]::Version.Major).0")
$emitOptions = $emitOptions.WithEmitMetadataOnly($false)

$peStream = New-Object System.IO.FileStream($outputFile, [System.IO.FileMode]::Create)
$pdbStream = if ($prepareDebug) {
	New-Object System.IO.FileStream(($outputFile -replace '\.exe$', '.pdb'), [System.IO.FileMode]::Create)
}
$emitResult = $compilation.Emit($peStream, $pdbStream, $null, $null, $null, $emitOptions)
$peStream.Close()
if ($prepareDebug) {
	$pdbStream.Close()
}

if (!$emitResult.Success) {
	throw $emitResult.Diagnostics -join "`n"
}
