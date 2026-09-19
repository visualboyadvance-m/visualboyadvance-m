[CmdletBinding()]
param (
	[ArgumentCompleter({
		Get-ChildItem $PSScriptRoot -Filter *.txt | ForEach-Object { $_.Name -replace '\.txt$', '' }
	})]
	[ValidateScript({ Test-Path "$PSScriptRoot\$_.txt" -ErrorAction Ignore })]
	[string]$Localize,
	[ArgumentCompleter({
		Param($commandName, $parameterName, $wordToComplete, $commandAst, $fakeBoundParameters)
		. "$PSScriptRoot\..\LocaleArgCompleter.ps1" @PSBoundParameters
	})]
	[ValidateScript({ Test-Path "$PSScriptRoot\$_.fbs" -ErrorAction Ignore })]
	[string]$TemplateLocalize = 'zh-CN',
	[string]$OutLocalize = $Localize
)

# 以xml格式读取目标fbs文件
$Xml = [xml](Get-Content "$PSScriptRoot\$TemplateLocalize.fbs" -Encoding utf8)
$LocalizeData = [System.Collections.ArrayList](Get-Content -LiteralPath "$PSScriptRoot\$Localize.txt" -Encoding utf8)
# 添加Pop到LocalizeData
Add-Member -InputObject $LocalizeData -MemberType ScriptMethod -Name Pop -Value {
	$data = $this[0]
	$this.RemoveAt(0)
	$data
}
# 遍历xml节点
function XmlMapper($Node) {
	# 若节点有Text、Filter、Title属性，则加入到resultContent
	@('Text', 'Filter', 'Title') | ForEach-Object {
		if ($Node.$_) { $Node.$_ = $LocalizeData.Pop() }
	}
	# 遍历子节点
	$Node.ChildNodes | ForEach-Object { XmlMapper $_ }
}
XmlMapper $Xml

# 保存
$XmlWriterSettings = New-Object System.Xml.XmlWriterSettings
$XmlWriterSettings.Encoding = New-Object System.Text.UTF8Encoding $false
$XmlWriterSettings.Indent = $true
$XmlWriterSettings.IndentChars = "`t"
$XmlWriterSettings.NewLineChars = "`n"
$XmlWriter = [System.XML.XmlWriter]::Create("$PSScriptRoot\$OutLocalize.fbs", $XmlWriterSettings)
$Xml.Save($XmlWriter)
$XmlWriter.Flush()
$XmlWriter.Close()
