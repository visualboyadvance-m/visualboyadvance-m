function GetAssembly($name, $otherinfo) {
	$n = New-Object System.Reflection.AssemblyName(@($name, $otherinfo) -ne $null -join ",")
	try {
		[System.AppDomain]::CurrentDomain.Load($n).Location
	}
	catch {
		$error.RemoveAt(0)
	}
}
$referenceAssembies = if ($targetRuntime -eq 'Framework2.0') {
	#_if PSScript
		powershell -version 2.0 -NoProfile -OutputFormat xml -file $PSScriptRoot/RuntimePwsh2.0/RefDlls.ps1 $(if ($noConsole) { '-noConsole' })
	#_else
		#_include_as_value Pwsh2RefDllsGetterCodeStr $PSScriptRoot/RuntimePwsh2.0/RefDlls.ps1
		#_!! powershell -version 2.0 -NoProfile -OutputFormat xml -Command "&{$Pwsh2RefDllsGetterCodeStr}$(if($noConsole){' -noConsole'})"
	#_endif
}
else {
	if ($PSVersionTable.PSEdition -eq "Core") {
		# Core 走 dotnet publish，不需要引用程序集列表；SMA 只用于 $isPwsh20Sma 判断。
		GetAssembly "System.Management.Automation"
	}
	else {
		# 绝不要直接使用 System.Private.CoreLib.dll，因为它是netlib的内部实现，而不是公共API；[int].Assembly.Location 等基础类型的程序集也是它。
		GetAssembly "mscorlib"
		GetAssembly "System.IO.Compression" "Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089"
		GetAssembly "System.Management.Automation"

		# 如果 noConsole 为 true，则将 System.Windows.Forms.dll 和 System.Drawing.dll 加入引用程序集列表
		if ($noConsole) {
			GetAssembly "System.Windows.Forms" "Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089"
			GetAssembly "System.Drawing" "Version=4.0.0.0, Culture=neutral, PublicKeyToken=b03f5f7f11d50a3a"
		}

		GetAssembly "System.Core" "Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089"
		"System.dll" # 某种魔法
	}
}

$smaRef = @($referenceAssembies) | Where-Object { $_ -and ([IO.Path]::GetFileName($_) -ieq 'System.Management.Automation.dll') } | Select-Object -First 1
$isPwsh20Sma = $smaRef -and [Reflection.AssemblyName]::GetAssemblyName($smaRef).Version.Major -lt 3

# 目标框架版本供 constexpr.cs / default.cs 的 $TargetFramework 替换使用。
if ($isCoreTarget) {
	$Info = [System.Environment]::Version
	$TargetFramework = ".NETCore,Version=v$($Info.Major).$($Info.Minor)"
}
elseif ($isPwsh20Sma) {
	$TargetFramework = ".NETFramework,Version=v2.0"
}
else {
	$TargetFramework = ".NETFramework,Version=v4.7"
}

. $PSScriptRoot\BuildFrame.ps1

# 是否在脚本顶层使用了 $input：只有用到时才把重定向的标准输入逐行读成管道输入（issue 62）。只扫描脚本顶层（也就是会被包进 PSEXEMainFunction 的那个 $input）；函数、脚本块、类内部的 $input 是它们各自的管道输入，与宿主无关，不计入。解析失败/无 AST 时保守起见仍读取输入。
$ScriptUsesInput = $true
if ($AST) {
	$ScriptUsesInput = $false
	foreach ($inputVar in $AST.FindAll({
		param($node)
		$node -is [System.Management.Automation.Language.VariableExpressionAst] -and $node.VariablePath.UserPath -eq 'input'
	}, $true)) {
		$parentNode = $inputVar.Parent
		$inNestedScope = $false
		while ($parentNode) {
			if ($parentNode -is [System.Management.Automation.Language.FunctionDefinitionAst] -or
				$parentNode -is [System.Management.Automation.Language.ScriptBlockExpressionAst] -or
				$parentNode -is [System.Management.Automation.Language.TypeDefinitionAst]) {
				$inNestedScope = $true
				break
			}
			$parentNode = $parentNode.Parent
		}
		if (-not $inNestedScope) {
			$ScriptUsesInput = $true
			break
		}
	}
}

