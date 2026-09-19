@{
	LangName                     = "Español (España)"
	LangID                       = "es-ES"
	# Right click Menu
	CompileTitle                 = "Compilar a EXE"
	OpenInGUI                    = "Abrir en ps12exeGUI"
	GUICfgFileDesc               = "Archivo de configuración de ps12exe GUI"
	VSCodeExtensionInstalling    = "Instalando la extensión ps12exe para {0}..."
	VSCodeExtensionInstallFailed = "No se pudo instalar la extensión ps12exe para {0} (puede que aún no esté publicada): {1}"
	VSCodeExtensionUninstalling  = "Desinstalando la extensión ps12exe para {0}..."
	VSCodeExtensionUninstallFailed = "No se pudo desinstalar la extensión ps12exe para {0}: {1}"
	# Web Server
	ErrorHead                    = "Error:"
	CompileResult                = "Resultado de la compilación"
	DefaultResult                = "¡Hecho!"
	AskSaveCfg                   = "¿Desea guardar el archivo de configuración?"
	AskSaveCfgTitle              = "Guardar archivo de configuración"
	CfgFileLabelHead             = "Archivo de configuración:"
	# Console
	ServerStarted                = "¡Servidor HTTP iniciado!"
	ServerStopped                = "¡Servidor HTTP detenido!"
	ServerStartFailed            = "Error al iniciar el servidor HTTP."
	TryRunAsRoot                 = "Vuelva a intentarlo como usuario root."
	ServerListening              = "Dirección de acceso:"
	ExitServerTip                = "Puede presionar Ctrl+C para salir del servidor en cualquier momento."
	# GUI
	ConsoleHelpData              = @{
		title      = "Uso:"
		Usage      = "[input |] ps12exe [[-inputFile] '<nombre de archivo|url>' | -Content '<script>'] [-outputFile '<nombre de archivo>']
	[-App @{Windowed=`$true; Silence=@('Output','Error'); OutputEncoding='UTF8'|'UTF16LE'|'Default';
	VisualStyles=`$true; ExitOnCancel=`$true; CredentialGUI=`$true; DpiAware=`$true; WinFormsDpiAware=`$true}]
	[-Os @{Admin=`$true; ModernOS=`$true; LongPaths=`$true; Virtualize=`$true}]
	[-Build @{Target='Framework4.0'|'Framework2.0'|'Core'; Platform='AnyCpu'|'x64'|'x86'; Apartment='STA'|'MTA';
	Culture='<cultura>'; Options='<opciones>'; KeepSource=`$true; Minify={<scriptblock>}; TempDir='<carpeta>'}]
	[-Resources @{Icon='<nombre de archivo|url>'; Title='<título>'; Description='<descripción>'; Company='<compañía>';
	Product='<producto>'; Copyright='<derechos de autor>'; Trademark='<marca>'; Version='<versión>'}]
	[-Signing @{Certificate='<ruta_del_archivo_PFX>'; Password='<contraseña_PFX>'; Thumbprint='<huella_digital_del_certificado>'; Timestamp='<servidor_de_marca_de_tiempo>'}]
	[-PreprocessOnly] [-Golf] [-Sandbox] [-NoUpdateCheck] [-Locale '<código de idioma>'] [-ConfigFile] [-help]"
		PrarmsData = [ordered]@{
			input            = "La cadena del contenido del archivo de script de PowerShell, igual que ``-Content``."
			inputFile        = "Ruta o URL del script de PowerShell a convertir (el archivo debe estar codificado en UTF-8 o UTF-16)."
			Content          = "El contenido del script de PowerShell que desea convertir en un archivo ejecutable"
			outputFile       = "El nombre del archivo o carpeta de destino, por defecto es el ``inputFile`` con la extensión ``'.exe'``"
			App              = [ordered]@{
				Windowed         = "El archivo ejecutable generado será una aplicación de Windows Forms sin ventana de consola."
				Silence          = "Nombres de flujos a silenciar; uno o varios de ``'Output'``, ``'Verbose'``, ``'Error'``, ``'Warning'``, ``'Debug'``, o ``'*'`` para todos."
				OutputEncoding   = "Codificación de salida de la consola; ``'Default'``, ``'UTF8'`` o ``'UTF16LE'``."
				VisualStyles     = "Habilitar los estilos visuales para las aplicaciones GUI (por defecto `` `$true ``)."
				ExitOnCancel     = "Salir del programa cuando se elija Cancelar o ``'X'`` en el cuadro de entrada de ``Read-Host``."
				CredentialGUI    = "Usar una GUI para solicitar credenciales en el modo de consola."
				DpiAware         = "Marcar el archivo ejecutable compilado como DPI aware."
				WinFormsDpiAware = "Permitir que WinForms use el escalado DPI (requiere Windows 10 y .Net 4.7 o superior)."
			}
			Os               = [ordered]@{
				Admin      = "Si se habilita el UAC, el archivo ejecutable compilado sólo se podrá ejecutar en un contexto elevado (si es necesario, aparecerá el cuadro de diálogo del UAC)."
				ModernOS   = "Usar las características de las últimas versiones de Windows (ejecutar ``[Environment]::OSVersion`` para ver las diferencias)."
				LongPaths  = "Habilitar las rutas largas (``> 260`` caracteres) si están habilitadas en el sistema operativo (sólo para Windows 10 o superior)."
				Virtualize = "Se ha activado la virtualización de aplicaciones (se fuerza el tiempo de ejecución x86)."
			}
			Build            = [ordered]@{
				Target     = "Versión de tiempo de ejecución de destino, ``'Framework4.0'`` por defecto; se admiten ``'Framework2.0'`` y ``'Core'``. ``'Core'`` genera un ejecutable de PowerShell Core (.NET) (requiere PowerShell Core y .NET en las máquinas de compilación y de destino; el resultado es mucho mayor)."
				Platform   = "Compilar sólo para un tiempo de ejecución específico. Los valores posibles son ``'AnyCpu'``, ``'x64'`` y ``'x86'``."
				Apartment  = "Modo ``'apartamento de un solo hilo'`` o ``'apartamento de varios hilos'``."
				Culture    = "Referencia cultural del archivo ejecutable compilado. Si no se especifica, será la cultura del usuario actual."
				Options    = "Opciones adicionales del compilador (ver ``https://msdn.microsoft.com/en-us/library/78f4aasd.aspx``)."
				KeepSource = "Crear información que ayude a la depuración."
				Minify     = "Bloque de script que reduce el tamaño del script antes de la compilación."
				TempDir    = "El directorio donde se almacenan los archivos temporales (por defecto es un directorio temporal generado aleatoriamente en ``%temp%``)."
			}
			Resources        = [ordered]@{
				Icon        = "Icono del ejecutable; puede ser una ruta de archivo o una URL."
				Title       = "Título (descripción del archivo) del ejecutable."
				Description = "Descripción breve del ejecutable."
				Company     = "Nombre de la compañía del ejecutable."
				Product     = "Nombre del producto del ejecutable."
				Copyright   = "Aviso de derechos de autor del ejecutable."
				Trademark   = "Información de marca del ejecutable."
				Version     = "Número de versión del ejecutable (por ejemplo ``'1.0.0.0'``)."
			}
			Signing          = [ordered]@{
				Certificate = "Ruta del archivo de certificado PFX; se debe especificar ``Certificate`` o ``Thumbprint``."
				Password    = "Contraseña del certificado PFX."
				Thumbprint  = "Huella digital del certificado; se debe especificar ``Certificate`` o ``Thumbprint``."
				Timestamp   = "URL del servidor de marca de tiempo usado para la firma de código."
			}
			PreprocessOnly   = "Preprocesa el script de entrada y devuélvelo sin compilar"
			Golf             = "Activar el modo golf, agregando abreviaturas y funciones comunes"
			Sandbox          = "Compilación de scripts con protección adicional frente al acceso a archivos nativos"
			NoUpdateCheck    = "Omitir la comprobación de nuevas versiones de ps12exe"
			Locale           = "El código de idioma que desea usar"
			ConfigFile       = "Escribir un archivo de configuración (``<outputfile>.exe.config``)"
			Help             = "Mostrar esta información de ayuda"
		}
	}
	GUIHelpData                  = @{
		title      = "Uso:"
		Usage      = @"
ps12exeGUI [[-ConfigFile] '<archivo de configuración>'] [-PS1File '<archivo de código>'] [-Locale '<código de idioma>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]

ps12exeGUI [[-PS1File] '<archivo de código>'] [-Locale '<código de idioma>'] [-UIMode 'Dark'|'Light'|'Auto'] [-help]
"@
		PrarmsData = [ordered]@{
			ConfigFile	= "El archivo de configuración que desea cargar."
			PS1File    = "El archivo de script a compilar."
			Locale     = "El código de idioma que desea usar."
			UIMode     = "Modo de interfaz."
			help       = "Mostrar esta información de ayuda."
		}
	}
	SetContextMenuHelpData       = @{
		title      = "Uso:"
		Usage      = "Set-ps12exeContextMenu [[-action] 'enable'|'disable'|'reset'] [-Locale '<código de idioma>'] [-SkipEditorExtension] [-help]"
		PrarmsData = [ordered]@{
			action              = "Acción a ejecutar."
			Locale              = "El código de idioma que desea usar."
			SkipEditorExtension	= "Omitir la instalación o desinstalación de la extensión ps12exe VS Code en los editores detectados."
			help                = "Mostrar esta información de ayuda."
		}
	}
	WebServerHelpData            = @{
		title      = "Uso:"
		Usage      = "Start-ps12exeWebServer [[-HostUrl] '<url>'] [-MaxCompileThreads '<uint>'] [-MaxCompileTime '<uint>']
	[-ReqLimitPerMin '<uint>'] [-MaxCachedFileSize '<uint>'] [-MaxScriptFileSize '<uint>'] [-CacheDir '<donde>']
	[-Locale '<código de idioma>'] [-help]"
		PrarmsData = [ordered]@{
			HostUrl           = "La dirección del servidor HTTP que se registrará."
			MaxCompileThreads = "El número máximo de hilos de compilación."
			MaxCompileTime    = "El tiempo máximo de compilación (segundos)."
			ReqLimitPerMin    = "El número de solicitudes por minuto para cada IP."
			MaxCachedFileSize = "El tamaño máximo de archivo en caché."
			MaxScriptFileSize = "El tamaño máximo de archivo de script."
			CacheDir          = "El directorio donde se almacenan los archivos caché."
			Locale            = "El código de idioma para el registro en el lado del servidor."
			help              = "Mostrar esta información de ayuda."
		}
	}
	exe21spHelpData              = @{
		title      = "Uso:"
		Usage      = "[input |] exe21sp [[-inputFile] '<ruta o url al exe>'] [-outputFile '<ruta al .ps1>'] [-help]"
		PrarmsData = [ordered]@{
			input      = "Ruta o URL al exe generado por ps12exe a descompilar, igual que ``-inputFile``."
			inputFile  = "Ruta o URL al exe generado por ps12exe a descompilar."
			outputFile = "Opcional; ruta donde escribir el script recuperado. Si se omite: salida a stdout si está redirigido, si no se escribe en ``<exe>.ps1`` en la misma carpeta."
			help       = "Mostrar esta ayuda."
		}
	}
	CompilingI18nData            = @{
		NewVersionAvailable                       = "¡Hay una nueva versión de ps12exe disponible: {0}!"
		NoneInput                                 = "¡No se ha especificado ningún archivo de entrada!"
		BothInputAndContentSpecified              = "¡No se puede usar el archivo de entrada y el contenido al mismo tiempo!"
		PreprocessDone                            = "Finalización de la preprocesación del script de entrada"
		PreprocessedScriptSize                    = "Script preprocesado -> {0} bytes"
		MinifyingScript                           = "Minificando script..."
		MinifyedScriptSize                        = "Script minificado -> {0} bytes"
		MinifyerError                             = "Error del minificador: {0}"
		MinifyerFailedUsingOriginalScript         = "Falló el minificador, utilizando el script original."
		TempFileMissing                           = "¡No se encontró el archivo temporal {0}!"
		PreprocessOnlyDone                        = "Finalización de la preprocesación del script de entrada"
		InvalidResourceParam                      = "Parámetro -Resources con una clave inválida: {0}"
		InputSyntaxError                          = "¡Error de sintaxis en el script!"
		SyntaxErrorLineStart                      = "En la línea {0}, Col {1}:"
		IdenticalInputOutput                      = "¡El archivo de entrada es idéntico al archivo de salida!"
		CombinedArg_Virtualize_requireAdmin       = "-Os @{Virtualize=`$true} no se puede usar con -Os @{Admin=`$true}"
		CombinedArg_Virtualize_supportOS          = "-Os @{Virtualize=`$true} no se puede usar con -Os @{ModernOS=`$true}"
		CombinedArg_Virtualize_longPaths          = "-Os @{Virtualize=`$true} no se puede usar con -Os @{LongPaths=`$true}"
		CombinedArg_NoConfigFile_LongPaths        = "Forzando la generación de un archivo de configuración, ya que la opción -Os @{LongPaths=`$true} lo requiere"
		CombinedArg_NoConfigFile_winFormsDPIAware = "Forzando la generación de un archivo de configuración, ya que la opción -App @{WinFormsDpiAware=`$true} lo requiere"
		SomeCmdletsMayNotAvailable                = "¡Se usaron cmdlets {0} pero pueden no estar disponibles en tiempo de ejecución, asegúrese de haberlos verificado!"
		SomeNotFoundCmdlets                       = "Se usaron funciones desconocidas {0}"
		SomeTypesMayNotAvailable                  = "¡Se usaron tipos {0} pero pueden no estar disponibles en tiempo de ejecución, asegúrese de haberlos verificado!"
		CompilingFile                             = "Compilando archivo..."
		CompilationFailed                         = "¡Falló la compilación!"
		OutputFileNotWritten                      = "No se escribió el archivo de salida {0}"
		CompiledFileSize                          = "Archivo compilado escrito -> {0} bytes"
		OopsSomethingWentWrong                    = "Vaya, algo salió mal."
		TryUpgrade                                = "¿La última versión es {0}, intenta actualizarla?"
		EnterToSubmitIssue                        = "Para obtener ayuda, envíe un problema presionando Enter."
		GuestModeFileTooLarge                     = "El archivo {0} es demasiado grande para leerlo."
		GuestModeIconFileTooLarge                 = "El icono {0} es demasiado grande para leerlo."
		GuestModeFtpNotSupported                  = "FTP no es compatible en el modo Sandbox."
		IconFileNotFound                          = "No se encontró el archivo de icono: {0}"
		ConvertingImageToIcon                     = "Convirtiendo imagen a formato de icono..."
		ImageConvertedToIcon                      = "Imagen convertida a icono: {0}"
		ImageConversionFailed                     = "Error en la conversión de imagen: {0}"
		PleaseUseIcoFile                          = "Por favor use un archivo .ico en lugar de {0}"
		SigningExecutable                         = "Firmando ejecutable..."
		ExecutableSignedSuccessfully              = "Ejecutable firmado exitosamente."
		SigningStatusNotValid                     = "Estado de firma no válido: {0} - {1}"
		CertificateNotFoundOrInvalidPassword      = "Certificado no encontrado o contraseña inválida."
		SigningFailed                             = "Error al firmar: {0}"
		ReadFileFailed                            = "Falló la lectura del archivo: {0}"
		PreprocessUnknownIfCondition              = "Condición desconocida: {0}`nSe asume que es falso."
		PreprocessNestedIfDeadCode                = "#_if {0} anidado dentro de #_if {1}: la condición externa ya fija esta rama, así que un lado es código muerto."
		PreprocessMissingEndIf                    = "Falta el final de la declaración if: {0}"
		PreprocessPsexeBranchCode                 = "El código de una rama #_if PSEXE no es #_!! ni un comentario, por lo que también se ejecuta al ejecutar el script directamente."
		PreprocessPsscriptBranchBang              = "#_!! en una rama #_if PSScript es un comentario al ejecutar el script directamente; usa código normal aquí."
		ConfigFileCreated                         = "Se creó el archivo de configuración para EXE"
		SourceFileCopied                          = "Nombre de archivo fuente copiado para depuración: {0}"
		CoreCompilePublishing                     = "Publishing single-file executable with the .NET SDK..."
		CoreCompileNeedDotnet                     = "PowerShell Core compilation requires the .NET SDK (dotnet). Install it, or pass -Build @{Target='Framework4.0'}."
		CoreCompileUnsupported                    = "These options are not supported by the PowerShell Core compiler yet: {0}"
		CoreCompileNeedPwsh                       = "This is Windows PowerShell; -Build @{Target='Core'} needs PowerShell Core (pwsh) installed and on PATH."
		CoreCompileNeedWindowsPowerShell          = "Windows PowerShell was not found; pass -Build @{Target='Core'} to compile a PowerShell Core executable."
		CoreCompileNeedPwshHost                   = "The compiled ps12exe executable cannot build PowerShell Core executables; run ps12exe from the script/module under pwsh instead."
		CoreCompileHint                           = "If this is a PowerShell Core-only script, pass -Build @{Target='Core'} (requires PowerShell Core and .NET on the build and target machines; the resulting exe is much larger)."
		ReadingFile                               = "Leyendo {0}, tamaño {1} bytes"
		ForceX86byVirtualization                  = "Se activó la virtualización de aplicaciones, forzando la plataforma x86."
		TryingTinySharpCompile                    = "Resultado constante, intentando el compilador TinySharp..."
		TinySharpFailedFallback                   = "Error del compilador TinySharp, retroceso al marco de programa normal"
		OutputPath                                = "Ruta: {0}"
		ReadingScriptDone                         = "Finalización de la lectura de {0}, inicio de la preprocesación..."
		PreprocessScriptDone                      = "Finalización de la preprocesación del archivo {0}"
		ConstEvalStart                            = "Evaluación de constantes..."
		ConstEvalDone                             = "Finalización de la evaluación de constantes -> {0} bytes"
		ConstEvalTooLongFallback                  = "El resultado de la constante es demasiado largo, retroceso al marco de programa normal"
		ConstEvalTimeoutFallback                  = "La evaluación de la constante se agotó después de {0} segundos, retroceso al marco de programa normal"
		ConstEvalThrowErrorFallback               = "Error al evaluar la constante, retroceso al marco de programa normal"
		ConstEvalNotConstFallback                 = "El script se declaró no constante, retroceso al marco de programa normal"
		InvalidArchitecture                       = "Plataforma inválida {0}, utilizando AnyCpu"
		UnknownPragma                             = "Pragma desconocido: {0}"
		UnknownPragmaBadParameterType             = "Pragma desconocido: {0}, no se puede analizar el tipo {1}."
		UnknownPragmaBoolValue                    = "Valor de pragma desconocido: {0}, no se puede tomar como booleano."
		PragmaUnsafeExpression                    = "Expresión insegura en el pragma {0}: {1}"
		DllExportDelNoneTypeArg                   = "{0}: {1} es un parámetro de tipo nulo, se asume que es una cadena."
		DllExportUsing                            = "Está utilizando #_DllExport, esta macro está en desarrollo y aún no es compatible."
	}
	WebServerI18nData            = @{
		CompilingUserInput  = "Compilando entrada de usuario: {0}"
		EmptyResponse       = "No se encontraron datos al manejar la solicitud, se devuelve una respuesta vacía"
		InputTooLarge413    = "La entrada del usuario es demasiado grande, se devuelve un error 413"
		ReqLimitExceeded429 = "La IP {0} ha superado el límite de {1} solicitudes por minuto, se devuelve un error 429"
	}
	InteractI18nData             = @{
		ModeName                    = "Modo interactivo"
		Welcome                     = "Bienvenido al modo interactivo de ps12exe. Pulsa Ctrl+C para salir en cualquier momento."
		EnterInputFile              = "Introduce la ruta o URL del archivo de entrada:"
		Prompt                      = " >> "
		ExitMessage                 = "Saliste del modo interactivo."
		InvalidInputFile            = "La ruta del archivo PS1 no es válida. Inténtalo de nuevo:"
		FileDoesNotExist            = "El archivo no existe."
		InvalidExtension            = "El archivo debe tener la extensión '.ps1', '.psd1' o '.tmp'."
		EnterOutputFile             = "Introduce la ruta del archivo de salida (dejar en blanco para <ps1>.exe en la misma carpeta):"
		OutputFileExtensionError    = "El archivo de salida debe ser un '.exe'. Se añadirá la extensión por ti."
		AddAdditionalInfo           = "¿Quieres añadir información adicional (icono, versión, etc.)?"
		AdditionalInfoPrompt        = "[Y/N]"
		CollectingInfo              = "Recopilando información adicional. Dejar en blanco si no es necesario."
		IconPath                    = "Ruta del archivo de icono o URL (admite .ico, .png, .jpg, .jpeg, .bmp, etc., dejar en blanco para omitir):"
		InvalidIconExtension        = "El archivo debe tener la extensión '.ico'. Se ha ignorado."
		IconDoesNotExist            = "El archivo de icono no existe, por favor vuelva a ingresar."
		EnterTitle                  = "Título"
		EnterDescription            = "Descripción"
		EnterCompany                = "Nombre de la empresa"
		EnterProduct                = "Nombre del producto"
		EnterCopyright              = "Copyright"
		EnterTrademark              = "Marca registrada"
		EnterResourcePrompt         = "Introduce {0}"
		Version                     = "Versión (por ejemplo, 1.0.0.0):"
		InvalidVersionFormat        = "Formato de versión no válido. Se ha ignorado."
		SkippingAdditionalInfo      = "Vale, se omite la información adicional."
		CompileAsGui                = "¿Compilar como aplicación GUI (sin consola)?"
		RequireAdmin                = "¿Requerir privilegios de administrador?"
		EnableCodeSigning           = "¿Habilitar la firma de código?"
		EnterCertificatePath        = "Ruta del certificado o URL (.pfx, dejar en blanco para omitir):"
		InvalidCertificateExtension = "El archivo de certificado debe ser formato .pfx, por favor vuelva a ingresar."
		CertificateDoesNotExist     = "El archivo de certificado no existe, por favor vuelva a ingresar."
		EnterCertificatePassword    = "Contraseña del certificado (dejar en blanco para omitir):"
		EnterCertificateThumbprint  = "Huella digital del certificado (dejar en blanco para omitir):"
		EnterTimestampServer        = "Servidor de marca de tiempo (dejar en blanco para el valor predeterminado):"
		SkippingCodeSigning         = "Omitiendo la firma de código."
		BuildingCommand             = "Generando el comando..."
		ExecutingCommand            = "Ejecutando el comando..."
		CompileSuccess              = "Archivo compilado correctamente"
		CompileFailed               = "La compilación ha fallado (código de salida: {0})."
		CompileFailedException      = "Error de compilación: {0}"
		CompileAnother              = "¿Compilar otro archivo?"
		Exiting                     = "Saliendo del modo interactivo."
	}
	exe21spInteractI18nData      = @{
		ModeName                 = "Modo interactivo"
		Welcome                  = "Bienvenido al modo interactivo de exe21sp. Pulsa Ctrl+C para salir en cualquier momento."
		EnterInputFile           = "Introduce la ruta o URL del exe de entrada:"
		Prompt                   = " >> "
		ExitMessage              = "Saliste del modo interactivo."
		InvalidInputFile         = "Introduce una ruta o URL de exe válida."
		FileDoesNotExist         = "El archivo no existe."
		EnterOutputFile          = "Introduce la ruta del archivo de salida (dejar en blanco para <exe>.ps1 en la misma carpeta):"
		OutputFileExtensionError	= "El archivo de salida debe ser '.ps1'. Se añadirá la extensión."
		AdditionalInfoPrompt     = "[Y/N]"
		ConvertAnother           = "¿Convertir otro exe?"
		Exiting                  = "Saliendo del modo interactivo."
	}
	exe21spI18nData              = @{
		NoneInput                    = "¡No se ha especificado ningún archivo de entrada!"
		TinySharpNoTextSection       = "El ejecutable es un ensamblado .NET pero no coincide con el diseño TinySharp (no hay sección .text)."
		TinySharpTextSectionEmpty    = "El ejecutable es un ensamblado .NET pero no coincide con el diseño TinySharp (la sección .text está vacía)."
		TinySharpCannotReadText      = "El ejecutable es un ensamblado .NET pero no coincide con el diseño TinySharp (no se puede leer .text)."
		TinySharpPayloadNotRecovered	= "El ejecutable es un ensamblado .NET pero no coincide con el diseño TinySharp; no se puede recuperar el script."
		NoEmbeddedScript             = "No se encontró script incrustado en '{0}' (no es un exe de ps12exe o no se puede recuperar la carga)."
		CoreExtractNeedsPwsh         = "La carga de este exe está comprimida con Brotli (compilación de PowerShell Core). Instale PowerShell 7 (pwsh) para que exe21sp pueda descomprimirla."
		FileNotFound                 = "Archivo no encontrado: {0}"
		InputUrlFailed               = "Error al leer desde la URL: {0}"
	}
}
