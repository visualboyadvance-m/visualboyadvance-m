@{
	LangName                     = "日本語"
	LangID                       = "ja-JP"
	# 右クリックメニュー
	CompileTitle                 = "EXE にコンパイル"
	OpenInGUI                    = "ps12exeGUI で開く"
	GUICfgFileDesc               = "ps12exe GUI 設定ファイル"
	VSCodeExtensionInstalling    = "{0} 用の ps12exe 拡張機能をインストールしています..."
	VSCodeExtensionInstallFailed = "{0} 用の ps12exe 拡張機能をインストールできませんでした（まだ公開されていない可能性があります）: {1}"
	VSCodeExtensionUninstalling  = "{0} 用の ps12exe 拡張機能をアンインストールしています..."
	VSCodeExtensionUninstallFailed = "{0} 用の ps12exe 拡張機能をアンインストールできませんでした: {1}"
	# Web サーバー
	ErrorHead                    = "エラー："
	CompileResult                = "コンパイル結果"
	DefaultResult                = "完了！"
	AskSaveCfg                   = "設定ファイルを保存しますか？"
	AskSaveCfgTitle              = "設定ファイルの保存"
	CfgFileLabelHead             = "設定ファイル："
	# コンソール
	ServerStarted                = "HTTP サーバーが起動しました！"
	ServerStopped                = "HTTP サーバーが停止しました！"
	ServerStartFailed            = "HTTP サーバーの起動に失敗しました！"
	TryRunAsRoot                 = "管理者権限で実行してください。"
	ServerListening              = "アクセスアドレス："
	ExitServerTip                = "いつでも Ctrl+C を押してサーバーを終了できます"
	# GUI
	ConsoleHelpData              = @{
		title      = "使用方法："
		Usage      = "[input |] ps12exe [[-inputFile] '<ファイル名|url>' | -Content '<スクリプト>'] [-outputFile '<ファイル名>']
	[-App @{Windowed=`$true; Silence=@('Output','Error'); OutputEncoding='UTF8'|'UTF16LE'|'Default';
	VisualStyles=`$true; ExitOnCancel=`$true; CredentialGUI=`$true; DpiAware=`$true; WinFormsDpiAware=`$true}]
	[-Os @{Admin=`$true; ModernOS=`$true; LongPaths=`$true; Virtualize=`$true}]
	[-Build @{Target='Framework4.0'|'Framework2.0'|'Core'; Platform='AnyCpu'|'x64'|'x86'; Apartment='STA'|'MTA';
	Culture='<カルチャ>'; Options='<オプション>'; KeepSource=`$true; Minify={<scriptblock>}; TempDir='<ディレクトリ>'}]
	[-Resources @{Icon='<ファイル名|url>'; Title='<タイトル>'; Description='<説明>'; Company='<会社>';
	Product='<製品>'; Copyright='<著作権>'; Trademark='<商標>'; Version='<バージョン>'}]
	[-Signing @{Certificate='<PFXファイルパス>'; Password='<PFXパスワード>'; Thumbprint='<証明書指紋>'; Timestamp='<時刻同期サーバー>'}]
	[-PreprocessOnly] [-Golf] [-Sandbox] [-NoUpdateCheck] [-Locale '<言語コード>'] [-ConfigFile] [-help]"
		PrarmsData = [ordered]@{
			input            = "PowerShell スクリプトファイルの内容の文字列で、``-Content`` と同じです"
			inputFile        = "変換元の PowerShell スクリプトのパスまたは URL（ファイルは UTF-8 または UTF-16 エンコードである必要があります）。"
			Content          = "実行可能ファイルに変換したい PowerShell スクリプトの内容"
			outputFile       = "ターゲットの実行可能ファイル名またはディレクトリ。デフォルトは ``'.exe'`` 拡張子を持つ ``inputFile`` です"
			App              = [ordered]@{
				Windowed         = "生成された実行可能ファイルは、コンソールウィンドウのない Windows Forms アプリケーションになります。"
				Silence          = "抑制する出力ストリームの名前。``'Output'``、``'Verbose'``、``'Error'``、``'Warning'``、``'Debug'`` のいずれか 1 つ以上、またはすべてを表す ``'*'``。"
				OutputEncoding   = "コンソール出力のエンコーディング。``'Default'``、``'UTF8'``、``'UTF16LE'``。"
				VisualStyles     = "GUI アプリケーションのビジュアルスタイルを有効にします（既定値 `` `$true ``）。"
				ExitOnCancel     = "``Read-Host`` 入力ボックスで Cancel または ``'X'`` を選択したときにプログラムを終了します。"
				CredentialGUI    = "コンソールモードで GUI プロンプトを使用して資格情報を求めます。"
				DpiAware         = "コンパイルされた実行可能ファイルを DPI 対応としてマークします。"
				WinFormsDpiAware = "WinForms で DPI スケーリングを使用します（Windows 10 および .Net 4.7 以上が必要）。"
			}
			Os               = [ordered]@{
				Admin      = "UAC が有効になっている場合、コンパイルされた実行可能ファイルは昇格されたコンテキストでのみ実行可能です（必要に応じて UAC ダイアログが表示されます）。"
				ModernOS   = "最新の Windows バージョンの機能を使用します（``[Environment]::OSVersion`` を実行して違いを確認）。"
				LongPaths  = "OS で有効になっている場合、長いパス（260 文字超）を有効にします（Windows 10 以上にのみ適用）。"
				Virtualize = "アプリケーションの仮想化が有効になっています（x86 ランタイムを強制）。"
			}
			Build            = [ordered]@{
				Target     = "ターゲット ランタイム バージョン、既定値は ``'Framework4.0'``、``'Framework2.0'`` と ``'Core'`` がサポートされています。``'Core'`` は PowerShell Core (.NET) 実行可能ファイルを生成します（コンパイル機とターゲット機の両方に PowerShell Core と .NET が必要で、成果物は大幅に大きくなります）。"
				Platform   = "特定のランタイムのみのコンパイル。可能な値は ``'AnyCpu'``、``'x64'``、``'x86'`` です。"
				Apartment  = "``'STA'``（シングルスレッドアパートメント）または ``'MTA'``（マルチスレッドアパートメント）モード。"
				Culture    = "コンパイルされた実行可能ファイルのカルチャ。指定されていない場合は、現在のユーザーのカルチャです。"
				Options    = "追加のコンパイラオプション（参照： ``https://msdn.microsoft.com/en-us/library/78f4aasd.aspx``）。"
				KeepSource = "デバッグに役立つ情報を作成します。"
				Minify     = "コンパイル前にスクリプトを縮小するスクリプトブロック。"
				TempDir    = "一時ファイルを保存するディレクトリ（デフォルトは ``%temp%`` にランダムに生成される一時ディレクトリ）。"
			}
			Resources        = [ordered]@{
				Icon        = "実行可能ファイルのアイコン。アイコンファイルのパスまたは URL にできます。"
				Title       = "実行可能ファイルのタイトル（ファイルの説明）。"
				Description = "実行可能ファイルの簡単な説明。"
				Company     = "実行可能ファイルの会社名。"
				Product     = "実行可能ファイルの製品名。"
				Copyright   = "実行可能ファイルの著作権表示。"
				Trademark   = "実行可能ファイルの商標情報。"
				Version     = "実行可能ファイルのバージョン番号（例： ``'1.0.0.0'``）。"
			}
			Signing          = [ordered]@{
				Certificate = "PFX 証明書ファイルのパス。``Certificate`` または ``Thumbprint`` のいずれかを指定する必要があります。"
				Password    = "PFX 証明書のパスワード。"
				Thumbprint  = "証明書のサムプリント。``Certificate`` または ``Thumbprint`` のいずれかを指定する必要があります。"
				Timestamp   = "コード署名に使用するタイムスタンプ サーバーの URL。"
			}
			PreprocessOnly   = "入力スクリプトをプリプロセス処理し、コンパイルせずに返します"
			Golf             = "golf モードを有効にします、略語と一般的な関数を追加します"
			Sandbox          = "ネイティブ ファイルへのアクセスを防ぐために、スクリプトをコンパイルする際に保護を追加します"
			NoUpdateCheck    = "ps12exe の新しいバージョンの確認をスキップします。"
			Locale           = "使用する言語コード"
			ConfigFile       = "設定ファイル（``<outputfile>.exe.config``）を書き込みます"
			Help             = "このヘルプ情報を表示します"
		}
	}
	GUIHelpData                  = @{
		title      = "使用方法："
		Usage      = @"
ps12exeGUI [[-ConfigFile] '<設定ファイル>'] [-PS1File '<スクリプトファイル>'] [-Locale '<言語コード>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]

ps12exeGUI [[-PS1File] '<スクリプトファイル>'] [-Locale '<言語コード>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]
"@
		PrarmsData = [ordered]@{
			ConfigFile	= "読み込む設定ファイル。"
			PS1File    = "コンパイルするスクリプトファイル。"
			Locale     = "使用する言語コード。"
			UIMode     = "使用する UI モード。"
			help       = "このヘルプ情報を表示します。"
		}
	}
	SetContextMenuHelpData       = @{
		title      = "使用方法："
		Usage      = "Set-ps12exeContextMenu [[-action] 'enable'|'disable'|'reset'] [-Locale '<言語コード>'] [-SkipEditorExtension] [-help]"
		PrarmsData = [ordered]@{
			action              = "実行するアクション。"
			Locale              = "使用する言語コード。"
			SkipEditorExtension	= "検出されたエディターへの ps12exe VS Code 拡張機能のインストールまたはアンインストールをスキップします。"
			help                = "このヘルプ情報を表示します。"
		}
	}
	WebServerHelpData            = @{
		title      = "使用方法："
		Usage      = "Start-ps12exeWebServer [[-HostUrl] '<url>'] [-MaxCompileThreads '<uint>'] [-MaxCompileTime '<uint>']
	[-ReqLimitPerMin '<uint>'] [-MaxCachedFileSize '<uint>'] [-MaxScriptFileSize '<uint>'] [-CacheDir '<パス>']
	[-Locale '<言語コード>'] [-help]"
		PrarmsData = [ordered]@{
			HostUrl           = "登録する HTTP サーバーのアドレス。"
			MaxCompileThreads = "最大コンパイル スレッド数。"
			MaxCompileTime    = "最大コンパイル時間（秒）。"
			ReqLimitPerMin    = "IP アドレスごとの 1 分間のリクエスト制限。"
			MaxCachedFileSize = "最大キャッシュファイルサイズ。"
			MaxScriptFileSize = "最大スクリプトファイルサイズ。"
			CacheDir          = "キャッシュディレクトリ。"
			Locale            = "サーバー側のログに使用する言語コード。"
			help              = "このヘルプ情報を表示します。"
		}
	}
	exe21spHelpData              = @{
		title      = "用法："
		Usage      = "[input |] exe21sp [[-inputFile] '<exeのパスまたはurl>'] [-outputFile '<出力ps1パス>'] [-help]"
		PrarmsData = [ordered]@{
			input      = "反コンパイルする ps12exe 生成 exe のパスまたは URL。``-inputFile`` と同じ。"
			inputFile  = "反コンパイルする ps12exe 生成 exe のパスまたは URL。"
			outputFile = "省略可。復元スクリプトを書き出す ps1 のパス。省略時はリダイレクト時は標準出力へ、そうでなければ同じフォルダの ``<exe>.ps1`` に書き出す。"
			help       = "このヘルプを表示。"
		}
	}
	CompilingI18nData            = @{
		NewVersionAvailable                       = "ps12exe の新しいバージョンが利用可能です: {0}！"
		NoneInput                                 = "入力ファイルが指定されていません！"
		BothInputAndContentSpecified              = "入力ファイルとコンテンツを同時に使用することはできません！"
		PreprocessDone                            = "入力スクリプトの前処理が完了しました"
		PreprocessedScriptSize                    = "前処理済みスクリプト -> {0} バイト"
		MinifyingScript                           = "スクリプトを圧縮しています..."
		MinifyedScriptSize                        = "圧縮済みスクリプト -> {0} バイト"
		MinifyerError                             = "圧縮エラー：{0}"
		MinifyerFailedUsingOriginalScript         = "圧縮に失敗しました。元のスクリプトを使用します。"
		TempFileMissing                           = "一時ファイル {0} が見つかりません！"
		PreprocessOnlyDone                        = "入力スクリプトの前処理が完了しました"
		InvalidResourceParam                      = "パラメーター -Resources に無効なキーがあります：{0}"
		InputSyntaxError                          = "スクリプトに構文エラーがあります！"
		SyntaxErrorLineStart                      = "行 {0} 列 {1}："
		IdenticalInputOutput                      = "入力ファイルと出力ファイルが同じです！"
		CombinedArg_Virtualize_requireAdmin       = "-Os @{Virtualize=`$true} は -Os @{Admin=`$true} と組み合わせることはできません"
		CombinedArg_Virtualize_supportOS          = "-Os @{Virtualize=`$true} は -Os @{ModernOS=`$true} と組み合わせることはできません"
		CombinedArg_Virtualize_longPaths          = "-Os @{Virtualize=`$true} は -Os @{LongPaths=`$true} と組み合わせることはできません"
		CombinedArg_NoConfigFile_LongPaths        = "オプション -Os @{LongPaths=`$true} はこの設定ファイルを必要とするため、設定ファイルの生成を強制します"
		CombinedArg_NoConfigFile_winFormsDPIAware = "オプション -App @{WinFormsDpiAware=`$true} はこの設定ファイルを必要とするため、設定ファイルの生成を強制します"
		SomeCmdletsMayNotAvailable                = "実行時に利用できない可能性のあるコマンドレット {0} が使用されています。確認してください！"
		SomeNotFoundCmdlets                       = "未知のコマンド {0} が使用されています"
		SomeTypesMayNotAvailable                  = "実行時に利用できない可能性のある型 {0} が使用されています。確認してください！"
		CompilingFile                             = "コンパイル中..."
		CompilationFailed                         = "コンパイルに失敗しました！"
		OutputFileNotWritten                      = "出力ファイル {0} が書き込まれませんでした"
		CompiledFileSize                          = "コンパイル済みファイル -> {0} バイト"
		OopsSomethingWentWrong                    = "エラーが発生しました。"
		TryUpgrade                                = "最新バージョンは {0} です。アップグレードを試みますか？"
		EnterToSubmitIssue                        = "ヘルプが必要な場合は、Enter キーを押して問題を報告してください。"
		GuestModeFileTooLarge                     = "ファイル {0} は大きすぎて読み取れません。"
		GuestModeIconFileTooLarge                 = "アイコン {0} は大きすぎて読み取れません。"
		GuestModeFtpNotSupported                  = "Sandbox モードでは FTP はサポートされていません。"
		IconFileNotFound                          = "アイコンファイルが見つかりません：{0}"
		ConvertingImageToIcon                     = "画像をアイコン形式に変換中..."
		ImageConvertedToIcon                      = "画像をアイコンに変換しました：{0}"
		ImageConversionFailed                     = "画像の変換に失敗しました：{0}"
		PleaseUseIcoFile                          = "{0} の代わりに .ico ファイルを使用してください"
		SigningExecutable                         = "実行可能ファイルに署名中..."
		ExecutableSignedSuccessfully              = "実行可能ファイルの署名が成功しました。"
		SigningStatusNotValid                     = "署名ステータスが無効です：{0} - {1}"
		CertificateNotFoundOrInvalidPassword      = "証明書が見つからないか、パスワードが無効です。"
		SigningFailed                             = "署名に失敗しました：{0}"
		ReadFileFailed                            = "ファイルの読み取りに失敗しました：{0}"
		PreprocessUnknownIfCondition              = "未知の条件：{0}`nfalse と仮定します。"
		PreprocessNestedIfDeadCode                = "#_if {1} 内のネストされた #_if {0}：外側の条件でこの分岐は確定するため、片側はデッドコードです。"
		PreprocessMissingEndIf                    = "endif がありません：{0}"
		PreprocessPsexeBranchCode                 = "#_if PSEXE 分岐内のコードが #_!! でもコメントでもないため、スクリプトを直接実行するときにも実行されます。"
		PreprocessPsscriptBranchBang              = "#_if PSScript 分岐内の #_!! はスクリプトを直接実行するときはコメントになります。ここには通常のコードを書いてください。"
		ConfigFileCreated                         = "EXE の設定ファイルが作成されました"
		SourceFileCopied                          = "デバッグ用のソースファイル名がコピーされました：{0}"
		CoreCompilePublishing                     = "Publishing single-file executable with the .NET SDK..."
		CoreCompileNeedDotnet                     = "PowerShell Core compilation requires the .NET SDK (dotnet). Install it, or pass -Build @{Target='Framework4.0'}."
		CoreCompileUnsupported                    = "These options are not supported by the PowerShell Core compiler yet: {0}"
		CoreCompileNeedPwsh                       = "This is Windows PowerShell; -Build @{Target='Core'} needs PowerShell Core (pwsh) installed and on PATH."
		CoreCompileNeedWindowsPowerShell          = "Windows PowerShell was not found; pass -Build @{Target='Core'} to compile a PowerShell Core executable."
		CoreCompileNeedPwshHost                   = "The compiled ps12exe executable cannot build PowerShell Core executables; run ps12exe from the script/module under pwsh instead."
		CoreCompileHint                           = "If this is a PowerShell Core-only script, pass -Build @{Target='Core'} (requires PowerShell Core and .NET on the build and target machines; the resulting exe is much larger)."
		ReadingFile                               = "ファイル {0} を読み取っています ({1} バイト)"
		ForceX86byVirtualization                  = "アプリケーション仮想化が有効化されているため、x86 プラットフォームを強制します。"
		TryingTinySharpCompile                    = "結果が定数であるため、TinySharp コンパイラを試しています..."
		TinySharpFailedFallback                   = "TinySharp コンパイラエラー。通常のプログラムフレームにフォールバックします"
		OutputPath                                = "パス：{0}"
		ReadingScriptDone                         = "{0} の読み取りが完了しました。前処理を開始します..."
		PreprocessScriptDone                      = "{0} の前処理が完了しました"
		ConstEvalStart                            = "定数の評価中..."
		ConstEvalDone                             = "定数の評価が完了しました -> {0} バイト"
		ConstEvalTooLongFallback                  = "定数結果が長すぎるため、通常のプログラムフレームにフォールバックします"
		ConstEvalTimeoutFallback                  = "定数の評価が {0} 秒後にタイムアウトしました。通常のプログラムフレームにフォールバックします"
		ConstEvalThrowErrorFallback               = "定数の評価中にエラーが発生しました。通常のプログラムフレームにフォールバックします"
		ConstEvalNotConstFallback                 = "スクリプトが非定数であると宣言されたため、通常のプログラムフレームにフォールバックします"
		InvalidArchitecture                       = "無効なプラットフォーム {0} です。AnyCpu を使用します"
		UnknownPragma                             = "未知の pragma：{0}"
		UnknownPragmaBadParameterType             = "未知の pragma：{0}。型 {1} は解析できません。"
		UnknownPragmaBoolValue                    = "未知の pragma 値：{0}。ブール値として解釈できません。"
		PragmaUnsafeExpression                    = "pragma {0} に安全でない式があります：{1}"
		DllExportDelNoneTypeArg                   = "{0}：{1} は無型パラメーターです。文字列として扱います。"
		DllExportUsing                            = "#_DllExport を使用しています。このマクロはまだ開発中であり、サポートされていません。"
	}
	WebServerI18nData            = @{
		CompilingUserInput  = "ユーザー入力をコンパイルしています：{0}"
		EmptyResponse       = "要求を処理中にデータが見つかりませんでした。空の応答を返します"
		InputTooLarge413    = "ユーザー入力が大きすぎるため、413 エラーを返します"
		ReqLimitExceeded429 = "IP {0} は、1 分あたりのリクエスト数 {1} の制限を超えたため、429 エラーを返します"
	}
	InteractI18nData             = @{
		ModeName                    = "インタラクティブモード"
		Welcome                     = "ps12exe インタラクティブモードへようこそ。いつでも Ctrl+C で終了できます。"
		EnterInputFile              = "入力ファイルのパスまたはURLを入力してください:"
		Prompt                      = " >> "
		ExitMessage                 = "インタラクティブモードを終了しました。"
		InvalidInputFile            = "有効なPS1ファイルのパスを入力してください。"
		FileDoesNotExist            = "ファイルが存在しません。"
		InvalidExtension            = "ファイルは'.ps1'、'.psd1'、または'.tmp'の拡張子である必要があります。"
		EnterOutputFile             = "出力ファイルのパスを入力してください（空白の場合は同じフォルダの <ps1>.exe）:"
		OutputFileExtensionError    = "出力ファイルは'.exe'の拡張子である必要があります。'.exe'を追加します。"
		AddAdditionalInfo           = "追加情報 (アイコン、バージョンなど) を追加しますか？"
		AdditionalInfoPrompt        = "[Y/N]"
		CollectingInfo              = "追加情報を収集しています。不要な場合は、何も入力せず Enter を押してください。"
		IconPath                    = "アイコンファイルのパスまたはURL (.ico, .png, .jpg, .jpeg, .bmp などをサポート、空白でスキップ):"
		InvalidIconExtension        = "ファイルは'.ico'の拡張子である必要があります。無視します。"
		IconDoesNotExist            = "アイコンファイルが存在しません。再入力してください。"
		EnterTitle                  = "タイトル"
		EnterDescription            = "説明"
		EnterCompany                = "会社名"
		EnterProduct                = "製品名"
		EnterCopyright              = "著作権"
		EnterTrademark              = "商標"
		EnterResourcePrompt         = "{0}を入力してください"
		Version                     = "バージョン (例: 1.0.0.0):"
		InvalidVersionFormat        = "無効なバージョン形式です。この設定は無視されます。"
		SkippingAdditionalInfo      = "追加情報はスキップしました。"
		CompileAsGui                = "GUIアプリケーションとしてコンパイルしますか (コンソールなし)？"
		RequireAdmin                = "管理者権限が必要ですか？"
		EnableCodeSigning           = "コード署名を有効にしますか？"
		EnterCertificatePath        = "証明書パスまたはURL (.pfx、空白でスキップ):"
		InvalidCertificateExtension = "証明書ファイルは .pfx 形式である必要があります。再入力してください。"
		CertificateDoesNotExist     = "証明書ファイルが存在しません。再入力してください。"
		EnterCertificatePassword    = "証明書パスワード (空白でスキップ):"
		EnterCertificateThumbprint  = "証明書拇印 (空白でスキップ):"
		EnterTimestampServer        = "タイムスタンプサーバー (空白でデフォルト):"
		SkippingCodeSigning         = "コード署名をスキップしました。"
		BuildingCommand             = "コマンドを生成中..."
		ExecutingCommand            = "コマンドを実行中..."
		CompileSuccess              = "ファイルは正常にコンパイルされました。"
		CompileFailed               = "コンパイルに失敗しました。終了コード: {0}"
		CompileFailedException      = "コンパイルに失敗しました: {0}"
		CompileAnother              = "続けてコンパイルしますか？"
		Exiting                     = "インタラクティブモードを終了します。"
	}
	exe21spInteractI18nData      = @{
		ModeName                 = "インタラクティブモード"
		Welcome                  = "exe21sp インタラクティブモードへようこそ。いつでも Ctrl+C で終了できます。"
		EnterInputFile           = "入力 exe のパスまたは URL を入力してください："
		Prompt                   = " >> "
		ExitMessage              = "インタラクティブモードを終了しました。"
		InvalidInputFile         = "有効な exe のパスまたは URL を入力してください。"
		FileDoesNotExist         = "ファイルが存在しません。"
		EnterOutputFile          = "出力ファイルのパスを入力してください（空白の場合は同じフォルダの <exe>.ps1）:"
		OutputFileExtensionError	= "出力ファイルは'.ps1'の拡張子である必要があります。追加します。"
		AdditionalInfoPrompt     = "[Y/N]"
		ConvertAnother           = "別の exe を変換しますか？"
		Exiting                  = "インタラクティブモードを終了します。"
	}
	exe21spI18nData              = @{
		NoneInput                    = "入力ファイルが指定されていません！"
		TinySharpNoTextSection       = "実行ファイルは .NET アセンブリですが、TinySharp レイアウトと一致しません（.text セクションがありません）。"
		TinySharpTextSectionEmpty    = "実行ファイルは .NET アセンブリですが、TinySharp レイアウトと一致しません（.text セクションが空です）。"
		TinySharpCannotReadText      = "実行ファイルは .NET アセンブリですが、TinySharp レイアウトと一致しません（.text を読み取れません）。"
		TinySharpPayloadNotRecovered	= "実行ファイルは .NET アセンブリですが、TinySharp レイアウトと一致しません。スクリプト ペイロードを復元できません。"
		NoEmbeddedScript             = "「{0}」に埋め込みスクリプトが見つかりません（ps12exe ビルドの exe ではないか、ペイロードを復元できません）。"
		CoreExtractNeedsPwsh         = "この exe のペイロードは Brotli 圧縮されています（PowerShell Core ビルド）。exe21sp が解凍できるよう PowerShell 7 (pwsh) をインストールしてください。"
		FileNotFound                 = "ファイルが見つかりません: {0}"
		InputUrlFailed               = "URL からの読み取りに失敗しました: {0}"
	}
}
