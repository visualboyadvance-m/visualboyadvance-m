param (
	[hashtable]$HelpData
)
. $PSScriptRoot/VirtualTerminal.ps1
function Showuseage($Usage) {
	$Usage -replace '(?<!\w)\-(\w+)', "$($VirtualTerminal.Colors.BrightYellow)-`$1$($VirtualTerminal.Colors.Reset)"`
		-replace "'([^']+)'", "$($VirtualTerminal.Colors.BrightMagenta)'`$1'$($VirtualTerminal.Colors.Reset)"`
		-replace '(\w+)=', "$($VirtualTerminal.Colors.BrightGreen)`$1$($VirtualTerminal.Colors.Reset)="`
		-replace '\[([a-zA-Z]+)', "[$($VirtualTerminal.Colors.BrightGreen)`$1$($VirtualTerminal.Colors.Reset)"`

}
function ShowColoredString($Value, $KnownOptions) {
	# 在Value中寻找``包裹的内容，对其进行色彩化
	while ($Value -match '`(?<coloringstr>[^\`]+)`') {
		$str = $Matches['coloringstr']
		$newstr = $str
		$color = $VirtualTerminal.Colors.BrightBlue
		if (($str.StartsWith('"') -and $str.EndsWith('"')) -or ($str.StartsWith("'") -and $str.EndsWith("'"))) {
			# 字符串，淡紫色渲染
			$color = $VirtualTerminal.Colors.BrightMagenta
		}
		elseif ($str.IndexOf('::') -ge 0) {
			$newstr = $str.Replace('::', "$($VirtualTerminal.ResetAll)::$($VirtualTerminal.Colors.BrightYellow)")
		}
		elseif ($str.StartsWith('-') -or $KnownOptions -ccontains $str) {
			# 选项，淡黄色渲染
			$color = $VirtualTerminal.Colors.BrightYellow
		}
		elseif ($str.IndexOf('://') -ge 0) {
			# URL，淡蓝色渲染+下划线
			$color += $VirtualTerminal.Styles.Underline
		}
		elseif ($str -match '^%\w+%$') {
			# 环境变量，绿色渲染
			$color = $VirtualTerminal.Colors.BrightGreen
		}
		elseif ($str -match '^[\w\-]+$' -and (Get-Command $str -ErrorAction Ignore)) {
			# 命令，黄色渲染
			$color = $VirtualTerminal.Colors.BrightYellow
		}
		$Value = $Value.Replace("``$str``", "$color$newstr$($VirtualTerminal.ResetAll)")
	}
	return $Value
}
function ShowParamsHelp($ParamsHelpData) {
	# 已知的选项名集合：顶层键加上所有嵌套子键，供描述中的``着色``识别
	$KnownOptions = @($ParamsHelpData.Keys)
	foreach ($ParamValue in $ParamsHelpData.Values) {
		if ($ParamValue -is [System.Collections.IDictionary]) {
			$KnownOptions += @($ParamValue.Keys)
		}
	}

	# 对于所有的键
	$MaxKeyLength = $ParamsHelpData.Keys.Length | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum

	$ParamsHelpData.Keys | ForEach-Object {
		$Key = $_
		$Value = $ParamsHelpData[$Key]
		$Spaces = ' ' * ($MaxKeyLength - $Key.Length)

		if ($Value -is [System.Collections.IDictionary]) {
			# 对象参数：键名作为分组标题，子键缩进并相互对齐
			"$($VirtualTerminal.Colors.BrightYellow)$Key$Spaces$($VirtualTerminal.Colors.Reset)"
			$SubMaxKeyLength = $Value.Keys.Length | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
			$Value.Keys | ForEach-Object {
				$SubKey = $_
				$SubValue = ShowColoredString $Value[$SubKey] $KnownOptions
				$SubSpaces = ' ' * ($SubMaxKeyLength - $SubKey.Length)
				"    $($VirtualTerminal.Colors.BrightYellow)$SubKey$SubSpaces$($VirtualTerminal.Colors.Reset) : $SubValue"
			}
		}
		else {
			$Value = ShowColoredString $Value $KnownOptions
			"$($VirtualTerminal.Colors.BrightYellow)$Key$Spaces$($VirtualTerminal.Colors.Reset) : $Value"
		}
	}
}
$HelpData.title
Showuseage $HelpData.Usage
ShowParamsHelp $HelpData.PrarmsData
