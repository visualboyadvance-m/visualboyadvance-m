# Microsoft Developer Studio Project File - Name="GBA" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=GBA - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GBA.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GBA.mak" CFG="GBA - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GBA - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "GBA - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "GBA - Win32 ReleaseNoDev" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GBA - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /Zi /O2 /I "include\zlib" /I "include\png" /I "include\cxImage" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "OEMRESOURCE" /D "_AFXDLL" /D "MMX" /D "FINAL_VERSION" /D "BKPT_SUPPORT" /D "DEV_VERSION" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 d3dx8.lib wsock32.lib ddraw.lib dxguid.lib winmm.lib dinput.lib vfw32.lib opengl32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libm" /nodefaultlib:"libmmd" /out:"Release/VisualBoyAdvance.exe" /MAPINFO:EXPORTS /MAPINFO:LINES /OPT:ref
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "GBA - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "include\zlib" /I "include\png" /I "include\cxImage" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "OEMRESOURCE" /D "_AFXDLL" /D "MMX" /D "DEV_VERSION" /D "BKPT_SUPPORT" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 d3dx8.lib wsock32.lib ddraw.lib dxguid.lib winmm.lib dinput.lib vfw32.lib opengl32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"msvcrt" /nodefaultlib:"libc" /nodefaultlib:"libm" /nodefaultlib:"libmmd" /out:"Debug/VisualBoyAdvance.exe" /pdbtype:sept /MAPINFO:EXPORTS /MAPINFO:LINES
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "GBA - Win32 ReleaseNoDev"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseNoDev"
# PROP BASE Intermediate_Dir "ReleaseNoDev"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseNoDev"
# PROP Intermediate_Dir "ReleaseNoDev"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /I "include\zlib" /I "include\png" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "OEMRESOURCE" /D "_AFXDLL" /D "MMX" /D "FINAL_VERSION" /D "BKPT_SUPPORT" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /Zi /O2 /I "include\zlib" /I "include\png" /I "include\cxImage" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "OEMRESOURCE" /D "_AFXDLL" /D "MMX" /D "FINAL_VERSION" /D "BKPT_SUPPORT" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 d3dx8.lib wsock32.lib ddraw.lib dxguid.lib winmm.lib dinput.lib vfw32.lib opengl32.lib /nologo /subsystem:windows /map /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libm" /nodefaultlib:"libmmd" /out:"Release/VisualBoyAdvance.exe" /MAPINFO:EXPORTS /MAPINFO:LINES /OPT:ref
# SUBTRACT BASE LINK32 /nodefaultlib
# ADD LINK32 d3dx8.lib wsock32.lib ddraw.lib dxguid.lib winmm.lib dinput.lib vfw32.lib opengl32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libm" /nodefaultlib:"libmmd" /out:"ReleaseNoDev/VisualBoyAdvance.exe" /MAPINFO:EXPORTS /MAPINFO:LINES /OPT:ref
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "GBA - Win32 Release"
# Name "GBA - Win32 Debug"
# Name "GBA - Win32 ReleaseNoDev"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "GBA"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\2xSaI.cpp
# End Source File
# Begin Source File

SOURCE=..\src\admame.cpp
# End Source File
# Begin Source File

SOURCE=..\src\agbprint.cpp
# End Source File
# Begin Source File

SOURCE=..\src\armdis.cpp
# End Source File
# Begin Source File

SOURCE=..\src\bilinear.cpp
# End Source File
# Begin Source File

SOURCE=..\src\bios.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Cheats.cpp
# End Source File
# Begin Source File

SOURCE=..\src\CheatSearch.cpp
# End Source File
# Begin Source File

SOURCE=..\src\EEprom.cpp
# End Source File
# Begin Source File

SOURCE=..\src\elf.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Flash.cpp
# End Source File
# Begin Source File

SOURCE=..\src\GBA.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Gfx.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Globals.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hq2x.cpp
# End Source File
# Begin Source File

SOURCE=..\src\interframe.cpp
# End Source File
# Begin Source File

SOURCE=..\src\memgzio.c
# End Source File
# Begin Source File

SOURCE=..\src\Mode0.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Mode1.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Mode2.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Mode3.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Mode4.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Mode5.cpp
# End Source File
# Begin Source File

SOURCE=..\src\motionblur.cpp
# End Source File
# Begin Source File

SOURCE=..\src\pixel.cpp
# End Source File
# Begin Source File

SOURCE=..\src\remote.cpp
# End Source File
# Begin Source File

SOURCE=..\src\RTC.cpp
# End Source File
# Begin Source File

SOURCE=..\src\scanline.cpp
# End Source File
# Begin Source File

SOURCE=..\src\simple2x.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Sound.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Sram.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Text.cpp
# End Source File
# Begin Source File

SOURCE=..\src\unzip.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Util.cpp
# End Source File
# End Group
# Begin Group "GB"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\gb\GB.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbCheats.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbDis.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbGfx.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbGlobals.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbMemory.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbPrinter.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbSGB.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbSound.cpp
# End Source File
# End Group
# Begin Group "Dialogs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\win32\AboutDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\AccelEditor.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\AcceleratorManager.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Associate.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Commands.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Directories.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Disassemble.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\ExportGSASnapshot.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\FileDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBACheats.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBCheatsDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBColorDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBDisassemble.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBMapView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBMemoryViewerDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBOamView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBPaletteView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBPrinterDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBTileView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GDBConnection.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GSACodeSelect.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\IOViewer.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Joypad.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\LangSelect.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Logging.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MapView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MemoryViewerAddressSize.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MemoryViewerDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\ModeConfirm.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\OamView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\PaletteView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\ResizeDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\RewindInterval.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\RomInfo.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Throttle.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\TileView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\VideoMode.cpp
# End Source File
# End Group
# Begin Group "Controls"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\win32\BitmapControl.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\ColorButton.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\ColorControl.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Hyperlink.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\KeyboardEdit.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\PaletteViewControl.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\ZoomControl.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\i386\2xSaImmx.asm

