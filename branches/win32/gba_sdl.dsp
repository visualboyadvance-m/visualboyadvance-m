# Microsoft Developer Studio Project File - Name="gba_sdl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=gba_sdl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "gba_sdl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gba_sdl.mak" CFG="gba_sdl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gba_sdl - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "gba_sdl - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "gba_sdl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "SDLRelease"
# PROP Intermediate_Dir "SDLRelease"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "../src" /I "include/zlib" /I "include/png" /I "SDL-1.2.2/include" /D "NDEBUG" /D "FINAL_VERSION" /D "DEV_VERSION" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "BKPT_SUPPORT" /D "MMX" /D "SDL" /D "PROFILING" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /machine:I386 /nodefaultlib:"libmmd" /nodefaultlib:"libircmt" /out:"SDLRelease/VisualBoyAdvance-SDL.exe" /MAPINFO:EXPORTS /MAPINFO:LINES
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "gba_sdl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "SDLDebug"
# PROP Intermediate_Dir "SDLDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "../src" /I "include/zlib" /I "include/png" /I "SDL-1.2.2/include" /D "_DEBUG" /D "DEV_VERSION" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "BKPT_SUPPORT" /D "MMX" /D "SDL" /D "PROFILING" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /map /debug /machine:I386 /nodefaultlib:"libmmd" /nodefaultlib:"libircmt" /out:"SDLDebug/VisualBoyAdvance-SDL.exe" /MAPINFO:EXPORTS /MAPINFO:LINES
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "gba_sdl - Win32 Release"
# Name "gba_sdl - Win32 Debug"
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

SOURCE=..\src\bilinear.cpp
# End Source File
# Begin Source File

SOURCE=..\src\bios.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Cheats.cpp

!IF  "$(CFG)" == "gba_sdl - Win32 Release"

# ADD CPP /W3

!ELSEIF  "$(CFG)" == "gba_sdl - Win32 Debug"

!ENDIF 

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
# Begin Group "SDL"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\armdis.cpp
# End Source File
# Begin Source File

SOURCE=..\src\sdl\debugger.cpp
# End Source File
# Begin Source File

SOURCE="..\src\expr-lex.cpp"
# End Source File
# Begin Source File

SOURCE=..\src\expr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\exprNode.cpp
# End Source File
# Begin Source File

SOURCE=..\src\getopt.c
# End Source File
# Begin Source File

SOURCE=..\src\getopt1.c
# End Source File
# Begin Source File

SOURCE=..\src\prof\prof.cpp
# End Source File
# Begin Source File

SOURCE=..\src\remote.cpp
# End Source File
# Begin Source File

SOURCE=..\src\sdl\SDL.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Text.cpp
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\src\i386\2xSaImmx.asm

!IF  "$(CFG)" == "gba_sdl - Win32 Release"

# Begin Custom Build
OutDir=.\SDLRelease
InputPath=..\src\i386\2xSaImmx.asm
InputName=2xSaImmx

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	"c:\Program Files\Nasm\nasmw.exe" -D__DJGPP__ -f win32 -o $(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "gba_sdl - Win32 Debug"

# Begin Custom Build
OutDir=.\SDLDebug
InputPath=..\src\i386\2xSaImmx.asm
InputName=2xSaImmx

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	"c:\Program Files\Nasm\nasmw.exe" -D__DJGPP__ -f win32 -o $(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE="SDL-1.2.2\lib\SDLmain.lib"
# End Source File
# Begin Source File

SOURCE="SDL-1.2.2\lib\SDL.lib"
# End Source File
# Begin Source File

SOURCE=lib\win32\zlibMD.lib
# End Source File
# Begin Source File

SOURCE=lib\win32\libpngMD.lib
# End Source File
# End Target
# End Project
