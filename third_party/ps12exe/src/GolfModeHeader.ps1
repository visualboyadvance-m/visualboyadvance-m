#region Input/Output (输入/输出)
function rl { Read-Host }

function pr([Parameter(ValueFromPipeline = $true)] [object[]]$input) {
	process {
		Write-Host -NoNewline ($input -join '')
	}
}

function pn([Parameter(ValueFromPipeline = $true)] [object[]]$input) {
	process {
		Write-Host ($input -join '')
	}
}

function rla {
	[System.IO.File]::ReadAllLines($MyInvocation.MyCommand.Path)
}

function rlat {
	[System.IO.File]::ReadAllText($MyInvocation.MyCommand.Path)
}

#endregion

#region String Operations (字符串操作)
function ch([Parameter(ValueFromPipeline = $true)]$InputObject, [int]$index) {
	process {
		$InputObject[$index]
	}
}

function rpl([Parameter(ValueFromPipeline = $true)]$InputObject, [string]$old, [string]$new = '') {
	process {
		$InputObject -replace $old, $new
	}
}

function spc([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		$InputObject.ToCharArray()
	}
}

function padl([Parameter(ValueFromPipeline = $true)]$InputObject, [int]$width, [char]$char = ' ') {
	process {
		$InputObject.PadLeft($width, $char)
	}
}

function padr([Parameter(ValueFromPipeline = $true)]$InputObject, [int]$width, [char]$char = ' ') {
	process {
		$InputObject.PadRight($width, $char)
	}
}

function sub([Parameter(ValueFromPipeline = $true)]$InputObject, [int]$start = 0, [int]$length) {
	process {
		if ($length -eq $null) {
			$length = $InputObject.Length - $start
		}
		$InputObject.Substring($start, [System.Math]::Min($length, $InputObject.Length - $start))
	}
}

function rev {
	$input -join '' -split '' | ForEach-Object {
		$a = $_
		$b = $a + $b
	}
	$b
}

function occ([Parameter(ValueFromPipeline = $true)]$InputObject, [char]$char) {
	process {
		($InputObject.ToCharArray() | Where-Object { $_ -eq $char } | Measure-Object).Count
	}
}

function delc([Parameter(ValueFromPipeline = $true)]$InputObject, [char[]]$chars) {
	process {
		$InputObject -replace "[$($chars -join '')]"
	}
}

function getnums([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		[regex]::Matches($InputObject, '\d').Value -join ''
	}
}

function chcodes([Parameter(ValueFromPipeline = $true)]$InputObject, [string]$separator = '') {
	process {
		$InputObject.ToCharArray() | ForEach-Object { [int]$_ } -join $separator
	}
}

function suball([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		$s = $InputObject
		1..$s.Length | ForEach-Object {
			$len = $_
			0..($s.Length - $len) | ForEach-Object {
				$s.Substring($_, $len)
			}
		}
	}
}

#endregion

#region Number Operations (数字操作)

function mod([Parameter(ValueFromPipeline = $true)]$InputObject, [int]$divisor) {
	process {
		$InputObject % $divisor
	}
}

function idiv([Parameter(ValueFromPipeline = $true)]$InputObject, [int]$divisor) {
	process {
		[int]($InputObject / $divisor)
	}
}

function sgn([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		[Math]::Sign($InputObject)
	}
}

function even([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		($InputObject % 2) -eq 0
	}
}

function odd([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		($InputObject % 2) -ne 0
	}
}

function gcd {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$a,

		[Parameter(Mandatory = $true, Position = 1)]
		[int]$b
	)
	process {
		if ($b -eq 0) {
			$a
		}
		else {
			gcd -a $b -b ($a % $b)
		}
	}
}

function lcm {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$a,

		[Parameter(Mandatory = $true, Position = 1)]
		[int]$b
	)
	process {
		($a * $b) / (gcd -a $a -b $b)
	}
}

function fac {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$n
	)
	process {
		if ($n -le 1) {
			1
		}
		else {
			$n * (fac -n ($n - 1))
		}
	}
}