[string[]]$Constants = @()

$Constants += $threadingModel
if ($lcid) { $Constants += "culture" }
if ($noError) { $Constants += "noError" }
if ($noConsole) { $Constants += "noConsole" }
if ($noOutput) { $Constants += "noOutput" }
if ($noVerbose) { $Constants += "noVerbose" }
if ($noWarning) { $Constants += "noWarning" }
if ($noDebug) { $Constants += "noDebug" }
if ($resourceParams.version) { $Constants += "version" }
if ($resourceParams.Count) { $Constants += "Resources" }
if ($credentialGUI) { $Constants += "credentialGUI" }
if ($noVisualStyles) { $Constants += "noVisualStyles" }
if ($exitOnCancel) { $Constants += "exitOnCancel" }
if ($UNICODEEncoding) { $Constants += "UNICODEEncoding" }
if ($UTF8Encoding) { $Constants += "UTF8Encoding" }
if ($winFormsDPIAware) { $Constants += "winFormsDPIAware" }
if ($isPwsh20Sma) { $Constants += "Pwsh20" }
if ($ScriptUsesInput) { $Constants += "ReadInput" }
if ($AST -and $AST.ParamBlock) { $Constants += "ScriptHasParam" }
if ($StartupTiming) { $Constants += "StartupTiming" }

if (-not $TempDir) {
	$TempDir = $TempTempDir = [System.IO.Path]::GetTempPath() + [System.IO.Path]::GetRandomFileName()
	New-Item -Path $TempTempDir -ItemType Directory | Out-Null
}
$TempDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($TempDir)
# 脚本以未压缩的 main.ps1 资源内嵌；打包时整块负载还会再 gzip 一遍，这里先压反而不可压。
[byte[]]$scriptBytes = [System.Text.Encoding]::UTF8.GetBytes($Content)
$scriptPath = Join-Path $TempDir 'main.ps1'
[System.IO.File]::WriteAllBytes($scriptPath, $scriptBytes)
if ($iconFile -match "^(https?|ftp)://") {
	try {
		# 首先尝试从URL中获取文件扩展名
		$urlExtension = [System.IO.Path]::GetExtension([System.Uri]$iconFile).ToLower()

		# 如果URL中没有扩展名，尝试从Content-Type获取
		if (!$urlExtension) {
			$headResponse = Invoke-WebRequest $iconFile -Method Head -ErrorAction SilentlyContinue
			if ($headResponse) {
				$contentType = $headResponse.Headers.'Content-Type'
				if ($contentType) {
					# 从Content-Type映射到文件扩展名
					switch -Regex ($contentType) {
						"image/png" { $urlExtension = ".png" }
						"image/jpeg|image/jpg" { $urlExtension = ".jpg" }
						"image/gif" { $urlExtension = ".gif" }
						"image/bmp" { $urlExtension = ".bmp" }
						"image/x-icon|image/vnd.microsoft.icon|image/ico" { $urlExtension = ".ico" }
						"image/svg" { $urlExtension = ".svg" }
						default { $urlExtension = ".ico" }  # 默认使用.ico
					}
				}
			}
		}

		# 如果仍然没有扩展名，默认使用.ico
		if (!$urlExtension) { $urlExtension = ".ico" }

		if ($GuestMode) {
			if ((Invoke-WebRequest $iconFile -Method Head -ErrorAction SilentlyContinue).Headers.'Content-Length' -gt 1mb) {
				Write-I18n Error GuestModeIconFileTooLarge $iconFile -Category LimitsExceeded
				throw
			}
			if ($File -match "^ftp://") {
				Write-I18n Error GuestModeFtpNotSupported -Category ReadError
				throw
			}
		}
		$downloadedIconPath = "$TempDir\icon$urlExtension"
		Invoke-WebRequest -ErrorAction Stop -Uri $iconFile -OutFile $downloadedIconPath
		$iconFile = $downloadedIconPath
	}
	catch {
		Write-I18n Error IconFileNotFound $iconFile -Category ReadError
		throw
	}
}
elseif ($iconFile) {
	# 获取绝对路径，无论给出的是相对路径还是绝对路径
	$iconFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($iconFile)

	if (!(Test-Path $iconFile -PathType Leaf)) {
		Write-I18n Error IconFileNotFound $iconFile -Category ReadError
		throw
	}
}