!IF  "$(CFG)" == "GBA - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\i386\2xSaImmx.asm
InputName=2xSaImmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	"c:\Program Files\Nasm\nasmw.exe" -D__DJGPP__ -f win32 -o "$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "GBA - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\i386\2xSaImmx.asm
InputName=2xSaImmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	"c:\Program Files\Nasm\nasmw.exe" -D__DJGPP__ -f win32 -o "$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "GBA - Win32 ReleaseNoDev"

# Begin Custom Build
IntDir=.\ReleaseNoDev
InputPath=..\src\i386\2xSaImmx.asm
InputName=2xSaImmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	"c:\Program Files\Nasm\nasmw.exe" -D__DJGPP__ -f win32 -o "$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\win32\AVIWrite.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\BugReport.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\CmdAccelOb.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Direct3D.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\DirectDraw.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\DirectInput.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\DirectSound.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GameOverrides.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\GDIDisplay.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MainWnd.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MainWndCheats.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MainWndFile.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MainWndHelp.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MainWndOptions.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MainWndTools.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MaxScale.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\MemoryViewer.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\OpenGL.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\Reg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\skin.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\skinButton.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\stdafx.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\StringTokenizer.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\VBA.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\VBA.rc
# End Source File
# Begin Source File

SOURCE=..\src\win32\WavWriter.cpp
# End Source File
# Begin Source File

SOURCE=..\src\win32\WinResUtil.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\win32\AboutDialog.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\AccelEditor.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\AcceleratorManager.h
# End Source File
# Begin Source File

SOURCE=..\src\agbprint.h
# End Source File
# Begin Source File

SOURCE="..\src\arm-new.h"
# End Source File
# Begin Source File

SOURCE=..\src\armdis.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Associate.h
# End Source File
# Begin Source File

SOURCE=..\src\AutoBuild.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\AVIWrite.h
# End Source File
# Begin Source File

SOURCE=..\src\bios.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\BitmapControl.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\BugReport.h
# End Source File
# Begin Source File

SOURCE=..\src\Cheats.h
# End Source File
# Begin Source File

SOURCE=..\src\CheatSearch.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\CmdAccelOb.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\ColorButton.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\ColorControl.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Directories.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Disassemble.h
# End Source File
# Begin Source File

SOURCE=..\src\EEprom.h
# End Source File
# Begin Source File

SOURCE=..\src\elf.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\ExportGSASnapshot.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\FileDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\Flash.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GameOverrides.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\GB.h
# End Source File
# Begin Source File

SOURCE=..\src\GBA.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBACheats.h
# End Source File
# Begin Source File

SOURCE=..\src\GBAinline.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbCheats.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBCheatsDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbCodes.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbCodesCB.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBColorDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBDisassemble.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbGlobals.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBMapView.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbMemory.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBMemoryViewerDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBOamView.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBPaletteView.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbPrinter.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBPrinter.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBPrinterDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbSGB.h
# End Source File
# Begin Source File

SOURCE=..\src\gb\gbSound.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GBTileView.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GDBConnection.h
# End Source File
# Begin Source File

SOURCE=..\src\Gfx.h
# End Source File
# Begin Source File

SOURCE=..\src\Globals.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\GSACodeSelect.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Hyperlink.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Input.h
# End Source File
# Begin Source File

SOURCE=..\src\interp.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\IOViewer.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Joypad.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\KeyboardEdit.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\LangSelect.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Logging.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\MainWnd.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\MapView.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\MaxScale.h
# End Source File
# Begin Source File

SOURCE=..\src\memgzio.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\MemoryViewer.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\MemoryViewerAddressSize.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\MemoryViewerDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\ModeConfirm.h
# End Source File
# Begin Source File

SOURCE=..\src\NLS.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\OamView.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\PaletteView.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\PaletteViewControl.h
# End Source File
# Begin Source File

SOURCE=..\src\Port.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Reg.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\ResizeDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\resource.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\RewindInterval.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\RomInfo.h
# End Source File
# Begin Source File

SOURCE=..\src\RTC.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\skin.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\skinButton.h
# End Source File
# Begin Source File

SOURCE=..\src\Sound.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Sound.h
# End Source File
# Begin Source File

SOURCE=..\src\Sram.h
# End Source File
# Begin Source File

SOURCE=..\src\win32.\stdafx.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\StringTokenizer.h
# End Source File
# Begin Source File

SOURCE=..\src\System.h
# End Source File
# Begin Source File

SOURCE=..\src\Text.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\Throttle.h
# End Source File
# Begin Source File

SOURCE=..\src\thumb.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\TileView.h
# End Source File
# Begin Source File

SOURCE=..\src\unzip.h
# End Source File
# Begin Source File

SOURCE=..\src\Util.h
# End Source File
# Begin Source File

SOURCE=..\src\win32.\VBA.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\VideoMode.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\WavWriter.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\WinResUtil.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\ZoomControl.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\src\win32\gbadvance.ico
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\win32\VisualBoyAdvance.exe.manifest
# End Source File
# Begin Source File

SOURCE=.\lib\win32\libpngMD.lib
# End Source File
# Begin Source File

SOURCE=.\lib\win32\zlibMD.lib
# End Source File
# Begin Source File

SOURCE=.\lib\win32\CxImage.lib
# End Source File
# Begin Source File

SOURCE=.\lib\win32\jpeg.lib
# End Source File
# End Target
# End Project