function qpow {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$base,
		[Parameter(Mandatory = $true, Position = 1)]
		[int]$exponent
	)
	process {
		$result = 1
		while ($exponent -gt 0) {
			if ($exponent -band 1) {
				$result *= $base # Fixed: Use $base here, not $input which is pipeline input
			}
			$base = $base * $base # Fixed: Use $base here, not $input which is pipeline input
			$exponent = $exponent -shr 1
		}
		$result
	}
}

function fib {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$n
	)
	process {
		$a, $b = 0, 1
		for ($i = 0; $i -lt $n; $i++) {
			$a, $b = $b, ($a + $b)
		}
		$a
	}
}

function fibs {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$n
	)
	process {
		$a, $b = 0, 1
		$result = @()
		for ($i = 0; $i -lt $n; $i++) {
			$result += $a
			$a, $b = $b, ($a + $b)
		}
		$result
	}
}

function isprime {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$n
	)
	process {
		if ($n -le 1) {
			return $false
		}
		if ($n -le 3) {
			return $true
		}
		if ($n % 2 -eq 0 -or $n % 3 -eq 0) {
			return $false
		}
		for ($i = 5; $i * $i -le $n; $i += 6) {
			if ($n % $i -eq 0 -or $n % ($i + 2) -eq 0) {
				return $false
			}
		}
		return $true
	}
}

function factors($n) {
	$n = $(if ($input) { $n; $input }else { $n })
	1..[math]::Sqrt($n) | ForEach-Object {
		if ($n % $_ -eq 0) {
			$_
			if ($_ -ne ($n / $_)) {
				$n / $_
			}
		}
	}
}

#endregion

#region Array/Collection Operations (数组/集合操作)

function hd($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values[0]
}

function tl($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values[-1]
}

function rest($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values[1..($values.Length - 1)]
}

function take($values, $n = 1) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values[0..($n - 1)]
}

function drop($values, $n = 1) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values[$n..($values.Length - 1)]
}

function sum($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	($values | Measure-Object -Sum).Sum
}

function avg($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	($values | Measure-Object -Average).Average
}

function min($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	($values | Measure-Object -Minimum).Minimum
}

function max($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	($values | Measure-Object -Maximum).Maximum
}

function uniq($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values | Select-Object -Unique
}

function srt($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values | Sort-Object
}

function rvs($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	[array]::Reverse($values)
	$values
}

function flat($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values | ForEach-Object {
		if ($_ -is [System.Collections.IEnumerable] -and $_ -isnot [string]) {
			flat $_
		}
		else {
			$_
		}
	}
}

function gpby($values, $property) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values | Group-Object -Property $property
}

function ct($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values.Count
}

function flt($values, $scriptblock) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values | Where-Object -FilterScript $scriptblock
}

function intersect($values, $arr2) {
	$values = $(if ($input) { $values; $input }else { $values })
	Compare-Object $values $arr2 -IncludeEqual -ExcludeDifferent |
	Where-Object { $_.SideIndicator -eq '==' } |
	ForEach-Object { $_.InputObject }
}

function diff($values, $arr2) {
	$values = $(if ($input) { $values; $input }else { $values })
	Compare-Object $values $arr2 -ExcludeDifferent |
	Where-Object { $_.SideIndicator -eq '<=' } |
	ForEach-Object { $_.InputObject }
}

function perm {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, ValueFromPipeline = $true)]
		[System.Collections.ArrayList]$arr
	)
	process {
		if ($arr.Count -le 1) {
			, $arr
			return
		}

		for ($i = 0; $i -lt $arr.Count; $i++) {
			$copy = [System.Collections.ArrayList]::new($arr)
			$first = $copy[$i]
			$copy.RemoveAt($i)
			$subPerms = perm -arr $copy
			foreach ($subPerm in $subPerms) {
				[System.Collections.ArrayList]$newPerm = [System.Collections.ArrayList]::new($subPerm)
				$newPerm.Insert(0, $first)
				, $newPerm
			}
		}
	}
}

