@{
	LangName                     = "English (United Kingdom)"
	LangID                       = "en-UK"
	# Right click Menu
	CompileTitle                 = "Compile to EXE"
	OpenInGUI                    = "Open in ps12exeGUI"
	GUICfgFileDesc               = "ps12exeGUI configuration file"
	VSCodeExtensionInstalling    = "Installing the ps12exe extension for {0} ..."
	VSCodeExtensionInstallFailed = "I'm afraid installing the ps12exe extension for {0} did not succeed (it may not be published yet): {1}"
	VSCodeExtensionUninstalling  = "Uninstalling the ps12exe extension for {0} ..."
	VSCodeExtensionUninstallFailed = "I'm afraid uninstalling the ps12exe extension for {0} did not succeed: {1}"
	# Web Server
	ErrorHead                    = "An error occurred:"
	CompileResult                = "Compilation result:"
	DefaultResult                = "Jolly good, we're finished!"
	AskSaveCfg                   = "Might you wish to save the configuration file?"
	AskSaveCfgTitle              = "Save configuration file"
	CfgFileLabelHead             = "Configuration file:"
	# Console
	ServerStarted                = "The HTTP server is up and running!"
	ServerStopped                = "The HTTP server has been stopped."
	ServerStartFailed            = "Rather unfortunate—failed to start the HTTP server!"
	TryRunAsRoot                 = "Do try running as root."
	ServerListening              = "Access address:"
	ExitServerTip                = "You may press Ctrl+C to stop the server at any time."
	# GUI
	ConsoleHelpData              = @{
		title      = "Usage:"
		Usage      = "[input |] ps12exe [[-inputFile] '<filename|url>' | -Content '<script>'] [-outputFile '<filename>']
	[-App @{Windowed=`$true; Silence=@('Output','Error'); OutputEncoding='UTF8'|'UTF16LE'|'Default';
	VisualStyles=`$true; ExitOnCancel=`$true; CredentialGUI=`$true; DpiAware=`$true; WinFormsDpiAware=`$true}]
	[-Os @{Admin=`$true; ModernOS=`$true; LongPaths=`$true; Virtualize=`$true}]
	[-Build @{Target='Framework4.0'|'Framework2.0'|'Core'; Platform='AnyCpu'|'x64'|'x86'; Apartment='STA'|'MTA';
	Culture='<culture>'; Options='<options>'; KeepSource=`$true; Minify={<scriptblock>}; TempDir='<directory>'}]
	[-Resources @{Icon='<filename|url>'; Title='<title>'; Description='<description>'; Company='<company>';
	Product='<product>'; Copyright='<copyright>'; Trademark='<trademark>'; Version='<version>'}]
	[-Signing @{Certificate='<PFX file path>'; Password='<PFX password>'; Thumbprint='<certificate thumbprint>'; Timestamp='<timestamp server>'}]
	[-PreprocessOnly] [-Golf] [-Sandbox] [-NoUpdateCheck] [-Locale '<language code>'] [-ConfigFile] [-help]"
		PrarmsData = [ordered]@{
			input            = "String of the contents of the PowerShell script file (same as ``-Content``)."
			inputFile        = "PowerShell script file path or URL that you want to convert to executable (file has to be UTF8 or UTF16 encoded)."
			Content          = "PowerShell script content that you want to convert to executable."
			outputFile       = "Destination executable file name or folder (defaults to ``inputFile`` with the extension ``'.exe'``)."
			App              = [ordered]@{
				Windowed         = "Build a Windows Forms application without a console window."
				Silence          = "Stream names to suppress; one or more of ``'Output'``, ``'Verbose'``, ``'Error'``, ``'Warning'``, ``'Debug'``, or ``'*'``."
				OutputEncoding   = "Console output encoding; ``'Default'``, ``'UTF8'`` or ``'UTF16LE'``."
				VisualStyles     = "Enable visual styles for GUI applications (default `` `$true ``)."
				ExitOnCancel     = "Exit when Cancel or ``'X'`` is selected in a ``Read-Host`` input box."
				CredentialGUI    = "Use a GUI for prompting credentials in console mode."
				DpiAware         = "Mark the compiled executable as DPI aware."
				WinFormsDpiAware = "Let WinForms use DPI scaling (requires Windows 10 and .NET 4.7 or up)."
			}
			Os               = [ordered]@{
				Admin      = "If UAC is enabled, the compiled executable will run only in an elevated context (UAC dialog appears if required)."
				ModernOS   = "Use functions of the newest Windows versions (execute ``[Environment]::OSVersion`` to see the difference)."
				LongPaths  = "Enable long paths (``> 260`` characters) if enabled on the OS (works only with Windows 10 or up)."
				Virtualize = "Application virtualization is activated (forcing x86 runtime)."
			}
			Build            = [ordered]@{
				Target     = "Target runtime version (``'Framework4.0'`` by default; ``'Framework2.0'`` and ``'Core'`` are supported). ``'Core'`` builds a PowerShell Core (.NET) executable (needs PowerShell Core and .NET on both build and target machines; the output is much larger)."
				Platform   = "Compile for specific runtime only (possible values are ``'AnyCpu'``, ``'x64'``, and ``'x86'``)."
				Apartment  = "``'Single Thread Apartment'`` or ``'Multi Thread Apartment'`` mode."
				Culture    = "Locale for the compiled executable (current user culture if not specified)."
				Options    = "Additional compiler options (see ``https://msdn.microsoft.com/en-us/library/78f4aasd.aspx``)."
				KeepSource = "Create helpful information for debugging."
				Minify     = "Scriptblock to minify the script before compiling."
				TempDir    = "Directory for storing temporary files (default is a randomly generated temp directory in ``%temp%``)."
			}
			Resources        = [ordered]@{
				Icon        = "Icon of the executable; can be a file path or URL."
				Title       = "Title (file description) of the executable."
				Description = "Short description of the executable."
				Company     = "Company name of the executable."
				Product     = "Product name of the executable."
				Copyright   = "Copyright notice of the executable."
				Trademark   = "Trademark information of the executable."
				Version     = "Version number of the executable (for example ``'1.0.0.0'``)."
			}
			Signing          = [ordered]@{
				Certificate = "Path to the PFX certificate file; either ``Certificate`` or ``Thumbprint`` must be specified."
				Password    = "Password of the PFX certificate."
				Thumbprint  = "Certificate thumbprint; either ``Certificate`` or ``Thumbprint`` must be specified."
				Timestamp   = "URL of the timestamp server used for code signing."
			}
			PreprocessOnly   = "Preprocess the input script and return it without compiling."
			Golf             = "Enable golf mode, adding abbreviations and common functions."
			Sandbox          = "Compile scripts with additional protection, preventing native files from being accessed."
			NoUpdateCheck    = "Skip the check for new versions of ps12exe"
			Locale           = "The language code to use."
			ConfigFile       = "Write a config file (``<outputfile>.exe.config``)."
			Help             = "Show this help message."
		}
	}
	GUIHelpData                  = @{
		title      = "Usage:"
		Usage      = @"
ps12exeGUI [[-ConfigFile] '<config file>'] [-PS1File '<PS1 file>'] [-Locale '<language code>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]

ps12exeGUI [[-PS1File] '<PS1 file>'] [-Locale '<language code>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]
"@
		PrarmsData = [ordered]@{
			ConfigFile	= "The configuration file to load."
			PS1File    = "The script file to be compiled."
			Locale     = "The language code to use."
			UIMode     = "The UI mode to use."
			help       = "Show this help message."
		}
	}
	SetContextMenuHelpData       = @{
		title      = "Usage:"
		Usage      = "Set-ps12exeContextMenu [[-action] 'enable'|'disable'|'reset'] [-Locale '<language code>'] [-SkipEditorExtension] [-help]"
		PrarmsData = [ordered]@{
			action              = "The action to execute."
			Locale              = "The language code to use."
			SkipEditorExtension	= "Skip installing or uninstalling the ps12exe VS Code extension in detected editors."
			help                = "Show this help message."
		}
	}
	WebServerHelpData            = @{
		title      = "Usage:"
		Usage      = "Start-ps12exeWebServer [[-HostUrl] '<url>'] [-MaxCompileThreads '<uint>'] [-MaxCompileTime '<uint>']
	[-ReqLimitPerMin '<uint>'] [-MaxCachedFileSize '<uint>'] [-MaxScriptFileSize '<uint>'] [-CacheDir '<path>']
	[-Locale '<language code>'] [-help]"
		PrarmsData = [ordered]@{
			HostUrl           = "The HTTP server address to register."
			MaxCompileThreads = "The maximum number of compile threads."
			MaxCompileTime    = "The maximum compile time in seconds."
			ReqLimitPerMin    = "The maximum number of requests per minute per IP."
			MaxCachedFileSize = "The maximum size of the cached file."
			MaxScriptFileSize = "The maximum size of the script file."
			CacheDir          = "The directory to store the cached files."
			Locale            = "The language code to be used for server-side logging."
			help              = "Display this help information."
		}
	}
	exe21spHelpData              = @{
		title      = "Usage:"
		Usage      = "[input |] exe21sp [[-inputFile] '<path or url to exe>'] [-outputFile '<path to output .ps1>'] [-help]"
		PrarmsData = [ordered]@{
			input      = "Path or URL to the ps12exe-generated exe to decompile, same as ``-inputFile``."
			inputFile  = "Path or URL to the ps12exe-generated exe to decompile."
			outputFile = "Optional; path to write the recovered script. If omitted, output goes to stdout when redirected, otherwise writes to ``<exe>.ps1`` in the same folder."
			help       = "Display this help message."
		}
	}
	CompilingI18nData            = @{
		NewVersionAvailable                       = "There's a new version of ps12exe available: {0}!"
		NoneInput                                 = "No input file specified!"
		BothInputAndContentSpecified              = "Input file and content can't be used at the same time."
		PreprocessDone                            = "Done pre-processing the input script."
		PreprocessedScriptSize                    = "Preprocessed script -> {0} bytes."
		MinifyingScript                           = "Minifying the script..."
		MinifyedScriptSize                        = "Minified script -> {0} bytes."
		MinifyerError                             = "Minifyer error: {0}"
		MinifyerFailedUsingOriginalScript         = "Minifyer failed, using the original script."
		TempFileMissing                           = "Temporary file {0} not found."
		PreprocessOnlyDone                        = "Done pre-processing the input script."
		InvalidResourceParam                      = "Parameter -Resources has an invalid key: {0}"
		InputSyntaxError                          = "Syntax error in the script."
		SyntaxErrorLineStart                      = "At line {0}, Col {1}:"
		IdenticalInputOutput                      = "Input file is identical to the output file."
		CombinedArg_Virtualize_requireAdmin       = "-Os @{Virtualize=`$true} can't be combined with -Os @{Admin=`$true}."
		CombinedArg_Virtualize_supportOS          = "-Os @{Virtualize=`$true} can't be combined with -Os @{ModernOS=`$true}."
		CombinedArg_Virtualize_longPaths          = "-Os @{Virtualize=`$true} can't be combined with -Os @{LongPaths=`$true}."
		CombinedArg_NoConfigFile_LongPaths        = "Forcing the generation of a config file, since the option -Os @{LongPaths=`$true} requires this."
		CombinedArg_NoConfigFile_winFormsDPIAware = "Forcing the generation of a config file, since the option -App @{WinFormsDpiAware=`$true} requires this."
		SomeCmdletsMayNotAvailable                = "Cmdlets {0} are used but may not be available at runtime. Make sure you've checked."
		SomeNotFoundCmdlets                       = "Unknown functions {0} are used."
		SomeTypesMayNotAvailable                  = "Types {0} are used but may not be available at runtime. Make sure you've checked."
		CompilingFile                             = "Compiling file..."
		CompilationFailed                         = "Compilation failed!"
		OutputFileNotWritten                      = "Output file {0} not written."
		CompiledFileSize                          = "Compiled file written -> {0} bytes."
		OopsSomethingWentWrong                    = "Oh dear, something has gone rather wrong."
		TryUpgrade                                = "Latest version is {0}; do try upgrading."
		EnterToSubmitIssue                        = "For help, please submit an issue by pressing Enter."
		GuestModeFileTooLarge                     = "The file {0} is too large to read."
		GuestModeIconFileTooLarge                 = "The icon {0} is too large to read."
		GuestModeFtpNotSupported                  = "FTP is not supported in Sandbox mode."
		IconFileNotFound                          = "Icon file not found: {0}"
		ConvertingImageToIcon                     = "Converting image to icon format..."
		ImageConvertedToIcon                      = "Image converted to icon: {0}"
		ImageConversionFailed                     = "Image conversion failed: {0}"
		PleaseUseIcoFile                          = "Please use a .ico file instead of {0}"
		SigningExecutable                         = "Signing executable..."
		ExecutableSignedSuccessfully              = "Executable signed successfully."
		SigningStatusNotValid                     = "Signing status not valid: {0} - {1}"
		CertificateNotFoundOrInvalidPassword      = "Certificate not found or invalid password."
		SigningFailed                             = "Signing failed: {0}"
		ReadFileFailed                            = "Failed to read the file: {0}"
		PreprocessUnknownIfCondition              = "Unknown condition: {0}`nassuming false."
		PreprocessNestedIfDeadCode                = "Nested #_if {0} inside #_if {1}: the enclosing condition already fixes this branch, so one side is dead code."
		PreprocessMissingEndIf                    = "Missing end of if statement: {0}"
		PreprocessPsexeBranchCode                 = "Code in a #_if PSEXE branch is neither #_!! nor a comment, so it also runs when the script is executed directly."
		PreprocessPsscriptBranchBang              = "#_!! in a #_if PSScript branch is a comment when the script is executed directly; use plain code here."
		ConfigFileCreated                         = "Config file for the EXE created."
		SourceFileCopied                          = "Source file name for debugging copied: {0}"
		CoreCompilePublishing                     = "Publishing single-file executable with the .NET SDK..."
		CoreCompileNeedDotnet                     = "PowerShell Core compilation requires the .NET SDK (dotnet). Install it, or pass -Build @{Target='Framework4.0'}."
		CoreCompileUnsupported                    = "These options are not supported by the PowerShell Core compiler yet: {0}"
		CoreCompileNeedPwsh                       = "This is Windows PowerShell; -Build @{Target='Core'} needs PowerShell Core (pwsh) installed and on PATH."
		CoreCompileNeedWindowsPowerShell          = "Windows PowerShell was not found; pass -Build @{Target='Core'} to compile a PowerShell Core executable."
		CoreCompileNeedPwshHost                   = "The compiled ps12exe executable cannot build PowerShell Core executables; run ps12exe from the script/module under pwsh instead."
		CoreCompileHint                           = "If this is a PowerShell Core-only script, pass -Build @{Target='Core'} (requires PowerShell Core and .NET on the build and target machines; the resulting exe is much larger)."
		ReadingFile                               = "Reading file {0}, size {1} bytes."
		ForceX86byVirtualization                  = "Application virtualization is activated, forcing x86 platform."
		TryingTinySharpCompile                    = "Const result, trying TinySharp Compiler..."
		TinySharpFailedFallback                   = "TinySharp Compiler error, falling back to the normal program frame."
		OutputPath                                = "Path: {0}"
		ReadingScriptDone                         = "Done reading file {0}, starting preprocessing..."
		PreprocessScriptDone                      = "Done preprocessing file {0}."
		ConstEvalStart                            = "Evaluation of constants..."
		ConstEvalDone                             = "Done evaluating constants -> {0} bytes."
		ConstEvalTooLongFallback                  = "Constant result is too long, falling back to the normal program frame."
		ConstEvalTimeoutFallback                  = "Evaluation timed out after {0} seconds, falling back to the normal program frame."
		ConstEvalThrowErrorFallback               = "Constant result throws an error, falling back to the normal program frame."
		ConstEvalNotConstFallback                 = "Script declared itself non-const, falling back to the normal program frame."
		InvalidArchitecture                       = "Invalid platform {0}, using AnyCpu."
		UnknownPragma                             = "Unknown pragma: {0}"
		UnknownPragmaBadParameterType             = "Unknown pragma: {0}, as type {1} can't analyse."
		UnknownPragmaBoolValue                    = "Unknown pragma value: {0}, can't take it as a boolean."
		PragmaUnsafeExpression                    = "Unsafe expression in pragma {0}: {1}"
		DllExportDelNoneTypeArg                   = "{0}: {1} is a none type parameter, assuming it's a string."
		DllExportUsing                            = "You are using #_DllExport, this macro is in dev and not yet supported."
	}
	WebServerI18nData            = @{
		CompilingUserInput  = "Compiling User Input: {0}"
		EmptyResponse       = "No data found when handling the request; returning an empty response."
		InputTooLarge413    = "User input is too large, returning a 413 error."
		ReqLimitExceeded429 = "IP {0} has exceeded the limit of {1} requests per minute, returning a 429 error."
	}
	InteractI18nData             = @{
		ModeName                    = "Interactive"
		Welcome                     = "Welcome to the ps12exe interactive mode. Do press Ctrl+C to exit at any time."
		EnterInputFile              = "Kindly provide the path or URL to the input file, if you would:"
		Prompt                      = " >> "
		ExitMessage                 = "Exited interactive mode."
		InvalidInputFile            = "I do beg your pardon, but that does not appear to be a valid PS1 file path."
		FileDoesNotExist            = "Regrettably, the specified file could not be located."
		InvalidExtension            = "File must have '.ps1', '.psd1', or '.tmp' extension."
		EnterOutputFile             = "Please enter the output file path (leave blank for <ps1>.exe in the same folder):"
		OutputFileExtensionError    = "It appears the output file lacks the requisite '.exe' extension. I shall append it for you."
		AddAdditionalInfo           = "Would you care to embellish the executable with additional details?"
		AdditionalInfoPrompt        = "[Y/N]"
		CollectingInfo              = "Gathering particulars. Should you wish to omit an item, simply leave the field vacant."
		IconPath                    = "Icon file path or URL (supports .ico, .png, .jpg, .jpeg, .bmp, etc., leave blank to skip):"
		InvalidIconExtension        = "An icon must be a '.ico' file, I'm afraid. This selection shall be disregarded."
		IconDoesNotExist            = "Icon file does not exist, please re-enter."
		EnterTitle                  = "Title"
		EnterDescription            = "Description"
		EnterCompany                = "Company Name"
		EnterProduct                = "Product Name"
		EnterCopyright              = "Copyright"
		EnterTrademark              = "Trademark"
		EnterResourcePrompt         = "Please enter {0}"
		Version                     = "Version (e.g., 1.0.0.0):"
		InvalidVersionFormat        = "That version format appears to be invalid; it shall be disregarded."
		SkippingAdditionalInfo      = "Very well, we shall proceed without the extra fineries."
		CompileAsGui                = "Compile as GUI application (no console)?"
		RequireAdmin                = "Require administrator privileges?"
		EnableCodeSigning           = "Would you care to enable code signing?"
		EnterCertificatePath        = "Certificate path or URL (.pfx, leave blank to skip):"
		InvalidCertificateExtension = "Certificate file must be .pfx format, please re-enter."
		CertificateDoesNotExist     = "Certificate file does not exist, please re-enter."
		EnterCertificatePassword    = "Certificate password (leave blank to skip):"
		EnterCertificateThumbprint  = "Certificate thumbprint (leave blank to skip):"
		EnterTimestampServer        = "Timestamp server (leave blank for default):"
		SkippingCodeSigning         = "Very well, we shall proceed without code signing."
		BuildingCommand             = "Preparing the compilation instructions..."
		ExecutingCommand            = "Executing command..."
		CompileSuccess              = "Splendid! The file has been compiled successfully."
		CompileFailed               = "Oh, dear. The compilation has failed with exit code {0}."
		CompileFailedException      = "Compilation failed: {0}"
		CompileAnother              = "Shall we proceed with another compilation?"
		Exiting                     = "Exiting interactive mode."
	}
	exe21spInteractI18nData      = @{
		ModeName                 = "Interactive"
		Welcome                  = "Welcome to the exe21sp interactive mode. Do press Ctrl+C to exit at any time."
		EnterInputFile           = "Kindly provide the input exe path or URL, if you would:"
		Prompt                   = " >> "
		ExitMessage              = "Exited interactive mode."
		InvalidInputFile         = "I'm afraid that doesn't appear to be a valid exe path or URL. Do try again."
		FileDoesNotExist         = "Regrettably, that file could not be located."
		EnterOutputFile          = "Kindly enter the output file path (leave blank for <exe>.ps1 in the same folder):"
		OutputFileExtensionError	= "It appears the output file lacks the requisite '.ps1' extension. I shall append it for you."
		AdditionalInfoPrompt     = "[Y/N]"
		ConvertAnother           = "Shall we convert another exe?"
		Exiting                  = "Exiting interactive mode."
	}
	exe21spI18nData              = @{
		NoneInput                    = "No input file specified!"
		TinySharpNoTextSection       = "The executable is a .NET assembly but does not match the TinySharp layout (no .text section)."
		TinySharpTextSectionEmpty    = "The executable is a .NET assembly but does not match the TinySharp layout (.text section is empty)."
		TinySharpCannotReadText      = "The executable is a .NET assembly but does not match the TinySharp layout (cannot read .text)."
		TinySharpPayloadNotRecovered	= "The executable is a .NET assembly but does not match the TinySharp layout; script payload cannot be recovered."
		NoEmbeddedScript             = "No embedded script found in '{0}' (not a ps12exe-built exe, or payload cannot be recovered)."
		CoreExtractNeedsPwsh         = "This exe's payload is Brotli-compressed (a PowerShell Core build). Install PowerShell 7 (pwsh) so exe21sp can decompress it."
		FileNotFound                 = "File not found: {0}"
		InputUrlFailed               = "Regrettably, failed to read from URL: {0}"
	}
}
