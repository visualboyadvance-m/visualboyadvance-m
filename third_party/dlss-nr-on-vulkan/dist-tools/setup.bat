@echo off
setlocal
rem ===========================================================================
rem  setup.bat  -  configure a DLSS-NR release to run a game (Windows)
rem
rem  This ships INSIDE the release folder, next to the built layer and the runtime.
rem  It is the last step a user runs; nothing here compiles anything.
rem
rem    1. Find nvngx_dlssnr.dll. Three ways, in the order offered:
rem         a) one sitting next to this script  (drag the pack into a game folder
rem            and drop the DLL beside it - the common case)
rem         b) a folder you pick, searched for the file
rem            (no download: the DLL is NVIDIA's, and this script will not fetch it)
rem            already have it
rem       Nothing is redistributed: the DLL is NVIDIA's and the sizes below are the
rem       only thing checked about it.
rem    2. Extract the logical weights from that DLL (needs Python 3 with NumPy and
rem       safetensors; the script says so and stops if they are missing).
rem    3. Install the layer: copy it under a name you choose, write the manifest so
rem       its library_path is that file, and write a launcher that points the daemon
rem       at this folder.
rem
rem  Usage:
rem    setup.bat                          configure a game folder chosen interactively
rem    setup.bat --game "C:\Games\MyGame"
rem    setup.bat --game ... --dll X:\path\nvngx_dlssnr.dll
rem    setup.bat --game ... --name nr_layer.dll
rem    setup.bat --game ... --skip-weights
rem ===========================================================================

set "GAME="
set "DLL="
set "NAME=nr_layer.dll"
set "SKIP_WEIGHTS=0"
rem The script directory is the release root, and it must be captured before any
rem shift moves %0.
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

:parse
if "%~1"=="" goto :done_parse
if /i "%~1"=="--game"         ( set "GAME=%~2" & shift & shift & goto :parse )
if /i "%~1"=="--dll"          ( set "DLL=%~2"  & shift & shift & goto :parse )
if /i "%~1"=="--name"         ( set "NAME=%~2" & shift & shift & goto :parse )
if /i "%~1"=="--skip-weights" ( set "SKIP_WEIGHTS=1" & shift & goto :parse )
if /i "%~1"=="--dry-run"      ( set "DRY_RUN=1" & shift & goto :parse )
echo Unknown argument: %~1
exit /b 2
:done_parse

echo ============================================================
echo  DLSS-NR on Intel - setup
echo ============================================================
echo.

rem ---- what does this folder actually contain? ----
if not exist "%HERE%\nr_layer.dll" (
  echo ERROR: nr_layer.dll is not in %HERE%
  echo        Run this from inside the release folder it came in.
  exit /b 1
)
if not exist "%HERE%\work" (
  echo ERROR: the work\ folder is missing next to nr_layer.dll
  echo        The release is incomplete; re-extract it.
  exit /b 1
)
echo Release folder: %HERE%
echo.

rem ---- MLX-DLSS: the runtime modules the daemon imports, and the extractor ----
rem Checked before the weights, not with them: the modules are needed to run a game
rem whether or not this machine has extracted weights yet.
set "MLXDEST=%HERE%\work\mlx-dlss\python\mlxdlss"
set "MLXTOOLS=%MLXDEST%\tools"
if not exist "%MLXDEST%\features.py" goto :fetch_mlx
if not exist "%MLXTOOLS%\extract_dlssnr_weights.py" goto :fetch_mlx
goto :have_tools

:fetch_mlx
echo == fetching MLX-DLSS ^(runtime modules and extractor^)
where /q git || ( echo ERROR: git is needed to fetch it. & exit /b 3 )
set "MLXTMP=%TEMP%\nr_mlx"
if exist "%MLXTMP%" rmdir /S /Q "%MLXTMP%" 2>nul
git clone --quiet https://github.com/iamwavecut/MLX-DLSS.git "%MLXTMP%"
if errorlevel 1 (
  echo ERROR: could not clone MLX-DLSS. Check your network.
  exit /b 3
)
git -C "%MLXTMP%" checkout --quiet 06a3e11a8b68817127406ace5c764463543f699b
if errorlevel 1 (
  echo ERROR: could not pin MLX-DLSS.
  exit /b 3
)
if not exist "%MLXDEST%" mkdir "%MLXDEST%" >nul 2>&1
xcopy /E /I /Y /Q "%MLXTMP%\python\mlxdlss" "%MLXDEST%" >nul
rmdir /S /Q "%MLXTMP%" 2>nul
if not exist "%MLXTOOLS%\extract_dlssnr_weights.py" (
  echo ERROR: the extractor did not arrive under %MLXTOOLS%
  exit /b 3
)
if not exist "%MLXDEST%\features.py" (
  echo ERROR: the runtime modules did not arrive under %MLXDEST%
  exit /b 3
)
:have_tools

rem ---- 1. the NVIDIA DLL ----
rem --skip-weights means "the weights are already here", so it has to be checked rather
rem than trusted: a release folder with no weights that still installs and prints Done
rem fails later, in the game, with nothing on screen pointing back to here.
if "%SKIP_WEIGHTS%"=="1" (
  if exist "%HERE%\work\mlxw\dlssnr-logical.safetensors" (
    echo == weights already extracted; --skip-weights trusts that
    goto :after_dll
  )
  echo ERROR: --skip-weights was given but the weights are not here.
  echo        Expected: %HERE%\work\mlxw\dlssnr-logical.safetensors
  echo        Drop --skip-weights to extract them, passing --dll ^<path^> if needed.
  exit /b 3
)

if exist "%HERE%\work\mlxw\dlssnr-logical.safetensors" (
  echo == weights already extracted
  goto :after_dll
)

if not "%DLL%"=="" (
  call :resolve_dll "%DLL%" strict
  if errorlevel 1 (
    echo ERROR: --dll did not name a usable nvngx_dlssnr.dll.
    exit /b 3
  )
  echo == DLL: from the command line - %DLL%
  goto :find_python
)

rem (a) next to this script
if exist "%HERE%\nvngx_dlssnr.dll" (
  set "DLL=%HERE%\nvngx_dlssnr.dll"
  echo == DLL: found next to this script
  goto :find_python
)

rem (b) a folder the user picks
echo == DLL: none beside this script
echo.
echo       Where is yours? You can:
echo         * drag the folder that holds it onto this window and press Enter,
echo         * paste a full path to the DLL or to its folder,
echo         * or press Enter to stop here.
echo.
set "ANSWER="
set /p "ANSWER=Path: "
if not defined ANSWER goto :no_dll
call :resolve_dll "%ANSWER%"
if errorlevel 1 goto :no_dll
echo   using %DLL%
goto :find_python

:no_dll
rem The DLL is NVIDIA's. Every project in this space requires the user to supply it, and
rem this one does too: fetching it for you would mean redistributing it, which is why
rem there is no download here.
echo.
echo   nvngx_dlssnr.dll was not found.
echo.
echo   Put it beside this script, or point --dll ^<path^> at it, or drop it into a
echo   folder and pass that folder. The file is around 158 MB.
echo.
exit /b 3

:find_python
rem ---- 2. Python and the two packages the extractor needs ----
set "PY="
where /q py && set "PY=py"
if not defined PY ( where /q python3 && set "PY=python3" )
if not defined PY ( where /q python && set "PY=python" )
if not defined PY (
  echo ERROR: Python 3 was not found on PATH.
  echo        Install it from python.org, then run this again.
  exit /b 3
)
"%PY%" -c "import numpy, safetensors" >nul 2>&1
if errorlevel 1 (
  echo ERROR: Python is missing the packages the extractor needs.
  echo        Run:  %PY% -m pip install numpy safetensors
  exit /b 3
)

echo == extracting weights from %DLL%
echo       (a few minutes; the DLL is read, nothing is written back to it)
"%PY%" "%HERE%\scripts\get_weights.py" "%DLL%" --work-dir "%HERE%\work"
if errorlevel 1 (
  echo ERROR: extraction failed - see the message above.
  exit /b 3
)
if not exist "%HERE%\work\mlxw\dlssnr-logical.safetensors" (
  echo ERROR: the extractor reported success but the file is not there.
  exit /b 3
)
echo.

:after_dll
rem ---- 3. where is the game ----
if not "%GAME%"=="" goto :have_game
echo == Which game? Drag its folder here (the one with the .exe) and press Enter.
set "ANSWER="
set /p "ANSWER=Game folder: "
if not defined ANSWER (
  echo ERROR: no game folder given.
  exit /b 2
)
set "GAME=%ANSWER:"=%"
:have_game
if not exist "%GAME%" (
  echo ERROR: game folder not found: %GAME%
  exit /b 2
)

rem ---- 4. install ----
rem Everything above this point only reads; everything below writes into the game folder.
rem --dry-run stops here after saying what it would have done, so the whole chain can be
rem checked on a machine nobody wants a layer installed on.
if "%DRY_RUN%"=="1" (
  echo.
  echo == dry run: nothing was written
  echo   would install %HERE%\nr_layer.dll to %GAME%\dlss-nr\%NAME%
  echo   would write %GAME%\dlss-nr\VkLayer_dlss_nr.json with an absolute library_path
  echo   would copy the runtime: work\mlx-dlss, work\*.spv, work\*.dll
  if exist "%HERE%\work\mlxw\dlssnr-logical.safetensors" (
    echo   weights: present, and are NOT copied - the layer points at this folder
  ) else (
    echo   weights: MISSING - a real run would have failed before installing
  )
  echo.
  exit /b 0
)

echo == installing into %GAME%
set "DEST=%GAME%\dlss-nr"
if not exist "%DEST%" mkdir "%DEST%"
copy /Y "%HERE%\nr_layer.dll" "%DEST%\%NAME%" >nul || exit /b 3
echo   layer:      %DEST%\%NAME%

set "MANIFEST=%HERE%\VkLayer_dlss_nr.json"
if not exist "%MANIFEST%" set "MANIFEST=%HERE%\src\layer\VkLayer_dlss_nr.json"
if not exist "%MANIFEST%" (
  echo ERROR: no VkLayer_dlss_nr.json in the release.
  exit /b 3
)
rem An absolute library_path: a bare name makes the loader fail with error 87.
powershell -NoProfile -Command "$lib = (Join-Path '%DEST%' '%NAME%'); (Get-Content '%MANIFEST%') -replace 'LIBRARY_PATH_PLACEHOLDER', $lib.Replace('\','\\') | Set-Content '%DEST%\VkLayer_dlss_nr.json'"
echo   manifest:   %DEST%\VkLayer_dlss_nr.json

rem The runtime the daemon needs, and the sources it imports. The weights stay here in
rem the release folder - the launcher points the daemon at it rather than copying 278 MB
rem into a game directory where it could be passed on by accident.
for %%D in (layer ref gpu bench) do (
  if exist "%HERE%\src\%%D" xcopy /E /I /Y /Q "%HERE%\src\%%D" "%DEST%\src\%%D" >nul
)
if exist "%HERE%\work\mlx-dlss" xcopy /E /I /Y /Q "%HERE%\work\mlx-dlss" "%DEST%\work\mlx-dlss" >nul
copy /Y "%HERE%\work\*.spv" "%DEST%\work\" >nul 2>&1
copy /Y "%HERE%\work\*.dll" "%DEST%\work\" >nul 2>&1
copy /Y "%HERE%\work\*.so"  "%DEST%\work\" >nul 2>&1
echo   runtime:    shaders and libraries

for /d /r "%DEST%\src" %%J in (__pycache__) do @if exist "%%J" rmdir /S /Q "%%J" 2>nul

set "LAUNCH=%DEST%\launch-nr.bat"
(
  echo @echo off
  echo setlocal
  echo.
  echo rem The loader finds the layer through VK_LAYER_PATH plus the manifest.
  echo set "VK_LAYER_PATH=%DEST%"
  echo set "VK_INSTANCE_LAYERS=VK_LAYER_dlssnr_intel"
  echo set "ENABLE_NR_LAYER=1"
  echo set "NR_LAYER_SPAWN=1"
  echo set "NR_LAYER_LIVE=1"
  echo rem Windows talks to the daemon over a named pipe.
  echo set "NR_LAYER_SOCKET=\\.\pipe\nr_dlssnr_intel"
  echo rem The weights stay in the release folder.
  echo set "NR_ROOT=%HERE%"
  echo.
  echo set "GAME_EXE="
  echo for %%%%F in ^("%GAME%\*.exe"^) do ^( if not defined GAME_EXE set "GAME_EXE=%%%%F" ^)
  echo if not defined GAME_EXE ^( echo No .exe in %GAME% ^& exit /b 1 ^)
  echo start "" "%%GAME_EXE%%" %%*
) > "%LAUNCH%"
echo   launcher:   %LAUNCH%

echo.
echo Done.
echo   Run the game through %LAUNCH%
echo   Weights stayed in %HERE%\work\mlxw; the launcher points NR_ROOT there.
echo.
echo COMPLIANCE: nothing NVIDIA's is redistributed by this script. The DLL it read is
echo yours, the weights came from it, and both stay on this machine.
endlocal
exit /b 0

rem A path the user gave - by --dll or interactively - has to become the DLL itself, and
rem the two entry points must agree about what counts as valid. This turns either a file
rem or a folder into a full path to nvngx_dlssnr.dll, or fails.
rem
rem Called with the candidate in NR_CANDIDATE. On success DLL is set and errorlevel is 0;
rem on failure errorlevel is 1 and DLL is cleared. %1, when "strict", refuses a file whose
rem name is not nvngx_dlssnr.dll rather than accepting it with a warning.
:resolve_dll
set "DLL="
rem strip quotes and any trailing backslash, so comparisons behave. The test is spelled
rem with a variable because `if "%X:~-1%"=="\"` puts a backslash against the closing
rem quote and cmd mis-parses it - which killed the script silently.
set "NR_CANDIDATE=%~1"
set "NR_SLASH=\"
:resolve_strip
if not defined NR_CANDIDATE exit /b 1
if "%NR_CANDIDATE:~-1%"=="%NR_SLASH%" (
  set "NR_CANDIDATE=%NR_CANDIDATE:~0,-1%"
  goto :resolve_strip
)
if "%NR_CANDIDATE%"=="" exit /b 1
rem a folder that holds it
if exist "%NR_CANDIDATE%\nvngx_dlssnr.dll" (
  set "DLL=%NR_CANDIDATE%\nvngx_dlssnr.dll"
  exit /b 0
)
rem a bare file: accept only if it is the one we need
if exist "%NR_CANDIDATE%" (
  rem a directory without the DLL in it is not a candidate
  if exist "%NR_CANDIDATE%\" (
    echo   That folder does not contain nvngx_dlssnr.dll.
    exit /b 1
  )
  for %%F in ("%NR_CANDIDATE%") do set "NR_NAME=%%~nxF"
  if /i not "%NR_NAME%"=="nvngx_dlssnr.dll" (
    echo   That file is %NR_NAME%, not nvngx_dlssnr.dll.
    exit /b 1
  )
  set "DLL=%NR_CANDIDATE%"
  exit /b 0
)
echo   %NR_CANDIDATE% does not exist.
exit /b 1
