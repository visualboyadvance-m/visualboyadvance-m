<#
function ShowAst($Ast) {
	function Mapper($Ast) {
		$Ast.GetType().Name | Write-Host
		$Ast | Write-Host
		return $false
	}
	$Ast.Find($function:Mapper, $true)
}
function ShowAstOfExpr($Expr) {
	$Ast = [System.Management.Automation.Language.Parser]::ParseInput($Expr, [ref]$null, [ref]$null)
	ShowAst $Ast
}
#>
<#
AI prompt: 用你所知的最刁钻的方式和最花哨的技术写一段powershell5能用的、没有副作用、运行结果是常量的ps程序，以```pwsh包裹起来发送给我。
我需要测试一段评估程序是不是常量程序的程序
#>
function AstAnalyze([System.Management.Automation.Language.ScriptBlockAst]$Ast) {
	$script:ConstCommands = @('Write-Host', 'echo', 'Write-Output', 'Write-Debug', 'Write-Information', 'ConvertFrom-Json', 'ConvertTo-Json', 'Write-Host')
	$script:ConstVariables = @('?', '^', '$', 'Error', 'false', 'IsCoreCLR', 'IsLinux', 'IsMacOS', 'IsWindows', 'null', 'true', 'PSEXEScript', 'Write-Host', 'MyInvocation')
	$script:ConstTypes = @('Boolean', 'Char', 'DateTime', 'Decimal', 'Double', 'Int16', 'Int32', 'Int64', 'Int8', 'Int', 'Single', 'String', 'UInt16', 'UInt32', 'UInt64', 'UInt8', 'Void', 'Regex', 'System.Text.RegularExpressions.RegexOptions', 'HashTable', 'OrderedDictionary', 'PSObject', 'PSVariable', 'PSNoteProperty', 'PSMemberInfo', 'PSCustomObject', 'Math', 'Array', 'ref', 'Guid')
	$script:ConstAttributes = @('cmdletbinding', 'cmdlet', 'parameter', 'alias')
	$script:EffectVariables = @('PSEXEpath', 'ConfirmPreference', 'DebugPreference', 'EnabledExperimentalFeatures', 'ErrorActionPreference', 'ErrorView', 'ExecutionContext', 'FormatEnumerationLimit', 'HOME', 'Host', 'InformationPreference', 'input', 'MaximumHistoryCount', 'NestedPromptLevel', 'OutputEncoding', 'PID', 'PROFILE', 'ProgressPreference', 'PSBoundParameters', 'PSCommandPath', 'PSCulture', 'PSDefaultParameterValues', 'PSEdition', 'PSEmailServer', 'PSGetAPI', 'PSHOME', 'PSNativeCommandArgumentPassing', 'PSNativeCommandUseErrorActionPreference', 'PSScriptRoot', 'PSSessionApplicationName', 'PSSessionConfigurationName', 'PSSessionOption', 'PSStyle', 'PSUICulture', 'PSVersionTable', 'PWD', 'ShellId', 'StackTrace', 'VerbosePreference', 'WarningPreference', 'WhatIfPreference')
	$script:AnalyzeResult = @{
		IsConst                  = $true
		ImporttedExternalScripts = $false
		UsedNonConstVariables    = @()
		UsedNonConstFunctions    = @()
		UsedNonConstTypes        = @()
	}
	$script:ConstTypes = $script:ConstTypes | ForEach-Object { ($_ -as [Type]).FullName } | Where-Object { $_ -ne $null }
	function IsConstType([string]$typename) {
		$typename = $typename.TrimEnd('[]')
		if ($script:ConstTypes -contains ($typename -as [Type]).FullName) {
			return $true
		}
		if ($script:ConstTypes -contains $typename) {
			return $true
		}
		return $false
	}
	function AstMapper($Ast) {
		if ($Ast -is [System.Management.Automation.Language.CommandAst]) {
			if ($script:ConstCommands -notcontains $Ast.CommandElements[0].Value) {
				$script:AnalyzeResult.IsConst = $false
				$script:AnalyzeResult.UsedNonConstFunctions += $Ast.CommandElements[0].Value
			}
			if ($Ast.InvocationOperator -eq 'Dot' -or $Ast.InvocationOperator -eq 'Ampersand') {
				$script:AnalyzeResult.ImporttedExternalScripts = $true
			}
		}
		elseif ($Ast -is [System.Management.Automation.Language.VariableExpressionAst]) {
			if ($script:EffectVariables -contains $Ast.VariablePath.UserPath) {
				$script:AnalyzeResult.IsConst = $false
				$script:AnalyzeResult.UsedNonConstVariables += $Ast.VariablePath.UserPath
			}
		}
		elseif ($Ast -is [System.Management.Automation.Language.TypeDefinitionAst]) {
			$script:ConstTypes += $Ast.Name
		}
		elseif ($Ast -is [System.Management.Automation.Language.InvokeMemberExpressionAst]) {
			if ($script:ConstTypes -notcontains $Ast.Expression.TypeName) {
				if ($Ast.Expression.TypeName) {
					$script:AnalyzeResult.IsConst = $false
					$script:AnalyzeResult.UsedNonConstFunctions += "[$($Ast.Expression.TypeName)]::$($Ast.Member.Value)"
				}
			}
		}
		elseif ($Ast -is [System.Management.Automation.Language.TypeExpressionAst]) {
			if (-not (IsConstType $Ast.TypeName)) {
				$script:AnalyzeResult.IsConst = $false
				$script:AnalyzeResult.UsedNonConstVariables += "[$($Ast.TypeName)]"
			}
		}
		elseif ($Ast -is [System.Management.Automation.Language.FunctionDefinitionAst]) {
			$script:ConstCommands += $Ast.Name
			$script:ConstVariables += "function:$($Ast.Name)"
		}
		elseif ($Ast -is [System.Management.Automation.Language.AssignmentStatementAst]) {
			if ($script:EffectVariables -notcontains $Ast.Left.VariablePath) {
				$script:ConstVariables += $Ast.Left.VariablePath.UserPath
			}
		}
		elseif ($Ast -is [System.Management.Automation.Language.NamedBlockAst]) {
			$ConstCommandsBackup = $script:ConstCommands
			$ConstVariablesBackup = $script:ConstVariables
			$script:ConstVariables += 'args'
			foreach ($Statement in $Ast.Statements) {
				$Statement.Find($function:AstMapper, $true)
			}
			$script:ConstCommands = $ConstCommandsBackup
			$script:ConstVariables = $ConstVariablesBackup
		}
		elseif ($Ast -is [System.Management.Automation.Language.AttributeAst]) {
			if ($script:ConstAttributes -notcontains $Ast.TypeName.FullName) {
				$script:AnalyzeResult.IsConst = $false
				$script:AnalyzeResult.UsedNonConstTypes += $Ast.TypeName.FullName
			}
		}
		return $false
	}
	[void]$Ast.Find($function:AstMapper, $true)
	$script:AnalyzeResult.UsedNonConstVariables = $script:AnalyzeResult.UsedNonConstVariables | Sort-Object -Unique | Where-Object { $_ }
	$script:AnalyzeResult.UsedNonConstFunctions = $script:AnalyzeResult.UsedNonConstFunctions | Sort-Object -Unique | Where-Object { $_ }
	$local:AnalyzeResult = $script:AnalyzeResult
	Remove-Variable -Name @('ConstCommands', 'ConstVariables', 'ConstTypes', 'EffectVariables', 'AnalyzeResult') -Scope Script
	return $local:AnalyzeResult
}
