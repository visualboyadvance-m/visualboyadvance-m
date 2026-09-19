@{
	LangName                     = "Français"
	LangID                       = "fr-FR"
	# Right click Menu
	CompileTitle                 = "Compiler en EXE"
	OpenInGUI                    = "Ouvrir dans ps12exeGUI"
	GUICfgFileDesc               = "Fichier de configuration ps12exe GUI"
	VSCodeExtensionInstalling    = "Installation de l'extension ps12exe pour {0}..."
	VSCodeExtensionInstallFailed = "Impossible d'installer l'extension ps12exe pour {0} (elle n'est peut-être pas encore publiée) : {1}"
	VSCodeExtensionUninstalling  = "Désinstallation de l'extension ps12exe pour {0}..."
	VSCodeExtensionUninstallFailed = "Impossible de désinstaller l'extension ps12exe pour {0} : {1}"
	# Web Server
	ErrorHead                    = "Erreur :"
	CompileResult                = "Résultat de la compilation"
	DefaultResult                = "Terminé !"
	AskSaveCfg                   = "Faut-il enregistrer le fichier de configuration ?"
	AskSaveCfgTitle              = "Enregistrer le fichier de configuration"
	CfgFileLabelHead             = "Fichier de configuration :"
	# Console
	ServerStarted                = "Serveur HTTP démarré !"
	ServerStopped                = "Serveur HTTP arrêté !"
	ServerStartFailed            = "Échec du démarrage du serveur HTTP !"
	TryRunAsRoot                 = "Veuillez essayer d’exécuter en tant qu’administrateur."
	ServerListening              = "Adresse d’accès :"
	ExitServerTip                = "Vous pouvez quitter le serveur à tout moment en appuyant sur Ctrl+C"
	# GUI
	ConsoleHelpData              = @{
		title      = "Utilisation :"
		Usage      = "[input |] ps12exe [[-inputFile] '<nom_de_fichier|url>' | -Content '<script>'] [-outputFile '<nom_de_fichier>']
	[-App @{Windowed=`$true; Silence=@('Output','Error'); OutputEncoding='UTF8'|'UTF16LE'|'Default';
	VisualStyles=`$true; ExitOnCancel=`$true; CredentialGUI=`$true; DpiAware=`$true; WinFormsDpiAware=`$true}]
	[-Os @{Admin=`$true; ModernOS=`$true; LongPaths=`$true; Virtualize=`$true}]
	[-Build @{Target='Framework4.0'|'Framework2.0'|'Core'; Platform='AnyCpu'|'x64'|'x86'; Apartment='STA'|'MTA';
	Culture='<culture>'; Options='<options>'; KeepSource=`$true; Minify={<scriptblock>}; TempDir='<dossier>'}]
	[-Resources @{Icon='<nom_de_fichier|url>'; Title='<titre>'; Description='<description>'; Company='<société>';
	Product='<produit>'; Copyright='<copyright>'; Trademark='<marque_déposée>'; Version='<version>'}]
	[-Signing @{Certificate='<chemin_du_fichier_PFX>'; Password='<mot_de_passe_PFX>'; Thumbprint='<empreinte_numérique_de_certificat>'; Timestamp='<serveur_de_timestamp>'}]
	[-PreprocessOnly] [-Golf] [-Sandbox] [-NoUpdateCheck] [-Locale '<code_de_langue>'] [-ConfigFile] [-help]"
		PrarmsData = [ordered]@{
			input            = "Chaîne de caractères du contenu du fichier de script PowerShell, identique à ``-Content``."
			inputFile        = "Chemin d’accès ou URL du fichier de script PowerShell que vous voulez convertir en exécutable (le fichier doit être encodé en UTF8 ou UTF16)."
			Content          = "Contenu du script PowerShell que vous voulez convertir en exécutable."
			outputFile       = "Nom du fichier exécutable cible ou dossier, par défaut ``inputFile`` avec l’extension ``'.exe'``."
			App              = [ordered]@{
				Windowed         = "Le fichier exécutable généré sera une application Windows Forms sans fenêtre de console."
				Silence          = "Noms des flux à rendre silencieux ; un ou plusieurs parmi ``'Output'``, ``'Verbose'``, ``'Error'``, ``'Warning'``, ``'Debug'``, ou ``'*'`` pour tous."
				OutputEncoding   = "Encodage de sortie de la console ; ``'Default'``, ``'UTF8'`` ou ``'UTF16LE'``."
				VisualStyles     = "Active les styles visuels pour les applications GUI (par défaut `` `$true ``)."
				ExitOnCancel     = "Quitte le programme lorsqu'Annuler ou ``'X'`` est sélectionné dans la boîte de dialogue ``Read-Host``."
				CredentialGUI    = "Utilise une invite GUI pour les informations d'identification en mode console."
				DpiAware         = "Marque le fichier exécutable compilé comme compatible DPI."
				WinFormsDpiAware = "Laisse WinForms utiliser la mise à l'échelle DPI (nécessite Windows 10 et .Net 4.7 ou supérieur)."
			}
			Os               = [ordered]@{
				Admin      = "Si UAC est activé, l'exécutable compilé ne peut s'exécuter que dans un contexte élevé (une boîte de dialogue UAC apparaîtra si nécessaire)."
				ModernOS   = "Utilise les fonctionnalités de la dernière version de Windows (exécutez ``[Environment]::OSVersion`` pour voir la différence)."
				LongPaths  = "Active les chemins longs (``> 260`` caractères) si activé sur l'OS (ne fonctionne qu'avec Windows 10 ou plus récent)."
				Virtualize = "La virtualisation de l'application est activée (force le runtime x86)."
			}
			Build            = [ordered]@{
				Target     = "Version du runtime cible, par défaut ``'Framework4.0'``, prend également en charge ``'Framework2.0'`` et ``'Core'``. ``'Core'`` produit un exécutable PowerShell Core (.NET) (nécessite PowerShell Core et .NET sur les machines de compilation et cible ; le résultat est bien plus volumineux)."
				Platform   = "Compile uniquement pour un runtime spécifique. Les valeurs possibles sont ``'AnyCpu'``, ``'x64'`` et ``'x86'``."
				Apartment  = "Mode ``'Appartement à un seul thread'`` ou ``'Appartement à plusieurs threads'``."
				Culture    = "Culture du fichier exécutable compilé. Si non spécifié, la culture de l'utilisateur actuel sera utilisée."
				Options    = "Options de compilation supplémentaires (voir ``https://msdn.microsoft.com/en-us/library/78f4aasd.aspx``)."
				KeepSource = "Crée des informations utiles pour le débogage."
				Minify     = "Bloc de script pour réduire la taille du script avant la compilation."
				TempDir    = "Répertoire pour stocker les fichiers temporaires (par défaut un répertoire temporaire aléatoire généré dans ``%temp%``)."
			}
			Resources        = [ordered]@{
				Icon        = "Icône de l'exécutable ; peut être un chemin de fichier ou une URL."
				Title       = "Titre (description du fichier) de l'exécutable."
				Description = "Brève description de l'exécutable."
				Company     = "Nom de la société de l'exécutable."
				Product     = "Nom du produit de l'exécutable."
				Copyright   = "Mention de droits d'auteur de l'exécutable."
				Trademark   = "Informations sur la marque de l'exécutable."
				Version     = "Numéro de version de l'exécutable (par exemple ``'1.0.0.0'``)."
			}
			Signing          = [ordered]@{
				Certificate = "Chemin du fichier de certificat PFX ; vous devez spécifier ``Certificate`` ou ``Thumbprint``."
				Password    = "Mot de passe du certificat PFX."
				Thumbprint  = "Empreinte du certificat ; vous devez spécifier ``Certificate`` ou ``Thumbprint``."
				Timestamp   = "URL du serveur d'horodatage utilisé pour la signature de code."
			}
			PreprocessOnly   = "Prétraite le script d'entrée et le retourne sans compilation."
			Golf             = "Activer le mode golf : ajoute des abréviations et des fonctions courantes au script."
			Sandbox          = "Compiler le script avec une protection supplémentaire, empêchant l’accès aux fichiers natifs."
			NoUpdateCheck    = "Ignorer la vérification de la nouvelle version de ps12exe."
			Locale           = "Spécifier la langue de localisation."
			ConfigFile       = "Écrire un fichier de configuration (``<fichier_de_sortie>.exe.config``)."
			Help             = "Affiche cette aide."
		}
	}
	GUIHelpData                  = @{
		title      = "Utilisation :"
		Usage      = @"
ps12exeGUI [[-ConfigFile] '<fichier_de_configuration>'] [-PS1File '<fichier_de_script>'] [-Locale '<code_de_langue>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]

ps12exeGUI [[-PS1File] '<fichier_de_script>'] [-Locale '<code_de_langue>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]
"@
		PrarmsData = [ordered]@{
			ConfigFile	= "Fichier de configuration à charger."
			PS1File    = "Fichier de script à compiler."
			Locale     = "Code de langue à utiliser."
			UIMode     = "Mode d’interface utilisateur."
			help       = "Affiche cette aide."
		}
	}
	SetContextMenuHelpData       = @{
		title      = "Utilisation :"
		Usage      = "Set-ps12exeContextMenu [[-action] 'enable'|'disable'|'reset'] [-Locale '<code_de_langue>'] [-SkipEditorExtension] [-help]"
		PrarmsData = [ordered]@{
			action              = "Action à exécuter."
			Locale              = "Code de langue à utiliser."
			SkipEditorExtension	= "Ignore l'installation ou la désinstallation de l'extension ps12exe VS Code dans les éditeurs détectés."
			help                = "Affiche cette aide."
		}
	}
	WebServerHelpData            = @{
		title      = "Utilisation :"
		Usage      = "Start-ps12exeWebServer [[-HostUrl] '<url>'] [-MaxCompileThreads '<uint>'] [-MaxCompileTime '<uint>']
	[-ReqLimitPerMin '<uint>'] [-MaxCachedFileSize '<uint>'] [-MaxScriptFileSize '<uint>'] [-CacheDir '<chemin>']
	[-Locale '<code_de_langue>'] [-help]"
		PrarmsData = [ordered]@{
			HostUrl           = "Adresse du serveur HTTP à enregistrer."
			MaxCompileThreads = "Nombre maximal de threads de compilation."
			MaxCompileTime    = "Temps de compilation maximal (secondes)."
			ReqLimitPerMin    = "Limite de requêtes par minute et par adresse IP."
			MaxCachedFileSize = "Taille maximale du fichier mis en cache."
			MaxScriptFileSize = "Taille maximale du fichier script."
			CacheDir          = "Répertoire du cache."
			Locale            = "Code de langue à utiliser pour les journaux du côté serveur."
			help              = "Affiche cette aide."
		}
	}
	exe21spHelpData              = @{
		title      = "Utilisation :"
		Usage      = "[input |] exe21sp [[-inputFile] '<chemin ou url exe>'] [-outputFile '<chemin .ps1>'] [-help]"
		PrarmsData = [ordered]@{
			input      = "Chemin ou URL vers l'exe généré par ps12exe à décompiler, identique à ``-inputFile``."
			inputFile  = "Chemin ou URL vers l'exe généré par ps12exe à décompiler."
			outputFile = "Optionnel ; chemin du script .ps1 récupéré. Si omis : sortie sur stdout si redirigé, sinon écriture dans ``<exe>.ps1`` dans le même dossier."
			help       = "Afficher cette aide."
		}
	}
	CompilingI18nData            = @{
		NewVersionAvailable                       = "Une nouvelle version de ps12exe est disponible : {0} !"
		NoneInput                                 = "Aucun fichier d'entrée spécifié !"
		BothInputAndContentSpecified              = "Impossible de spécifier à la fois un fichier et du contenu !"
		PreprocessDone                            = "Prétraitement du script d’entrée terminé."
		PreprocessedScriptSize                    = "Script prétraité -> {0} octets."
		MinifyingScript                           = "Compression du script en cours..."
		MinifyedScriptSize                        = "Script compressé -> {0} octets."
		MinifyerError                             = "Erreur du compresseur : {0}"
		MinifyerFailedUsingOriginalScript         = "Échec du compresseur, utilisation du script d’origine."
		TempFileMissing                           = "Fichier temporaire introuvable {0} !"
		PreprocessOnlyDone                        = "Prétraitement seulement terminé."
		InvalidResourceParam                      = "Clé non valide pour le paramètre -Resources : {0}"
		InputSyntaxError                          = "Erreur de syntaxe du script !"
		SyntaxErrorLineStart                      = "Ligne {0}, colonne {1} :"
		IdenticalInputOutput                      = "Le fichier d’entrée est identique au fichier de sortie !"
		CombinedArg_Virtualize_requireAdmin       = "-Os @{Virtualize=`$true} ne peut pas être utilisé avec -Os @{Admin=`$true}"
		CombinedArg_Virtualize_supportOS          = "-Os @{Virtualize=`$true} ne peut pas être utilisé avec -Os @{ModernOS=`$true}"
		CombinedArg_Virtualize_longPaths          = "-Os @{Virtualize=`$true} ne peut pas être utilisé avec -Os @{LongPaths=`$true}"
		CombinedArg_NoConfigFile_LongPaths        = "La génération d’un fichier de configuration est forcée, car l’option -Os @{LongPaths=`$true} nécessite ce fichier."
		CombinedArg_NoConfigFile_winFormsDPIAware = "La génération d’un fichier de configuration est forcée, car l’option -App @{WinFormsDpiAware=`$true} nécessite ce fichier."
		SomeCmdletsMayNotAvailable                = "Des commandes susceptibles de ne pas être disponibles au moment de l’exécution ont été utilisées {0}, assurez-vous de les avoir vérifiées !"
		SomeNotFoundCmdlets                       = "Les commandes inconnues {0} ont été utilisées."
		SomeTypesMayNotAvailable                  = "Des types susceptibles de ne pas être disponibles au moment de l’exécution ont été utilisés {0}, assurez-vous de les avoir vérifiés !"
		CompilingFile                             = "Compilation en cours..."
		CompilationFailed                         = "Échec de la compilation !"
		OutputFileNotWritten                      = "Fichier de sortie non écrit {0}"
		CompiledFileSize                          = "Fichier compilé -> {0} octets"
		OopsSomethingWentWrong                    = "Oups, quelque chose s’est mal passé."
		TryUpgrade                                = "La dernière version est {0}, essayer de mettre à niveau ?"
		EnterToSubmitIssue                        = "Appuyez sur Entrée pour soumettre un problème pour obtenir de l’aide."
		GuestModeFileTooLarge                     = "Le fichier {0} est trop grand pour être lu."
		GuestModeIconFileTooLarge                 = "L’icône {0} est trop grande pour être lue."
		GuestModeFtpNotSupported                  = "FTP n’est pas pris en charge en mode Sandbox."
		IconFileNotFound                          = "Fichier d’icône introuvable : {0}"
		ReadFileFailed                            = "Échec de la lecture du fichier : {0}"
		PreprocessUnknownIfCondition              = "Condition inconnue : {0}\nSupposé être faux."
		PreprocessNestedIfDeadCode                = "#_if {0} imbriqué dans #_if {1} : la condition externe fixe déjà cette branche, donc un côté est du code mort."
		PreprocessMissingEndIf                    = "Fin de if manquante : {0}"
		PreprocessPsexeBranchCode                 = "Le code d’une branche #_if PSEXE n’est ni #_!! ni un commentaire ; il s’exécute donc aussi lorsque le script est exécuté directement."
		PreprocessPsscriptBranchBang              = "#_!! dans une branche #_if PSScript est un commentaire lors de l’exécution directe du script ; utilisez du code normal ici."
		ConfigFileCreated                         = "Fichier de configuration créé pour l’EXE."
		SourceFileCopied                          = "Nom du fichier source copié pour le débogage : {0}"
		CoreCompilePublishing                     = "Publishing single-file executable with the .NET SDK..."
		CoreCompileNeedDotnet                     = "PowerShell Core compilation requires the .NET SDK (dotnet). Install it, or pass -Build @{Target='Framework4.0'}."
		CoreCompileUnsupported                    = "These options are not supported by the PowerShell Core compiler yet: {0}"
		CoreCompileNeedPwsh                       = "This is Windows PowerShell; -Build @{Target='Core'} needs PowerShell Core (pwsh) installed and on PATH."
		CoreCompileNeedWindowsPowerShell          = "Windows PowerShell was not found; pass -Build @{Target='Core'} to compile a PowerShell Core executable."
		CoreCompileNeedPwshHost                   = "The compiled ps12exe executable cannot build PowerShell Core executables; run ps12exe from the script/module under pwsh instead."
		CoreCompileHint                           = "If this is a PowerShell Core-only script, pass -Build @{Target='Core'} (requires PowerShell Core and .NET on the build and target machines; the resulting exe is much larger)."
		ReadingFile                               = "Lecture de {0}, {1} octets."
		ForceX86byVirtualization                  = "La virtualisation d’application est activée, forçant l’utilisation de la plateforme x86."
		TryingTinySharpCompile                    = "Le résultat est une constante, essayez le compilateur TinySharp..."
		TinySharpFailedFallback                   = "Erreur du compilateur TinySharp, retour au framework du programme normal."
		OutputPath                                = "Chemin d’accès : {0}"
		ReadingScriptDone                         = "Lecture de {0} terminée, début du prétraitement..."
		PreprocessScriptDone                      = "Prétraitement de {0} terminé."
		ConstEvalStart                            = "Calcul de la constante en cours..."
		ConstEvalDone                             = "Calcul de la constante terminé -> {0} octets."
		ConstEvalTooLongFallback                  = "Le résultat de la constante est trop long, retour au framework de programme normal."
		ConstEvalTimeoutFallback                  = "Le calcul de la constante a pris {0} secondes, délai dépassé. Retour au framework de programme normal."
		ConstEvalThrowErrorFallback               = "Une erreur s’est produite lors du calcul de la constante, retour au framework de programme normal."
		ConstEvalNotConstFallback                 = "Le script s'est déclaré non constant, retour au framework de programme normal."
		InvalidArchitecture                       = "Plateforme {0} non valide, utilisation de AnyCpu."
		UnknownPragma                             = "Pragma inconnu : {0}."
		UnknownPragmaBadParameterType             = "Pragma inconnu : {0}, impossible d’analyser le type {1}."
		UnknownPragmaBoolValue                    = "Valeur pragma inconnue : {0}, impossible de la traiter comme un booléen."
		PragmaUnsafeExpression                    = "Expression non sécurisée dans le pragma {0} : {1}"
		DllExportDelNoneTypeArg                   = "{0} : {1} est un argument sans type, on suppose qu’il s’agit d’une chaîne."
		DllExportUsing                            = "Vous utilisez #_DllExport, cette macro est encore en développement, elle n’est pas encore prise en charge."
	}
	WebServerI18nData            = @{
		CompilingUserInput  = "Compilation de la saisie utilisateur en cours : {0}"
		EmptyResponse       = "Aucune donnée n’a été trouvée lors du traitement de la demande, renvoi d’une réponse vide."
		InputTooLarge413    = "La saisie utilisateur est trop grande, renvoi d’une erreur 413."
		ReqLimitExceeded429 = "L’adresse IP {0} a dépassé la limite de {1} requêtes par minute, renvoi d’une erreur 429."
	}
	InteractI18nData             = @{
		ModeName                    = "Mode interactif"
		Welcome                     = "Bienvenue au mode interactif ps12exe. Appuyez sur Ctrl+C pour quitter à tout moment."
		EnterInputFile              = "Veuillez entrer le chemin ou l'URL du fichier d'entrée :"
		Prompt                      = " >> "
		ExitMessage                 = "Mode interactif quitté."
		InvalidInputFile            = "Le chemin du fichier PS1 n'est pas valide. Veuillez réessayer :"
		FileDoesNotExist            = "Le fichier n'existe pas."
		InvalidExtension            = "Le fichier doit avoir l'extension '.ps1', '.psd1' ou '.tmp'."
		EnterOutputFile             = "Veuillez entrer le chemin du fichier de sortie (laisser vide pour <ps1>.exe dans le même dossier) :"
		OutputFileExtensionError    = "Le fichier de sortie doit avoir l'extension '.exe'. L'extension sera ajoutée."
		AddAdditionalInfo           = "Ajouter des informations supplémentaires (icône, version, etc.) ?"
		AdditionalInfoPrompt        = "[Y/N]"
		CollectingInfo              = "Collecte d'informations supplémentaires. Laisser vide si non nécessaire."
		IconPath                    = "Chemin du fichier d'icône ou URL (prend en charge .ico, .png, .jpg, .jpeg, .bmp, etc., laisser vide pour ignorer) :"
		InvalidIconExtension        = "Le fichier doit être au format '.ico'. L'icône sera ignorée."
		IconDoesNotExist            = "Le fichier d'icône n'existe pas, veuillez réessayer."
		EnterTitle                  = "Titre"
		EnterDescription            = "Description"
		EnterCompany                = "Nom de l'entreprise"
		EnterProduct                = "Nom du produit"
		EnterCopyright              = "Copyright"
		EnterTrademark              = "Marque déposée"
		EnterResourcePrompt         = "Veuillez entrer {0}"
		Version                     = "Version (par exemple, 1.0.0.0) :"
		InvalidVersionFormat        = "Format de version invalide. Valeur ignorée."
		SkippingAdditionalInfo      = "Informations supplémentaires ignorées."
		CompileAsGui                = "Compiler en tant qu'application graphique (sans console) ?"
		RequireAdmin                = "Requiert les privilèges administrateur ?"
		EnableCodeSigning           = "Activer la signature de code ?"
		EnterCertificatePath        = "Chemin du certificat ou URL (.pfx, laisser vide pour ignorer) :"
		InvalidCertificateExtension = "Le fichier de certificat doit être au format .pfx, veuillez réessayer."
		CertificateDoesNotExist     = "Le fichier de certificat n'existe pas, veuillez réessayer."
		EnterCertificatePassword    = "Mot de passe du certificat (laisser vide pour ignorer) :"
		EnterCertificateThumbprint  = "Empreinte du certificat (laisser vide pour ignorer) :"
		EnterTimestampServer        = "Serveur d'horodatage (laisser vide pour la valeur par défaut) :"
		SkippingCodeSigning         = "Signature de code ignorée."
		BuildingCommand             = "Préparation de la commande..."
		ExecutingCommand            = "Exécution de la commande..."
		CompileSuccess              = "Fichier compilé avec succès"
		CompileFailed               = "La compilation a échoué avec le code de sortie {0}"
		CompileFailedException      = "Échec de la compilation : {0}"
		CompileAnother              = "Compiler un autre fichier ?"
		Exiting                     = "Quittant le mode interactif."
	}
	exe21spInteractI18nData      = @{
		ModeName                 = "Mode interactif"
		Welcome                  = "Bienvenue au mode interactif exe21sp. Appuyez sur Ctrl+C pour quitter à tout moment."
		EnterInputFile           = "Veuillez entrer le chemin ou l'URL de l'exe d'entrée :"
		Prompt                   = " >> "
		ExitMessage              = "Mode interactif quitté."
		InvalidInputFile         = "Veuillez entrer un chemin ou une URL d'exe valide."
		FileDoesNotExist         = "Le fichier n'existe pas."
		EnterOutputFile          = "Veuillez entrer le chemin du fichier de sortie (laisser vide pour <exe>.ps1 dans le même dossier) :"
		OutputFileExtensionError	= "Le fichier de sortie doit avoir l'extension '.ps1'. L'extension sera ajoutée."
		AdditionalInfoPrompt     = "[Y/N]"
		ConvertAnother           = "Convertir un autre exe ?"
		Exiting                  = "Quittant le mode interactif."
	}
	exe21spI18nData              = @{
		NoneInput                    = "Aucun fichier d'entrée spécifié !"
		TinySharpNoTextSection       = "L'exécutable est un assembly .NET mais ne correspond pas au layout TinySharp (pas de section .text)."
		TinySharpTextSectionEmpty    = "L'exécutable est un assembly .NET mais ne correspond pas au layout TinySharp (section .text vide)."
		TinySharpCannotReadText      = "L'exécutable est un assembly .NET mais ne correspond pas au layout TinySharp (impossible de lire .text)."
		TinySharpPayloadNotRecovered = "L'exécutable est un assembly .NET mais ne correspond pas au layout TinySharp ; charge utile du script non récupérable."
		NoEmbeddedScript             = "Aucun script incorporé dans « {0} » (exe non construit par ps12exe ou charge utile non récupérable)."
		CoreExtractNeedsPwsh         = "La charge utile de cet exe est compressée en Brotli (build PowerShell Core). Installez PowerShell 7 (pwsh) pour qu'exe21sp puisse la décompresser."
		FileNotFound                 = "Fichier introuvable : {0}"
		InputUrlFailed               = "Échec de la lecture depuis l'URL : {0}"
	}
}