function comb {
	[CmdletBinding()]
	Param(
		[Parameter(Position = 0)]
		[int]$k = 1,

		[Parameter(Mandatory = $true, Position = 1, ValueFromPipeline = $true)]
		[System.Collections.ArrayList]$arr
	)
	process {
		if ($k -eq 0) {
			, @()
			return
		}
		if ($arr.Count -lt $k) {
			return
		}
		if ($arr.Count -eq $k) {
			, $arr
			return
		}
		$first = $arr[0]
		$rest = [System.Collections.ArrayList]::new($arr)
		$rest.RemoveAt(0)
		$combsWithFirst = comb -k ($k - 1) -arr $rest | ForEach-Object {
			[System.Collections.ArrayList]$newComb = [System.Collections.ArrayList]::new($_)
			$newComb.Insert(0, $first)
			, $newComb
		}
		$combsWithoutFirst = comb -k $k -arr $rest
		$combsWithFirst + $combsWithoutFirst
	}
}

function transpose {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0, ValueFromPipeline)]
		[object[]]$input
	)
	BEGIN {
		$data = @()
	}
	process {
		$data += , $input
	}

	END {
		if ($data -isnot [System.Array] -or $data[0] -isnot [System.Collections.IEnumerable]) {
			Write-Error "输入必须是二维数组"
			return
		}

		$rows = $data.Length
		$cols = $data[0].Length
		$result = @()

		for ($j = 0; $j -lt $cols; $j++) {
			$newRow = @()
			for ($i = 0; $i -lt $rows; $i++) {
				$newRow += $data[$i][$j]
			}
			$result += , $newRow
		}

		$result
	}
}

function tohash($values) {
	$values = $(if ($input) { $values; $input }else { $values })
	$h = @{}
	$values | ForEach-Object {
		$h[$_[0]] = $_[1]
	}
	$h
}

#endregion

#region Logical Operations (逻辑操作)

function all($values, $scriptblock) {
	$values = $(if ($input) { $values; $input }else { $values })
	if ($scriptblock) {
		!($values | Where-Object -FilterScript $scriptblock | Measure-Object).Count
	}
	else {
		!($values | Where-Object -FilterScript { -not $_ } | Measure-Object).Count
	}
}

function any($values, $scriptblock) {
	$values = $(if ($input) { $values; $input }else { $values })
	if ($scriptblock) {
		($values | Where-Object -FilterScript $scriptblock | Measure-Object).Count
	}
	else {
		($values | Where-Object -FilterScript { $_ } | Measure-Object).Count
	}
}

function iif {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[bool]$condition,

		[Parameter(Mandatory = $true, Position = 1)]
		[scriptblock]$trueBlock,

		[Parameter(Mandatory = $true, Position = 2)]
		[scriptblock]$falseBlock
	)
	process {
		if ($condition) {
			& $trueBlock
		}
		else {
			& $falseBlock
		}
	}
}

function rep {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$n,
		[Parameter(Mandatory = $true, Position = 1)]
		[scriptblock]$block
	)
	process {
		1..$n | ForEach-Object $block
	}
}

function map($values, $block) {
	$values = $(if ($input) { $values; $input }else { $values })
	$values | ForEach-Object $block
}

function reduce([Parameter(ValueFromPipeline = $true)] [object[]]$values, [scriptblock]$block, $init = $null) {
	process {
		$result = $init
		if ($null -eq $result) {
			$result = $values[0]
			$values = $values | drop 1
		}
		foreach ($i in $values) {
			$result = & $block $result $i
		}
		$result
	}
}

#endregion

#region Other Utilities (其他实用工具)

function rng {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[int]$start,

		[Parameter(Position = 1)]
		[int]$end = $start
	)
	process {
		$start..$end
	}
}

function time {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0)]
		[scriptblock]$scriptblock
	)
	process {
		Measure-Command -Expression $scriptblock
	}
}

function up([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		$InputObject.ToUpper()
	}
}

function low([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		$InputObject.ToLower()
	}
}

function len([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		$InputObject.Length
	}
}

function trim([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		$InputObject.Trim()
	}
}

function int([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		[int]$InputObject
	}
}

function chr([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		[char]$InputObject
	}
}

function ord([Parameter(ValueFromPipeline = $true)]$InputObject) {
	process {
		[int][char]$InputObject
	}
}

function jo([Parameter(ValueFromPipeline = $true)] [object[]]$values, [string]$separator = '') {
	process {
		$values -join $separator
	}
}

function sp([Parameter(ValueFromPipeline = $true)]$InputObject, [string]$separator = ' ') {
	process {
		$InputObject -split $separator
	}
}

