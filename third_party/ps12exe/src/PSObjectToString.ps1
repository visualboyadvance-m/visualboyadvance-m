$script:tabnum = 0
function PSObjectToString($obj, [Switch]$OneLine = $false) {
	if ($obj -is [System.Collections.IDictionary]) {
		$script:tabnum += 1
		$str = ''
		if ($obj -is [System.Collections.Specialized.OrderedDictionary]) {
			$str = '[ordered]'
		}
		$str += '@{'
		if (-not $OneLine) {
			$str += "`n"
		}
		$str += (($obj.GetEnumerator() | ForEach-Object {
			if (-not $OneLine) {
				"`t" * $script:tabnum
			}
			$_.Key + ' = ' + $(PSObjectToString $_.Value $OneLine)
			if (-not $OneLine) { "`n" }
			else { ';' }
		}) -join '')
		if (-not $OneLine) {
			$str += "`t" * ($script:tabnum - 1)
		}
		$str += '}'
		$str
		$script:tabnum -= 1
	}
	elseif ($obj -is [System.Collections.ICollection]) {
		'@(' + (($obj | ForEach-Object {
			PSObjectToString $_ $OneLine
			', '
		} | Select-Object -SkipLast 1) -join '') + ')'
	}
	elseif ($obj -is [string]) {
		"'" + $obj.Replace("'", "''") + "'"
	}
	elseif ($obj -is [int]) { $obj }
	elseif ($obj -is [bool] -or $obj -is [switch]) { "`$$obj" }
	else { "$obj" }
}
function Get-ArgsString([hashtable]$Params) {
	$Params.GetEnumerator() | ForEach-Object { "-$($_.Key):$(PSObjectToString $_.Value -OneLine)" }
}