if ($iconFile) {
	# 自动图片转换：检测非 .ico 后缀并自动转换
	$iconExtension = [System.IO.Path]::GetExtension($iconFile).ToLower()
	if ($iconExtension -ne ".ico") {
		Write-I18n Host ConvertingImageToIcon
		$sourceImage = $null
		$iconStream = $null
		$writer = $null

		try {
			Add-Type -AssemblyName System.Drawing

			$sourceImage = [System.Drawing.Image]::FromFile($iconFile)
			$tempIcoPath = [System.IO.Path]::Combine($TempDir, "converted_icon_$([Guid]::NewGuid().ToString()).ico")

			$sizes = @(16, 32, 48, 256)
			$iconStream = New-Object System.IO.MemoryStream
			$writer = New-Object System.IO.BinaryWriter($iconStream)

			$writer.Write([UInt16]0)  # 保留
			$writer.Write([UInt16]1)  # 类型（ICO）
			$writer.Write([UInt16]$sizes.Count)  # 图像数量

			$directoryOffset = $iconStream.Position
			$imageDataOffset = $directoryOffset + (16 * $sizes.Count)

			$imageData = @()
			foreach ($size in $sizes) {
				$bitmap = $null
				$graphics = $null
				$pngStream = $null

				try {
					$bitmap = New-Object System.Drawing.Bitmap($size, $size)
					$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
					$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
					$graphics.DrawImage($sourceImage, 0, 0, $size, $size)

					$pngStream = New-Object System.IO.MemoryStream
					$bitmap.Save($pngStream, [System.Drawing.Imaging.ImageFormat]::Png)
					$pngBytes = $pngStream.ToArray()

					$imageData += @{
						Size   = $size
						Data   = $pngBytes
						Offset = $imageDataOffset
						Length = $pngBytes.Length
					}
					$imageDataOffset += $pngBytes.Length
				}
				finally {
					if ($graphics) { $graphics.Dispose() }
					if ($pngStream) { $pngStream.Dispose() }
					if ($bitmap) { $bitmap.Dispose() }
				}
			}

			foreach ($imgData in $imageData) {
				$size = $imgData.Size
				$width = if ($size -eq 256) { 0 } else { $size }
				$height = if ($size -eq 256) { 0 } else { $size }

				$writer.Write([Byte]$width)
				$writer.Write([Byte]$height)
				$writer.Write([Byte]0)
				$writer.Write([Byte]0)
				$writer.Write([UInt16]1)
				$writer.Write([UInt16]32)
				$writer.Write([UInt32]$imgData.Length)
				$writer.Write([UInt32]$imgData.Offset)
			}

			foreach ($imgData in $imageData) {
				$writer.Write($imgData.Data)
			}

			$writer.Flush()
			[System.IO.File]::WriteAllBytes($tempIcoPath, $iconStream.ToArray())

			$iconFile = $tempIcoPath
			Write-I18n Host ImageConvertedToIcon $iconFile

		}
		catch {
			Write-I18n Warning ImageConversionFailed $_.Exception.Message
			Write-I18n Warning PleaseUseIcoFile $iconExtension
			# 转换失败时继续使用原文件，但可能会在后续步骤中失败
		}
		finally {
			if ($writer) { $writer.Dispose() }
			if ($iconStream) { $iconStream.Dispose() }
			if ($sourceImage) { $sourceImage.Dispose() }
		}
	}
}