function randstr {
	[CmdletBinding()]
	Param(
		[Parameter(Position = 0)]
		[int]$length = 8,
		[Parameter(Position = 1)]
		[string]$chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
	)
	process {
		-join (1..$length | ForEach-Object { $chars[(Get-Random -Maximum $chars.Length)] })
	}
}

function b64e {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0, ValueFromPipeline = $true)]
		[string]$text
	)
	process {
		[Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($text))
	}
}

function b64d {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, Position = 0, ValueFromPipeline = $true)]
		[string]$base64Text
	)
	process {
		[System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($base64Text))
	}
}
#endregion

#region Aliases (别名)
Set-Alias -Name mo -Value Measure-Object
Set-Alias -Name se -Value Select-Object
Set-Alias -Name rs -Value Read-Host  # 保留 rs，因为有些时候需要交互式输入
#endregion

#region 输入参数的简写
$I = $Input
$SI = "$I"
$CI = [CHAR[]]$SI
$NI = $I | ForEach-Object { $_ -as [int] }
$A = $Args
$SA = "$A"
$CA = [CHAR[]]$SA
$NA = $A | ForEach-Object { $_ -as [int] }
$0 = $1 = $2 = $3 = $4 = $5 = $6 = $7 = $8 = $9 = 0
if ($NA) { $N = $NA[0] }

#endregion

#region 数学运算

function pow($InputObject, $exp = 2) {
	$InputObject = $(if ($input) { $InputObject; $input }else { $InputObject })
	$InputObject | ForEach-Object {
		[Math]::Pow($_, $exp)
	}
}

function sqrt($InputObject) {
	$InputObject = $(if ($input) { $InputObject; $input }else { $InputObject })
	$InputObject | ForEach-Object {
		[Math]::Sqrt($_)
	}
}

function floor($InputObject) {
	$InputObject = $(if ($input) { $InputObject; $input }else { $InputObject })
	$InputObject | ForEach-Object {
		[Math]::Floor($_)
	}
}

function ceil($InputObject) {
	$InputObject = $(if ($input) { $InputObject; $input }else { $InputObject })
	$InputObject | ForEach-Object {
		[Math]::Ceiling($_)
	}
}

function round($InputObject) {
	$InputObject = $(if ($input) { $InputObject; $input }else { $InputObject })
	$InputObject | ForEach-Object {
		[Math]::Round($_)
	}
}

function abs($InputObject) {
	$InputObject = $(if ($input) { $InputObject; $input }else { $InputObject })
	$InputObject | ForEach-Object {
		[Math]::Abs($_)
	}
}

#endregion

#region 扩展

function deepflat([Parameter(ValueFromPipeline = $true)] [object[]]$values) {
	process {
		$values | ForEach-Object {
			if ($_ -is [System.Collections.IEnumerable] -and $_ -isnot [string]) {
				deepflat $_
			}
			else {
				$_
			}
		}
	}
}

function issorted([Parameter(ValueFromPipeline = $true)] [object[]]$values, [int]$direction = 1) {
	process {
		if ($values.Length -le 1) {
			return $true
		}
		for ($i = 1; $i -lt $values.Length; $i++) {
			if ($direction -gt 0 -and $values[$i - 1] -gt $values[$i]) {
				return $false
			}
			elseif ($direction -lt 0 -and $values[$i - 1] -lt $values[$i]) {
				return $false
			}
		}
		return $true
	}
}

function readj {
	Get-Content -Raw $MyInvocation.MyCommand.Path | ConvertFrom-Json
}

function uniqo {
	[CmdletBinding()]
	Param(
		[Parameter(Mandatory = $true, ValueFromPipeline = $true)]
		$InputObject
	)
	BEGIN {
		$seen = @{}
	}
	process {
		$InputObject | Where-Object {
			$key = $_
			if ($key -is [System.Collections.IStructuralEquatable]) {
				$key = $_.GetHashCode()  # 对于复杂对象使用哈希
			}
			if (-not $seen.ContainsKey($key)) {
				$seen[$key] = $true
				$true  #输出
			}
		}
	}
}

function counts([Parameter(ValueFromPipeline = $true)] [object[]]$values) {
	process {
		$values | Group-Object -NoElement | ForEach-Object {
			($_.Name, $_.Count)
		} | tohash
	}
}

#endregion
