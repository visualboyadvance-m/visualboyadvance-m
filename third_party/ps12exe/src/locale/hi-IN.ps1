@{
	LangName                     = "हिंदी (हिंदी)"
	LangID                       = "hi-IN"
	# Right click Menu
	CompileTitle                 = "कॉम्पाइल करें EXE में"
	OpenInGUI                    = "ps12exeGUI में खोलें"
	GUICfgFileDesc               = "ps12exe GUI कॉन्फ़िगरेशन फ़ाइल"
	VSCodeExtensionInstalling    = "{0} के लिए ps12exe एक्सटेंशन इंस्टॉल किया जा रहा है..."
	VSCodeExtensionInstallFailed = "{0} के लिए ps12exe एक्सटेंशन इंस्टॉल नहीं हो सका (हो सकता है यह अभी प्रकाशित न हुआ हो): {1}"
	VSCodeExtensionUninstalling  = "{0} के लिए ps12exe एक्सटेंशन अनइंस्टॉल किया जा रहा है..."
	VSCodeExtensionUninstallFailed = "{0} के लिए ps12exe एक्सटेंशन अनइंस्टॉल नहीं हो सका: {1}"
	# Web Server
	ErrorHead                    = "त्रुटि:"
	CompileResult                = "कॉम्पाइल परिणाम"
	DefaultResult                = "पूरा हुआ!"
	AskSaveCfg                   = "क्या आप कॉन्फ़िगरेशन फ़ाइल को सहेजना चाहते हैं?"
	AskSaveCfgTitle              = "कॉन्फ़िगरेशन फ़ाइल सहेजें"
	CfgFileLabelHead             = "कॉन्फ़िगरेशन फ़ाइल:"
	# Console
	ServerStarted                = "HTTP सर्वर शुरू हो गया है!"
	ServerStopped                = "HTTP सर्वर बंद हो गया है!"
	ServerStartFailed            = "HTTP सर्वर शुरू करने में विफल रहा!"
	TryRunAsRoot                 = "कृपया प्रशासक के रूप में चलाने का प्रयास करें।"
	ServerListening              = "पहुंच का पता:"
	ExitServerTip                = "आप कभी भी Ctrl+C दबाकर सर्वर को बंद कर सकते हैं"
	# GUI
	ConsoleHelpData              = @{
		title      = "उपयोग:"
		Usage      = "[input |] ps12exe [[-inputFile] '<फ़ाइल नाम|url>' | -Content '<स्क्रिप्ट>'] [-outputFile '<फ़ाइल नाम>']
	[-App @{Windowed=`$true; Silence=@('Output','Error'); OutputEncoding='UTF8'|'UTF16LE'|'Default';
	VisualStyles=`$true; ExitOnCancel=`$true; CredentialGUI=`$true; DpiAware=`$true; WinFormsDpiAware=`$true}]
	[-Os @{Admin=`$true; ModernOS=`$true; LongPaths=`$true; Virtualize=`$true}]
	[-Build @{Target='Framework4.0'|'Framework2.0'|'Core'; Platform='AnyCpu'|'x64'|'x86'; Apartment='STA'|'MTA';
	Culture='<संस्कृति>'; Options='<विकल्प>'; KeepSource=`$true; Minify={<स्क्रिप्टब्लॉक>}; TempDir='<फ़ोल्डर>'}]
	[-Resources @{Icon='<फ़ाइल नाम|url>'; Title='<शीर्षक>'; Description='<सारांश>'; Company='<कंपनी>';
	Product='<उत्पाद>'; Copyright='<कॉपीराइट>'; Trademark='<नामकरण>'; Version='<संस्करण>'}]
	[-Signing @{Certificate='<PFX फ़ाइल पथ>'; Password='<PFX पासवर्ड>'; Thumbprint='<प्रमाणपत्र फ़िंगरप्रिंट>'; Timestamp='<समय चिह्न सर्वर>'}]
	[-PreprocessOnly] [-Golf] [-Sandbox] [-NoUpdateCheck] [-Locale '<भाषा कोड>'] [-ConfigFile] [-help]"
		PrarmsData = [ordered]@{
			input            = "PowerShell स्क्रिप्ट फ़ाइल की सामग्री का स्ट्रिंग, ``-Content`` के समान"
			inputFile        = "परिवर्तित करने के लिए PowerShell स्क्रिप्ट का पथ या URL (फ़ाइल UTF-8 या UTF-16 एन्कोड होनी चाहिए)।"
			Content          = "जिसे आप एक्सीक्यूटेबल फ़ाइल में परिवर्तित करना चाहते हैं, उस PowerShell स्क्रिप्ट की सामग्री"
			outputFile       = "लक्षित एक्सीक्यूटेबल फ़ाइल का नाम या फ़ोल्डर, डिफ़ॉल्ट रूप से ``inputFile`` के साथ ``'.exe'`` एक्सटेंशन के साथ"
			App              = [ordered]@{
				Windowed         = "निर्मित एक्सीक्यूटेबल फ़ाइल एक विंडोज फ़ॉर्म्स एप्लिकेशन होगी जिसमें कोई कंसोल विंडो नहीं होगी।"
				Silence          = "शांत किए जाने वाले आउटपुट स्ट्रीम; ``'Output'``, ``'Verbose'``, ``'Error'``, ``'Warning'``, ``'Debug'`` में से एक या अधिक, या सभी के लिए ``'*'``।"
				OutputEncoding   = "कंसोल आउटपुट एन्कोडिंग; ``'Default'``, ``'UTF8'`` या ``'UTF16LE'``।"
				VisualStyles     = "GUI एप्लिकेशन के लिए विजुअल स्टाइल सक्षम करें (डिफ़ॉल्ट `` `$true ``)।"
				ExitOnCancel     = "``Read-Host`` इनपुट बॉक्स में Cancel या ``'X'`` का चयन करते समय प्रोग्राम से बाहर निकलें।"
				CredentialGUI    = "कंसोल मोड में क्रेडेंशल के लिए GUI का उपयोग करें।"
				DpiAware         = "संकलित एक्सीक्यूटेबल फ़ाइल को DPI aware के रूप में चिह्नित करें।"
				WinFormsDpiAware = "WinForms को DPI स्केलिंग का उपयोग करने दें (Windows 10 और .Net 4.7 या इससे ऊपर की आवश्यकता है)।"
			}
			Os               = [ordered]@{
				Admin      = "अगर UAC सक्षम है, तो कॉम्पाइल की गई एक्सीक्यूटेबल फ़ाइल को सिर्फ उच्चाधिकार कांटेक्स्ट में चलाया जा सकेगा (आवश्यकता होने पर, UAC संवाद बॉक्स प्रकट होगा)।"
				ModernOS   = "नवीनतम Windows संस्करण की विशेषताओं का उपयोग करें (विभिन्नता देखने के लिए ``[Environment]::OSVersion`` का चालन करें)।"
				LongPaths  = "यदि ऑपरेटिंग सिस्टम पर सक्षम है, तो लंबी पथ (260 वर्ण से अधिक) को सक्षम करें (केवल Windows 10 या इससे ऊपर के लिए)।"
				Virtualize = "ऐप्लिकेशन वर्चुअलाईजेशन सक्रिय कर दिया गया है (x86 रनटाइम को बाध्य करता है)।"
			}
			Build            = [ordered]@{
				Target     = "लक्ष्य रनटाइम संस्करण, डिफ़ॉल्ट रूप से ``'Framework4.0'``; ``'Framework2.0'`` और ``'Core'`` समर्थित हैं। ``'Core'`` PowerShell Core (.NET) निष्पादन योग्य बनाता है (कंपाइल और लक्ष्य मशीन दोनों पर PowerShell Core और .NET आवश्यक; आउटपुट बहुत बड़ा होता है)।"
				Platform   = "केवल विशेष रनटाइम के लिए कॉम्पाइल करें। संभावित मान हैं ``'AnyCpu'``, ``'x64'`` और ``'x86'``।"
				Apartment  = "``'STA'`` या ``'MTA'`` मॉडल।"
				Culture    = "संकलित एक्सीक्यूटेबल फ़ाइल की संस्कृति। अगर निर्दिष्ट नहीं किया गया है, तो वर्तमान उपयोगकर्ता संस्कृति होगी।"
				Options    = "अतिरिक्त कंपाइलर विकल्प (देखें ``https://msdn.microsoft.com/en-us/library/78f4aasd.aspx``)।"
				KeepSource = "डीबगिंग के लिए मददगार जानकारी बनाएं।"
				Minify     = "कॉम्पाइल से पहले स्क्रिप्ट को छोटा करने के लिए स्क्रिप्ट ब्लॉक।"
				TempDir    = "अस्थायी फ़ाइलें संग्रहित करने का फ़ोल्डर (डिफ़ॉल्ट रूप से ``%temp%`` में यादृच्छिक फ़ोल्डर)।"
			}
			Resources        = [ordered]@{
				Icon        = "एक्सीक्यूटेबल का आइकन; एक फ़ाइल पथ या URL हो सकता है।"
				Title       = "एक्सीक्यूटेबल का शीर्षक (फ़ाइल विवरण)।"
				Description = "एक्सीक्यूटेबल का संक्षिप्त विवरण।"
				Company     = "एक्सीक्यूटेबल की कंपनी का नाम।"
				Product     = "एक्सीक्यूटेबल का उत्पाद नाम।"
				Copyright   = "एक्सीक्यूटेबल की कॉपीराइट सूचना।"
				Trademark   = "एक्सीक्यूटेबल की ट्रेडमार्क जानकारी।"
				Version     = "एक्सीक्यूटेबल का संस्करण संख्या (उदाहरण ``'1.0.0.0'``)।"
			}
			Signing          = [ordered]@{
				Certificate = "PFX प्रमाणपत्र फ़ाइल का पथ; ``Certificate`` या ``Thumbprint`` में से एक निर्दिष्ट करना आवश्यक है।"
				Password    = "PFX प्रमाणपत्र का पासवर्ड।"
				Thumbprint  = "प्रमाणपत्र फ़िंगरप्रिंट; ``Certificate`` या ``Thumbprint`` में से एक निर्दिष्ट करना आवश्यक है।"
				Timestamp   = "कोड साइनिंग के लिए उपयोग किए जाने वाले समय चिह्न सर्वर का URL।"
			}
			PreprocessOnly   = "इनपुट स्क्रिप्ट को प्रीप्रोसेस करें और इसे संकलित किए बिना वापस करें"
			Golf             = "गॉल्फ मोड सक्षम करें, संक्षिप्त रूप और सामान्य फ़ंक्शन जोड़ें"
			Sandbox          = "एक्सट्रा सुरक्षा के साथ स्क्रिप्ट को कॉम्पाइल करें, स्थानीय फ़ाइलों की पहुँच को टालें"
			NoUpdateCheck    = "ps12exe के नए संस्करण की जाँच छोड़ें"
			Locale           = "संदेशों के लिए भाषा कोड।"
			ConfigFile       = "एक कॉन्फ़िगरेशन फ़ाइल लिखें (``<आउटपुटफ़ाइल>.exe.config``)"
			Help             = "इस मदद सूचना को दिखाएँ"
		}
	}
	GUIHelpData                  = @{
		title      = "उपयोग:"
		Usage      = @"
ps12exeGUI [[-ConfigFile] '<कॉन्फ़िगरेशन फ़ाइल>'] [-PS1File '<स्क्रिप्ट फ़ाइल>'] [-Locale '<भाषा कोड>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]

ps12exeGUI [[-PS1File] '<स्क्रिप्ट फाइल>'] [-Locale '<भाषा कोड>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]
"@
		PrarmsData = [ordered]@{
			ConfigFile	= "लोड करने के लिए कॉन्फ़िगरेशन फ़ाइल।"
			PS1File    = "कंपाइल करने के लिए स्क्रिप्ट फ़ाइल।"
			Locale     = "उपयोग किया जाने वाला भाषा कोड।"
			UIMode     = "UI मोड।"
			help       = "इस मदद सूचना को दिखाएँ।"
		}
	}
	SetContextMenuHelpData       = @{
		title      = "उपयोग:"
		Usage      = "Set-ps12exeContextMenu [[-action] 'enable'|'disable'|'reset'] [-Locale '<भाषा कोड>'] [-SkipEditorExtension] [-help]"
		PrarmsData = [ordered]@{
			action              = "क्रिया का कार्यान्वयन।"
			Locale              = "उपयोग किए जाने वाले भाषा कोड।"
			SkipEditorExtension	= "पहचाने गए संपादकों में ps12exe VS Code एक्सटेंशन को इंस्टॉल या अनइंस्टॉल करना छोड़ें।"
			help                = "इस मदद सूचना को दिखाएँ।"
		}
	}
	WebServerHelpData            = @{
		title      = "उपयोग:"
		Usage      = "Start-ps12exeWebServer [[-HostUrl] '<url>'] [-MaxCompileThreads '<uint>'] [-MaxCompileTime '<uint>']
	[-ReqLimitPerMin '<uint>'] [-MaxCachedFileSize '<uint>'] [-MaxScriptFileSize '<uint>'] [-CacheDir '<पथ>']
	[-Locale '<भाषा कोड>'] [-help]"
		PrarmsData = [ordered]@{
			HostUrl           = "रजिस्टर करने के लिए HTTP सर्वर पता।"
			MaxCompileThreads = "अधिकतम कॉम्पाइल धागों की संख्या।"
			MaxCompileTime    = "अधिकतम कॉम्पाइल समय (सेकंड)।"
			ReqLimitPerMin    = "फ़ाइल प्रति मिनट की अनुरोध सीमा।"
			MaxCachedFileSize = "अधिकतम कैश फ़ाइल का आकार।"
			MaxScriptFileSize = "अधिकतम स्क्रिप्ट फ़ाइल का आकार।"
			CacheDir          = "अधिकतम कैश फ़ाइल का डाइरेक्टरी पथ।"
			Locale            = "सर्वर साइड रिकॉर्ड करने के लिए उपयोग किए जाने वाले भाषा कोड।"
			help              = "इस मदद सूचना को दिखाएँ।"
		}
	}
	exe21spHelpData              = @{
		title      = "उपयोग:"
		Usage      = "[input |] exe21sp [[-inputFile] '<exe पथ या url>'] [-outputFile '<.ps1 पथ>'] [-help]"
		PrarmsData = [ordered]@{
			input      = "ps12exe द्वारा निर्मित exe का पथ या URL, ``-inputFile`` के समान।"
			inputFile  = "ps12exe द्वारा निर्मित exe का पथ या URL (डीकंपाइल करने के लिए)।"
			outputFile = "वैकल्पिक; पुनर्प्राप्त स्क्रिप्ट लिखने का पथ। छोड़ने पर: रीडायरेक्ट होने पर stdout, वरना समान फ़ोल्डर में ``<exe>.ps1`` में लिखता है।"
			help       = "यह सहायता दिखाएँ।"
		}
	}
	CompilingI18nData            = @{
		NewVersionAvailable                       = "ps12exe का नया संस्करण उपलब्ध है: {0}!"
		NoneInput                                 = "कोई इनपुट फ़ाइल निर्दिष्ट नहीं है!"
		BothInputAndContentSpecified              = "इनपुट फ़ाइल और सामग्री का उपयोग एक साथ नहीं किया जा सकता है!"
		PreprocessDone                            = "इनपुट स्क्रिप्ट को प्रीप्रोसेस करना पूर्ण हुआ"
		PreprocessedScriptSize                    = "प्रीप्रोसेस्ड स्क्रिप्ट -> {0} बाइट्स"
		MinifyingScript                           = "स्क्रिप्ट को छोटा किया जा रहा है..."
		MinifyedScriptSize                        = "छोटा किया गया स्क्रिप्ट -> {0} बाइट्स"
		MinifyerError                             = "छोटा करने वाला त्रुटि: {0}"
		MinifyerFailedUsingOriginalScript         = "छोटा करने वाला विफल, मूल स्क्रिप्ट का उपयोग करना।"
		TempFileMissing                           = "अस्थायी फ़ाइल {0} नहीं मिली!"
		PreprocessOnlyDone                        = "इनपुट स्क्रिप्ट को प्रीप्रोसेस करना पूर्ण हुआ"
		InvalidResourceParam                      = "पैरामीटर -Resources में एक अमान्य कुंजी है: {0}"
		InputSyntaxError                          = "स्क्रिप्ट में वाक्य रचना त्रुटि!"
		SyntaxErrorLineStart                      = "पंक्ति {0}, स्तंभ {1}:"
		IdenticalInputOutput                      = "इनपुट फ़ाइल आउटपुट फ़ाइल के समान है!"
		CombinedArg_Virtualize_requireAdmin       = "-Os @{Virtualize=`$true} का उपयोग -Os @{Admin=`$true} के साथ नहीं किया जा सकता"
		CombinedArg_Virtualize_supportOS          = "-Os @{Virtualize=`$true} का उपयोग -Os @{ModernOS=`$true} के साथ नहीं किया जा सकता"
		CombinedArg_Virtualize_longPaths          = "-Os @{Virtualize=`$true} का उपयोग -Os @{LongPaths=`$true} के साथ नहीं किया जा सकता"
		CombinedArg_NoConfigFile_LongPaths        = "एक कॉन्फ़िगरेशन फ़ाइल के निर्माण को मजबूर करना, क्योंकि विकल्प -Os @{LongPaths=`$true} को इसकी आवश्यकता होती है"
		CombinedArg_NoConfigFile_winFormsDPIAware = "एक कॉन्फ़िगरेशन फ़ाइल के निर्माण को मजबूर करना, क्योंकि विकल्प -App @{WinFormsDpiAware=`$true} को इसकी आवश्यकता होती है"
		SomeCmdletsMayNotAvailable                = "उपयोग किए गए Cmdlets {0} लेकिन रनटाइम में उपलब्ध नहीं हो सकते हैं, सुनिश्चित करें कि आपने उनकी जांच की है!"
		SomeNotFoundCmdlets                       = "अज्ञात कार्यों {0} का उपयोग किया गया"
		SomeTypesMayNotAvailable                  = "उपयोग किए गए टाइप {0} रनटाइम में उपलब्ध नहीं हो सकते हैं, सुनिश्चित करें कि आपने उनकी जांच की है!"
		CompilingFile                             = "संकलन..."
		CompilationFailed                         = "संकलन विफल!"
		OutputFileNotWritten                      = "आउटपुट फ़ाइल {0} नहीं लिखी गई"
		CompiledFileSize                          = "संकलित फ़ाइल लिखी गई -> {0} बाइट्स"
		OopsSomethingWentWrong                    = "ओह, कुछ गलत हो गया।"
		TryUpgrade                                = "नवीनतम संस्करण {0} है, इसे अपग्रेड करने का प्रयास करें?"
		EnterToSubmitIssue                        = "मदद के लिए, कृपया एक समस्या सबमिट करने के लिए Enter दबाएं।"
		GuestModeFileTooLarge                     = "फ़ाइल {0} पढ़ने के लिए बहुत बड़ी है।"
		GuestModeIconFileTooLarge                 = "आइकन {0} पढ़ने के लिए बहुत बड़ा है।"
		GuestModeFtpNotSupported                  = "FTP को Sandbox मोड में समर्थित नहीं किया जाता है।"
		IconFileNotFound                          = "आइकन फ़ाइल नहीं मिली: {0}"
		ConvertingImageToIcon                     = "छवि को आइकन प्रारूप में बदल रहा है..."
		ImageConvertedToIcon                      = "छवि को आइकन में बदल दिया गया: {0}"
		ImageConversionFailed                     = "छवि रूपांतरण विफल: {0}"
		PleaseUseIcoFile                          = "कृपया {0} के बजाय .ico फ़ाइल का उपयोग करें"
		SigningExecutable                         = "निष्पादन योग्य पर हस्ताक्षर कर रहा है..."
		ExecutableSignedSuccessfully              = "निष्पादन योग्य सफलतापूर्वक हस्ताक्षरित।"
		SigningStatusNotValid                     = "हस्ताक्षर स्थिति मान्य नहीं है: {0} - {1}"
		CertificateNotFoundOrInvalidPassword      = "प्रमाणपत्र नहीं मिला या अमान्य पासवर्ड।"
		SigningFailed                             = "हस्ताक्षर विफल: {0}"
		ReadFileFailed                            = "फ़ाइल को पढ़ने में विफल: {0}"
		PreprocessUnknownIfCondition              = "अज्ञात स्थिति: {0}`nमान लिया गया फाल्स।"
		PreprocessNestedIfDeadCode                = "#_if {1} के अंदर नेस्टेड #_if {0}: बाहरी शर्त पहले से इस शाखा को तय करती है, इसलिए एक हिस्सा डेड कोड है।"
		PreprocessMissingEndIf                    = "endif की कमी: {0}"
		PreprocessPsexeBranchCode                 = "#_if PSEXE शाखा में कोड न #_!! है न टिप्पणी, इसलिए स्क्रिप्ट को सीधे चलाने पर भी यह निष्पादित होता है।"
		PreprocessPsscriptBranchBang              = "#_if PSScript शाखा में #_!! स्क्रिप्ट को सीधे चलाने पर टिप्पणी बना रहता है; यहाँ सामान्य कोड लिखें।"
		ConfigFileCreated                         = "EXE के लिए कॉन्फ़िगरेशन फ़ाइल बनाई गई"
		SourceFileCopied                          = "डिबग के लिए स्रोत फ़ाइल नाम कॉपी किया गया: {0}"
		CoreCompilePublishing                     = "Publishing single-file executable with the .NET SDK..."
		CoreCompileNeedDotnet                     = "PowerShell Core compilation requires the .NET SDK (dotnet). Install it, or pass -Build @{Target='Framework4.0'}."
		CoreCompileUnsupported                    = "These options are not supported by the PowerShell Core compiler yet: {0}"
		CoreCompileNeedPwsh                       = "This is Windows PowerShell; -Build @{Target='Core'} needs PowerShell Core (pwsh) installed and on PATH."
		CoreCompileNeedWindowsPowerShell          = "Windows PowerShell was not found; pass -Build @{Target='Core'} to compile a PowerShell Core executable."
		CoreCompileNeedPwshHost                   = "The compiled ps12exe executable cannot build PowerShell Core executables; run ps12exe from the script/module under pwsh instead."
		CoreCompileHint                           = "If this is a PowerShell Core-only script, pass -Build @{Target='Core'} (requires PowerShell Core and .NET on the build and target machines; the resulting exe is much larger)."
		ReadingFile                               = "फ़ाइल {0} आकार {1} बाइट्स पढ़ रहा है"
		ForceX86byVirtualization                  = "अनुप्रयोग वर्चुअलाइजेशन सक्रिय है, x86 प्लेटफ़ॉर्म को मजबूर कर रहा है।"
		TryingTinySharpCompile                    = "स्थिरांक परिणाम, TinySharp संकलक का प्रयास कर रहा है..."
		TinySharpFailedFallback                   = "TinySharp संकलक त्रुटि, सामान्य प्रोग्राम फ्रेम पर वापस गिरें"
		OutputPath                                = "पथ: {0}"
		ReadingScriptDone                         = "फ़ाइल {0} पढ़ना पूर्ण, प्रीप्रोसेसिंग शुरू करना..."
		PreprocessScriptDone                      = "पूर्व-प्रक्रिया फ़ाइल {0} पूर्ण हुई"
		ConstEvalStart                            = "स्थिरांकों का मूल्यांकन..."
		ConstEvalDone                             = "स्थिरांकों का मूल्यांकन पूर्ण -> {0} बाइट्स"
		ConstEvalTooLongFallback                  = "स्थिरांक परिणाम बहुत लंबा है, सामान्य प्रोग्राम फ्रेम पर वापस आ रहा है"
		ConstEvalTimeoutFallback                  = "स्थिरांक मूल्यांकन {0} सेकंड के बाद समय समाप्त हो गया, सामान्य प्रोग्राम फ्रेम पर वापस आ रहा है"
		ConstEvalThrowErrorFallback               = "स्थिरांक परिणाम में एक त्रुटि आई, सामान्य प्रोग्राम फ्रेम पर वापस आ रहा है"
		ConstEvalNotConstFallback                 = "स्क्रिप्ट ने स्वयं को स्थिरांक नहीं घोषित किया, सामान्य प्रोग्राम फ्रेम पर वापस जा रहे हैं"
		InvalidArchitecture                       = "अमान्य प्लेटफ़ॉर्म {0}, AnyCpu का उपयोग करके"
		UnknownPragma                             = "अज्ञात pragma: {0}"
		UnknownPragmaBadParameterType             = "अज्ञात pragma: {0}, प्रकार {1} का विश्लेषण नहीं किया जा सकता है।"
		UnknownPragmaBoolValue                    = "अज्ञात pragma मान: {0}, इसे बूलियन के रूप में नहीं ले सकता।"
		PragmaUnsafeExpression                    = "pragma {0} में असुरक्षित अभिव्यक्ति: {1}"
		DllExportDelNoneTypeArg                   = "{0}: {1} एक गैर-प्रकार का पैरामीटर है, मान लें कि यह एक स्ट्रिंग है।"
		DllExportUsing                            = "आप #_DllExport का उपयोग कर रहे हैं, यह मैक्रो अभी भी विकास के अधीन है और अभी तक समर्थित नहीं है।"
	}
	WebServerI18nData            = @{
		CompilingUserInput  = "उपयोगकर्ता इनपुट संकलित कर रहा है: {0}"
		EmptyResponse       = "अनुरोध को संभालते समय कोई डेटा नहीं मिला, खाली प्रतिक्रिया लौटा रहा है"
		InputTooLarge413    = "उपयोगकर्ता इनपुट बहुत बड़ा है, 413 त्रुटि लौटा रहा है"
		ReqLimitExceeded429 = "IP {0} ने प्रति मिनट {1} अनुरोधों की सीमा पार कर ली है, 429 त्रुटि लौटा रहा है"
	}
	InteractI18nData             = @{
		ModeName                    = "इंटरैक्टिव मोड"
		Welcome                     = "ps12exe इंटरैक्टिव मोड में आपका स्वागत है। किसी भी समय बाहर निकलने के लिए Ctrl+C दबाएं।"
		EnterInputFile              = "कृपया इनपुट फ़ाइल पथ या URL दर्ज करें:"
		Prompt                      = " >> "
		ExitMessage                 = "इंटरैक्टिव मोड से बाहर निकल चुके हैं।"
		InvalidInputFile            = "कृपया एक वैध PS1 फ़ाइल पथ दर्ज करें।"
		FileDoesNotExist            = "फ़ाइल मौजूद नहीं है।"
		InvalidExtension            = "फ़ाइल में '.ps1', '.psd1' या '.tmp' एक्सटेंशन होना चाहिए।"
		EnterOutputFile             = "कृपया आउटपुट फ़ाइल पथ दर्ज करें (खाली छोड़ने पर समान फ़ोल्डर में <ps1>.exe):"
		OutputFileExtensionError    = "आउटपुट फ़ाइल में '.exe' एक्सटेंशन होना चाहिए। '.exe' जोड़ दिया जाएगा。"
		AddAdditionalInfo           = "अतिरिक्त जानकारी (आइकन, संस्करण, आदि) जोड़ें?"
		AdditionalInfoPrompt        = "[Y/N]"
		CollectingInfo              = "अतिरिक्त जानकारी एक��्र कर रहा है। यदि आवश्यक न हो तो खाली छोड़ दें।"
		IconPath                    = "आइकन फ़ाइल पथ या URL (.ico, .png, .jpg, .jpeg, .bmp आदि समर्थित, छोड़ने के लिए खाली छोड़ें):"
		InvalidIconExtension        = "फ़ाइल में '.ico' एक्सटेंशन होना चाहिए। अनदेखा किया गया।"
		IconDoesNotExist            = "आइकन फ़ाइल मौजूद नहीं है, कृपया पुनः दर्ज करें।"
		EnterTitle                  = "शीर्षक"
		EnterDescription            = "विवरण"
		EnterCompany                = "कंपनी का नाम"
		EnterProduct                = "उत्पाद का नाम"
		EnterCopyright              = "कॉपीराइट"
		EnterTrademark              = "ट्रेडमार्क"
		EnterResourcePrompt         = "कृपया {0} दर्ज करें"
		Version                     = "संस्करण (उदाहरण के लिए, 1.0.0.0):"
		InvalidVersionFormat        = "अमान्य संस्करण प्रारूप। अनदेखा किया गया।"
		SkippingAdditionalInfo      = "अतिरिक्त जानकारी छोड़ दी गई।"
		CompileAsGui                = "GUI एप्लिकेशन (बिना कंसोल के) के रूप में कंपाइल करें?"
		RequireAdmin                = "व्यवस्थापक विशेषाधिकारों की आवश्यकता है?"
		EnableCodeSigning           = "कोड हस्ताक्षर सक्षम करें?"
		EnterCertificatePath        = "प्रमाणपत्र पथ या URL (.pfx, छोड़ने के लिए खाली छोड़ें):"
		InvalidCertificateExtension = "प्रमाणपत्र फ़ाइल .pfx प्रारूप में होनी चाहिए, कृपया पुनः दर्ज करें।"
		CertificateDoesNotExist     = "प्रमाणपत्र फ़ाइल मौजूद नहीं है, कृपया पुनः दर्ज करें।"
		EnterCertificatePassword    = "प्रमाणपत्र पासवर्ड (छोड़ने के लिए खाली छोड़ें):"
		EnterCertificateThumbprint  = "प्रमाणपत्र अंगूठे का निशान (छोड़ने के लिए खाली छोड़ें):"
		EnterTimestampServer        = "टाइमस्टैम्प सर्वर (डिफ़ॉल्ट के लिए खाली छोड़ें):"
		SkippingCodeSigning         = "कोड हस्ताक्षर छोड़ा जा रहा है।"
		BuildingCommand             = "कमांड तैयार किया जा रहा है..."
		ExecutingCommand            = "कमांड निष्पादित किया जा रहा है..."
		CompileSuccess              = "फ़ाइल सफलतापूर्वक संकलित हुई"
		CompileFailed               = "संकलन विफल रहा, एग्जिट कोड: {0}"
		CompileFailedException      = "संकलन विफल: {0}"
		CompileAnother              = "एक और फ़ाइल संकलित करें?"
		Exiting                     = "इंटरैक्टिव मोड से बाहर निकल रहा है।"
	}
	exe21spInteractI18nData      = @{
		ModeName                 = "इंटरैक्टिव मोड"
		Welcome                  = "exe21sp इंटरैक्टिव मोड में आपका स्वागत है। किसी भी समय बाहर निकलने के लिए Ctrl+C दबाएं।"
		EnterInputFile           = "इनपुट exe का पथ या URL दर्ज करें:"
		Prompt                   = " >> "
		ExitMessage              = "इंटरैक्टिव मोड से बाहर निकल चुके हैं।"
		InvalidInputFile         = "कृपया एक वैध exe पथ या URL दर्ज करें।"
		FileDoesNotExist         = "फ़ाइल मौजूद नहीं है।"
		EnterOutputFile          = "कृपया आउटपुट फ़ाइल पथ दर्ज करें (खाली छोड़ने पर समान फ़ोल्डर में <exe>.ps1):"
		OutputFileExtensionError	= "आउटपुट फ़ाइल में '.ps1' एक्सटेंशन होना चाहिए। जोड़ रहा हूं।"
		AdditionalInfoPrompt     = "[Y/N]"
		ConvertAnother           = "किसी अन्य exe को परिवर्तित करें?"
		Exiting                  = "इंटरैक्टिव मोड से बाहर निकल रहा है।"
	}
	exe21spI18nData              = @{
		NoneInput                    = "कोई इनपुट फ़ाइल निर्दिष्ट नहीं है!"
		TinySharpNoTextSection       = "एक्ज़ीक्यूटेबल एक .NET असेंबली है लेकिन TinySharp लेआउट से मेल नहीं खाता (.text सेक्शन नहीं)।"
		TinySharpTextSectionEmpty    = "एक्ज़ीक्यूटेबल एक .NET असेंबली है लेकिन TinySharp लेआउट से मेल नहीं खाता (.text सेक्शन खाली)।"
		TinySharpCannotReadText      = "एक्ज़ीक्यूटेबल एक .NET असेंबली है लेकिन TinySharp लेआउट से मेल नहीं खाता (.text पढ़ नहीं सकते)।"
		TinySharpPayloadNotRecovered	= "एक्ज़ीक्यूटेबल एक .NET असेंबली है लेकिन TinySharp लेआउट से मेल नहीं खाता; स्क्रिप्ट पेलोड पुनर्प्राप्त नहीं हो सका।"
		NoEmbeddedScript             = "'{0}' में कोई एम्बेडेड स्क्रिप्ट नहीं मिली (ps12exe-बिल्ट exe नहीं, या पेलोड पुनर्प्राप्त नहीं हो सकता)।"
		CoreExtractNeedsPwsh         = "इस exe का पेलोड Brotli-संपीड़ित है (PowerShell Core बिल्ड)। exe21sp को इसे डीकंप्रेस करने के लिए PowerShell 7 (pwsh) स्थापित करें।"
		FileNotFound                 = "फ़ाइल नहीं मिली: {0}"
		InputUrlFailed               = "URL से पढ़ने में विफल: {0}"
	}
}
