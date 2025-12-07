. $PSScriptRoot\ConstProgramCheck.ps1
if (!$programFrame) {
	#_if PSEXE #这是该脚本被ps12exe编译时使用的预处理代码
		#_include_as_value programFrame "$PSScriptRoot/programFrames/default.cs" #将default.cs中的内容内嵌到该脚本中
	#_else #否则正常读取cs文件
		[string]$programFrame = Get-Content $PSScriptRoot/programFrames/default.cs -Raw -Encoding UTF8
	#_endif
}

$programFrame = $programFrame.Replace("`$lcid", $lcid)
$programFrame = $programFrame.Replace("`$threadingModel", $threadingModel)
$programFrame = $programFrame.Replace("`$TargetFramework", $TargetFramework)
$resourceParamKeys | ForEach-Object {
	$programFrame = $programFrame.Replace("`$$_", $resourceParams[$_])
}
