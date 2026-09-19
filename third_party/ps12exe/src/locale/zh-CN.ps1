@{
	# 语言元数据
	LangName                     = "简体中文"
	LangID                       = "zh-CN"

	# 右键菜单 / 外壳
	CompileTitle                 = "编译到 EXE"
	OpenInGUI                    = "在 ps12exeGUI 中打开"
	GUICfgFileDesc               = "ps12exe GUI 配置文件"
	VSCodeExtensionInstalling    = "正在为 {0} 安装 ps12exe 扩展……"
	VSCodeExtensionInstallFailed = "无法为 {0} 安装 ps12exe 扩展（可能尚未发布）：{1}"
	VSCodeExtensionUninstalling  = "正在为 {0} 卸载 ps12exe 扩展……"
	VSCodeExtensionUninstallFailed = "无法为 {0} 卸载 ps12exe 扩展：{1}"

	# GUI 通用
	ErrorHead                    = "错误："
	CompileResult                = "编译结果"
	DefaultResult                = "完成！"
	AskSaveCfg                   = "需要保存配置文件吗？"
	AskSaveCfgTitle              = "保存配置文件"
	CfgFileLabelHead             = "配置文件："

	# Web 服务器
	ServerStarted                = "HTTP 服务器已启动！"
	ServerStopped                = "HTTP 服务器已停止！"
	ServerStartFailed            = "HTTP 服务器启动失败！"
	TryRunAsRoot                 = "请尝试以管理员身份运行。"
	ServerListening              = "访问地址："
	ExitServerTip                = "您随时可以按 Ctrl+C 退出服务器"

	# 控制台帮助 - ps12exe
	ConsoleHelpData              = @{
		title      = "用法："
		Usage      = "[input |] ps12exe [[-inputFile] '<文件名|url>' | -Content '<脚本>'] [-outputFile '<文件名>']
	[-App @{Windowed=`$true; Silence=@('Output','Error'); OutputEncoding='UTF8'|'UTF16LE'|'Default';
	VisualStyles=`$true; ExitOnCancel=`$true; CredentialGUI=`$true; DpiAware=`$true; WinFormsDpiAware=`$true}]
	[-Os @{Admin=`$true; ModernOS=`$true; LongPaths=`$true; Virtualize=`$true}]
	[-Build @{Target='Framework4.0'|'Framework2.0'|'Core'; Platform='AnyCpu'|'x64'|'x86'; Apartment='STA'|'MTA';
	Culture='<区域>'; Options='<选项>'; KeepSource=`$true; Minify={<scriptblock>}; TempDir='<文件夹>'}]
	[-Resources @{Icon='<文件名|url>'; Title='<标题>'; Description='<简介>'; Company='<公司>';
	Product='<产品>'; Copyright='<版权>'; Trademark='<水印>'; Version='<版本>'}]
	[-Signing @{Certificate='<PFX文件路径>'; Password='<PFX密码>'; Thumbprint='<证书指纹>'; Timestamp='<时间戳服务器>'}]
	[-PreprocessOnly] [-Golf] [-Sandbox] [-NoUpdateCheck] [-Locale '<语言代码>'] [-ConfigFile] [-help]"
		PrarmsData = [ordered]@{
			input            = "PowerShell 脚本文件内容的字符串，与 ``-Content`` 相同。"
			inputFile        = "要转换为可执行文件的 PowerShell 脚本路径或 URL（文件须为 UTF-8 或 UTF-16 编码）。"
			Content          = "要转换为可执行文件的 PowerShell 脚本内容。"
			outputFile       = "输出可执行文件路径或文件夹；默认与输入文件同路径并添加 ``.exe`` 扩展名。"
			App              = [ordered]@{
				Windowed         = "生成的可执行文件将是一个没有控制台窗口的 Windows Forms 应用程序。"
				Silence          = "要静默的输出流；可取 ``'Output'``、``'Verbose'``、``'Error'``、``'Warning'``、``'Debug'`` 中的一个或多个，或 ``'*'`` 表示全部。"
				OutputEncoding   = "控制台输出编码；``'Default'``、``'UTF8'`` 或 ``'UTF16LE'``。"
				VisualStyles     = "为 GUI 应用程序启用视觉样式（默认 `` `$true ``）。"
				ExitOnCancel     = "当在 ``Read-Host`` 输入框中选择 Cancel 或 ``'X'`` 时退出程序。"
				CredentialGUI    = "在控制台模式下使用 GUI 提示凭据。"
				DpiAware         = "将编译的可执行文件标记为 DPI 感知。"
				WinFormsDpiAware = "让 WinForms 使用 DPI 缩放（需要 Windows 10 和 .Net 4.7 或更高版本）。"
			}
			Os               = [ordered]@{
				Admin      = "如果启用了 UAC，编译的可执行文件只能在提升的上下文中运行（如果需要，会出现 UAC 对话框）。"
				ModernOS   = "使用最新 Windows 版本的功能（执行 ``[Environment]::OSVersion`` 以查看差异）。"
				LongPaths  = "如果在 OS 上启用，启用长路径（``> 260`` 个字符）（仅适用于 Windows 10 或更高版本）。"
				Virtualize = "已激活应用程序虚拟化（强制 x86 运行时）。"
			}
			Build            = [ordered]@{
				Target     = "目标运行时版本，默认为 ``'Framework4.0'``，支持 ``'Framework2.0'`` 与 ``'Core'``；``'Core'`` 编译为 PowerShell Core (.NET) 可执行程序（需要编译机与目标机都装有 PowerShell Core 与 .NET，且产物体积大很多）。"
				Platform   = "仅为特定运行时编译。可能的值为 ``'AnyCpu'``、``'x64'`` 和 ``'x86'``。"
				Apartment  = "``'STA'``（单线程单元）或 ``'MTA'``（多线程单元）模式。"
				Culture    = "编译的可执行文件的文化。如果未指定，则为当前用户文化。"
				Options    = "额外的编译器选项（参见 ``https://msdn.microsoft.com/en-us/library/78f4aasd.aspx``）。"
				KeepSource = "创建有助于调试的信息。"
				Minify     = "在编译之前缩小脚本的脚本块。"
				TempDir    = "存储临时文件的目录（默认为 ``%temp%`` 中随机生成的临时目录）。"
			}
			Resources        = [ordered]@{
				Icon        = "可执行文件的图标；可以是图标文件路径或 URL。"
				Title       = "可执行文件的标题（文件说明）。"
				Description = "可执行文件的简要描述。"
				Company     = "可执行文件的公司名称。"
				Product     = "可执行文件的产品名称。"
				Copyright   = "可执行文件的版权声明。"
				Trademark   = "可执行文件的商标信息。"
				Version     = "可执行文件的版本号（例如 ``'1.0.0.0'``）。"
			}
			Signing          = [ordered]@{
				Certificate = "PFX 证书文件路径；必须指定 ``Certificate`` 或 ``Thumbprint`` 之一。"
				Password    = "PFX 证书的密码。"
				Thumbprint  = "证书指纹；必须指定 ``Certificate`` 或 ``Thumbprint`` 之一。"
				Timestamp   = "代码签名使用的时间戳服务器 URL。"
			}
			PreprocessOnly   = "预处理输入脚本并在不编译的情况下返回它"
			Golf             = "启用golf模式，添加缩写和常用函数"
			Sandbox          = "在额外保护下编译脚本，阻止访问本机文件。"
			NoUpdateCheck    = "跳过ps12exe的新版本检查"
			Locale           = "界面与消息所使用的语言代码。"
			ConfigFile       = "写一个配置文件（``<outputfile>.exe.config``）"
			Help             = "显示此帮助信息"
		}
	}

	# 控制台帮助 - GUI
	GUIHelpData                  = @{
		title      = "用法："
		Usage      = @"
ps12exeGUI [[-ConfigFile] '<配置文件>'] [-PS1File '<脚本文件>'] [-Locale '<语言代码>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]

ps12exeGUI [[-PS1File] '<脚本文件>'] [-Locale '<语言代码>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]
"@
		PrarmsData = [ordered]@{
			ConfigFile = "要加载的配置文件。"
			PS1File    = "要编译的脚本文件。"
			Locale     = "要使用的语言代码。"
			UIMode     = "界面模式。"
			help       = "显示此帮助信息。"
		}
	}

	# 控制台帮助 - 右键菜单
	SetContextMenuHelpData       = @{
		title      = "用法："
		Usage      = "Set-ps12exeContextMenu [[-action] 'enable'|'disable'|'reset'] [-Locale '<语言代码>'] [-SkipEditorExtension] [-help]"
		PrarmsData = [ordered]@{
			action              = "要执行的操作。"
			Locale              = "要使用的语言代码。"
			SkipEditorExtension	= "跳过向检测到的编辑器安装或卸载 ps12exe VS Code 扩展。"
			help                = "显示此帮助信息。"
		}
	}

	# 控制台帮助 - Web 服务器
	WebServerHelpData            = @{
		title      = "用法："
		Usage      = "Start-ps12exeWebServer [[-HostUrl] '<url>'] [-MaxCompileThreads '<uint>'] [-MaxCompileTime '<uint>']
	[-ReqLimitPerMin '<uint>'] [-MaxCachedFileSize '<uint>'] [-MaxScriptFileSize '<uint>'] [-CacheDir '<路径>']
	[-Locale '<语言代码>'] [-help]"
		PrarmsData = [ordered]@{
			HostUrl           = "要注册的 HTTP 服务器地址。"
			MaxCompileThreads = "最大编译线程数。"
			MaxCompileTime    = "最大编译时间（秒）。"
			ReqLimitPerMin    = "每个IP每分钟的请求限制。"
			MaxCachedFileSize = "最大缓存文件大小。"
			MaxScriptFileSize = "最大脚本文件大小。"
			CacheDir          = "缓存目录。"
			Locale            = "服务器端记录要使用的语言代码。"
			help              = "显示此帮助信息。"
		}
	}

	# 控制台帮助 - exe21sp
	exe21spHelpData              = @{
		title      = "用法："
		Usage      = "[input |] exe21sp [[-inputFile] '<exe路径或url>'] [-outputFile '<输出ps1路径>'] [-help]"
		PrarmsData = [ordered]@{
			input      = "要反编译的 ps12exe 生成的 exe 的路径或 URL，与``-inputFile``相同。"
			inputFile  = "要反编译的 ps12exe 生成的 exe 的路径或 URL。"
			outputFile = "可选；写出还原脚本的 ps1 文件路径，不指定则在被重定向时输出到标准输出，否则写入同目录下 ``<exe>.ps1``。"
			help       = "显示此帮助信息。"
		}
	}

	# 编译过程与错误消息
	CompilingI18nData            = @{
		# 输入与预处理
		NoneInput                                 = "未指定输入文件！"
		BothInputAndContentSpecified              = "不能同时输入文件和内容！"
		ReadingFile                               = "正在读取{0}，{1}字节"
		ReadingScriptDone                         = "读取{0}完成，正在开始预处理..."
		PreprocessDone                            = "预处理输入脚本完成"
		PreprocessedScriptSize                    = "预处理脚本 -> {0}字节"
		PreprocessScriptDone                      = "预处理{0}完成"
		PreprocessOnlyDone                        = "预处理完成"
		PreprocessUnknownIfCondition              = "未知条件：{0}`n假定为 false."
		PreprocessNestedIfDeadCode                = "嵌套的 #_if {0} 位于 #_if {1} 内：外层条件已决定该分支，另一支是死代码。"
		PreprocessMissingEndIf                    = "缺少endif：{0}"
		PreprocessPsexeBranchCode                 = "#_if PSEXE 分支中的代码既不是 #_!! 也不是注释，直接运行脚本时也会执行。"
		PreprocessPsscriptBranchBang              = "#_if PSScript 分支中的 #_!! 在直接运行脚本时是注释；这里应使用普通代码。"
		# 压缩
		MinifyingScript                           = "正在压缩脚本..."
		MinifyedScriptSize                        = "压缩脚本 -> {0}字节"
		MinifyerError                             = "压缩器错误：{0}"
		MinifyerFailedUsingOriginalScript         = "压缩器失败，使用原始脚本。"
		# 参数与选项冲突
		CombinedArg_Virtualize_requireAdmin       = "-Os @{Virtualize=`$true} 不能与 -Os @{Admin=`$true} 一起使用"
		CombinedArg_Virtualize_supportOS          = "-Os @{Virtualize=`$true} 不能与 -Os @{ModernOS=`$true} 一起使用"
		CombinedArg_Virtualize_longPaths          = "-Os @{Virtualize=`$true} 不能与 -Os @{LongPaths=`$true} 一起使用"
		CombinedArg_NoConfigFile_LongPaths        = "强制生成配置文件，因为选项 -Os @{LongPaths=`$true} 需要此配置文件"
		CombinedArg_NoConfigFile_winFormsDPIAware = "强制生成配置文件，因为选项 -App @{WinFormsDpiAware=`$true} 需要此配置文件"
		InvalidResourceParam                      = "参数 -Resources 的无效Key：{0}"
		InvalidArchitecture                       = "无效的平台 {0}，使用 AnyCpu"
		# 语法与文件
		InputSyntaxError                          = "脚本语法错误！"
		SyntaxErrorLineStart                      = "第{0}行 第{1}列："
		IdenticalInputOutput                      = "输入文件与输出文件相同！"
		TempFileMissing                           = "找不到临时文件{0}！"
		ReadFileFailed                            = "读取文件失败：{0}"
		# 命令与类型检查
		SomeCmdletsMayNotAvailable                = "使用了可能会在运行时不可用的命令 {0}，确保已检查它们！"
		SomeNotFoundCmdlets                       = "使用了未知的命令 {0}"
		SomeTypesMayNotAvailable                  = "使用了可能会在运行时不可用的类型 {0}，确保已检查它们！"
		# 编译与输出
		CompilingFile                             = "编译中..."
		CompilationFailed                         = "编译失败！"
		OutputFileNotWritten                      = "未写入输出文件 {0}"
		CompiledFileSize                          = "已编译文件 -> {0}字节"
		OutputPath                                = "路径：{0}"
		ConfigFileCreated                         = "创建了 EXE 的配置文件"
		SourceFileCopied                          = "已复制用于调试的源文件名：{0}"
		# 常量与 TinySharp
		ConstEvalStart                            = "正在计算常量..."
		ConstEvalDone                             = "计算常量完成 -> {0}字节"
		ConstEvalTooLongFallback                  = "常量结果太长，退回正常程序框架"
		ConstEvalTimeoutFallback                  = "常量计算{0}秒，超时。退回正常程序框架"
		ConstEvalThrowErrorFallback               = "常量计算抛出错误，退回正常程序框架"
		ConstEvalNotConstFallback                 = "脚本声明自己不是常量，退回正常程序框架"
		TryingTinySharpCompile                    = "结果为常量，尝试 TinySharp 编译器..."
		TinySharpFailedFallback                   = "TinySharp 编译器错误，退回正常程序框架"
		ForceX86byVirtualization                  = "已激活应用程序虚拟化，强制使用x86平台。"
		# 图标与资源
		IconFileNotFound                          = "找不到图标文件：{0}"
		ConvertingImageToIcon                     = "正在将图片转换为图标格式..."
		ImageConvertedToIcon                      = "图片已转换为图标：{0}"
		ImageConversionFailed                     = "图片转换失败：{0}"
		PleaseUseIcoFile                          = "请使用 .ico 文件代替 {0}"
		# 代码签名
		SigningExecutable                         = "正在签名可执行文件..."
		ExecutableSignedSuccessfully              = "可执行文件签名成功。"
		SigningStatusNotValid                     = "签名状态无效：{0} - {1}"
		CertificateNotFoundOrInvalidPassword      = "证书未找到或密码无效。"
		SigningFailed                             = "签名失败：{0}"
		# 版本与异常
		NewVersionAvailable                       = "ps12exe有了新版本：{0}！"
		TryUpgrade                                = "最新版本是{0}，尝试升级?"
		EnterToSubmitIssue                        = "如需帮助，按回车提交issue。"
		OopsSomethingWentWrong                    = "我去，出错了。"
		CoreCompilePublishing                     = "正在使用 .NET SDK 发布单文件可执行程序..."
		CoreCompileNeedDotnet                     = "PowerShell Core 编译需要 .NET SDK（dotnet）。请安装它，或传入 -Build @{Target='Framework4.0'}。"
		CoreCompileUnsupported                    = "PowerShell Core 编译器暂不支持以下选项：{0}"
		CoreCompileNeedPwsh                       = "当前是 Windows PowerShell；-Build @{Target='Core'} 需要安装 PowerShell Core (pwsh) 并加入 PATH。"
		CoreCompileNeedWindowsPowerShell          = "未找到 Windows PowerShell；请传入 -Build @{Target='Core'} 来编译 PowerShell Core 可执行程序。"
		CoreCompileNeedPwshHost                   = "已编译的 ps12exe.exe 无法编译 PowerShell Core 程序；请在 pwsh 下通过模块/脚本运行 ps12exe。"
		CoreCompileHint                           = "如果这是 PowerShell Core 专属脚本，请传入 -Build @{Target='Core'}（需要编译机和目标机都安装 PowerShell Core 与 .NET，且产物体积大很多）。"
		# 访客模式与 Pragma
		GuestModeFileTooLarge                     = "文件{0}太大，无法读取。"
		GuestModeIconFileTooLarge                 = "图标{0}太大，无法读取。"
		GuestModeFtpNotSupported                  = "沙箱模式不支持FTP。"
		UnknownPragma                             = "未知的 pragma：{0}"
		UnknownPragmaBadParameterType             = "未知的pragma：{0}，无法分析类型{1}。"
		UnknownPragmaBoolValue                    = "未知的pragma值：{0}，无法将其视为bool。"
		PragmaUnsafeExpression                    = "pragma {0} 中的表达式不安全：{1}"
		DllExportDelNoneTypeArg                   = "{0}：{1}是无类型参数，假设它是字符串。"
		DllExportUsing                            = "您正在使用 #_DllExport，此宏尚在开发中，尚未支持。"
	}

	# Web 服务器运行时消息
	WebServerI18nData            = @{
		CompilingUserInput  = "正在编译用户输入：{0}"
		EmptyResponse       = "处理请求时未找到数据，返回空响应"
		InputTooLarge413    = "用户输入太大，返回413错误"
		ReqLimitExceeded429 = "IP {0} 超过每分钟{1}次请求的限制，返回429错误"
	}

	# ps12exe 交互模式（与 exe21sp 交互模式用词统一）
	InteractI18nData             = @{
		# 模式与欢迎
		ModeName                    = "交互模式"
		Welcome                     = "欢迎使用 ps12exe 交互模式。可随时按 Ctrl+C 退出。"
		Prompt                      = " >> "
		ExitMessage                 = "已退出交互模式。"
		# 输入文件
		EnterInputFile              = "请输入输入文件路径或URL："
		InvalidInputFile            = "不是有效的 PS1 文件路径，请重新输入："
		FileDoesNotExist            = "文件不存在。"
		InvalidExtension            = "文件必须具有 '.ps1'、'.psd1' 或 '.tmp' 扩展名。"
		# 输出文件
		EnterOutputFile             = "请输入输出文件路径（留空则使用同目录下 <ps1>.exe）："
		OutputFileExtensionError    = "输出文件必须使用 '.exe' 扩展名。已为您自动添加。"
		# 附加信息
		AddAdditionalInfo           = "是否添加附加信息 (图标、版本等)？"
		AdditionalInfoPrompt        = "[Y/N]"
		CollectingInfo              = "请输入附加信息 (留空则跳过)。"
		IconPath                    = "图标文件路径或URL (支持 .ico, .png, .jpg, .jpeg, .bmp 等，留空则跳过)："
		InvalidIconExtension        = "图标文件必须为 .ico 格式。此项已忽略。"
		IconDoesNotExist            = "图标文件不存在，请重新输入。"
		EnterTitle                  = "标题"
		EnterDescription            = "描述"
		EnterCompany                = "公司名称"
		EnterProduct                = "产品名称"
		EnterCopyright              = "版权"
		EnterTrademark              = "商标"
		EnterResourcePrompt         = "请输入{0}："
		Version                     = "版本 (示例：1.0.0.0)："
		InvalidVersionFormat        = "版本格式无效。已忽略。"
		SkippingAdditionalInfo      = "已跳过附加信息。"
		# 编译选项
		CompileAsGui                = "是否作为 GUI 应用程序编译 (无控制台)？"
		RequireAdmin                = "是否需要管理员权限？"
		EnableCodeSigning           = "是否启用代码签名？"
		# 代码签名
		EnterCertificatePath        = "证书路径或URL (.pfx，留空则跳过)："
		InvalidCertificateExtension = "证书文件必须为 .pfx 格式，请重新输入。"
		CertificateDoesNotExist     = "证书文件不存在，请重新输入。"
		EnterCertificatePassword    = "证书密码 (留空则跳过)："
		EnterCertificateThumbprint  = "证书指纹 (留空则跳过)："
		EnterTimestampServer        = "时间戳服务器 (留空则使用默认值)："
		SkippingCodeSigning         = "已跳过代码签名。"
		# 执行与结果
		BuildingCommand             = "正在生成命令..."
		ExecutingCommand            = "正在执行命令..."
		CompileSuccess              = "文件编译成功。"
		CompileFailed               = "编译失败，退出代码：{0}"
		CompileFailedException      = "编译时发生错误：{0}"
		CompileAnother              = "是否要编译下一个文件？"
		Exiting                     = "正在退出交互模式。"
	}

	# exe21sp 交互模式（与 ps12exe 交互模式键顺序、键名统一）
	exe21spInteractI18nData      = @{
		ModeName                 = "交互模式"
		Welcome                  = "欢迎使用 exe21sp 交互模式。可随时按 Ctrl+C 退出。"
		EnterInputFile           = "请输入输入 exe 的路径或 URL："
		Prompt                   = " >> "
		ExitMessage              = "已退出交互模式。"
		InvalidInputFile         = "请输入有效的 exe 路径或 URL。"
		FileDoesNotExist         = "文件不存在。"
		EnterOutputFile          = "请输入输出文件路径（留空则使用同目录下 <exe>.ps1）："
		OutputFileExtensionError = "输出文件须为 .ps1 扩展名，已自动添加。"
		AdditionalInfoPrompt     = "[Y/N]"
		ConvertAnother           = "是否继续转换其他 exe？"
		Exiting                  = "正在退出交互模式。"
	}

	# exe21sp 反编译消息
	exe21spI18nData              = @{
		NoneInput                    = "未指定输入文件！"
		TinySharpNoTextSection       = "该可执行文件是 .NET 程序集，但不符合 TinySharp 布局（无 .text 节）。"
		TinySharpTextSectionEmpty    = "该可执行文件是 .NET 程序集，但不符合 TinySharp 布局（.text 节为空）。"
		TinySharpCannotReadText      = "该可执行文件是 .NET 程序集，但不符合 TinySharp 布局（无法读取 .text）。"
		TinySharpPayloadNotRecovered = "该可执行文件是 .NET 程序集，但不符合 TinySharp 布局；无法恢复脚本负载。"
		NoEmbeddedScript             = '在 "{0}" 中未找到嵌入脚本（不是 ps12exe 构建的 exe，或无法恢复负载）。'
		CoreExtractNeedsPwsh         = '该 exe 的负载是 Brotli 压缩的（PowerShell Core 构建）。请安装 PowerShell 7 (pwsh) 以便 exe21sp 解压。'
		FileNotFound                 = "文件不存在：{0}"
		InputUrlFailed               = "无法从 URL 读取：{0}"
	}
}
