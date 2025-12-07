$DisablePredicates = @('off', 'false', 'N', 'No', 'unset', '0', 'disable', '$false')
function IsDisable($value) {
	$DisablePredicates -contains "$value"
}

$EnablePredicates = @('on', 'true', 'Y', 'Yes', 'set', '1', 'enable', '$true')
function IsEnable($value) {
	$EnablePredicates -contains "$value"
}
