@echo off
REM ===========================================================================
REM Build Release.bat
REM
REM Developer-side script. Builds LoopFinder from source and produces a
REM Release\Windows\ folder containing only what an end user needs:
REM
REM   Release\Windows\
REM     Install LoopFinder.bat        (the same installer, but it'll skip the
REM     Uninstall LoopFinder.bat       build because LoopFinder.vst3 is right
REM     LoopFinder.vst3\               next to it - plug-and-play, no CMake /
REM                                    Git / Visual Studio required for the
REM                                    person who runs it)
REM
REM Zip up that folder, send it to a friend, they double-click the installer
REM and they're done in 5 seconds.
REM ===========================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "EXIT_CODE=0"

echo.
echo ===========================================
echo   LoopFinder - Release Builder (Windows)
echo ===========================================
echo.
echo This will build LoopFinder and assemble a redistributable folder at:
echo   %~dp0Release\Windows\
echo.
echo You can then ZIP that folder and send it to anyone with Windows.
echo They will be able to install LoopFinder by double-clicking
echo "Install LoopFinder.bat" - no compilers or other tools required.
echo.
choice /c YN /m "Continue?"
if errorlevel 2 (
    echo Cancelled.
    goto :end
)

REM ---- 1. tools ----------------------------------------------------------
echo.
echo === Checking required tools ===
where cmake >nul 2>&1
if errorlevel 1 (
    echo   ^^! CMake is not installed or not on PATH.
    echo     Install it from https://cmake.org/download/
    set "EXIT_CODE=1"
    goto :end
)
where git >nul 2>&1
if errorlevel 1 (
    echo   ^^! Git is not installed or not on PATH.
    echo     Install Git from https://git-scm.com/download/win
    set "EXIT_CODE=1"
    goto :end
)
echo   * cmake and git available

REM ---- 2. Visual Studio --------------------------------------------------
set "VS_GENERATOR="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=1 delims=." %%v in (`"%VSWHERE%" -latest -prerelease -all -products * -property installationVersion 2^>nul`) do set "VS_MAJOR=%%v"
)
if "%VS_MAJOR%"=="18" set "VS_GENERATOR=Visual Studio 18 2026"
if "%VS_MAJOR%"=="17" set "VS_GENERATOR=Visual Studio 17 2022"
if "%VS_MAJOR%"=="16" set "VS_GENERATOR=Visual Studio 16 2019"
if "%VS_MAJOR%"=="15" set "VS_GENERATOR=Visual Studio 15 2017"

if not defined VS_GENERATOR (
    for %%E in (Community Professional Enterprise BuildTools Preview) do (
        if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E"        set "VS_GENERATOR=Visual Studio 17 2022"
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\%%E"   set "VS_GENERATOR=Visual Studio 17 2022"
        if exist "%ProgramFiles%\Microsoft Visual Studio\2019\%%E"        set "VS_GENERATOR=Visual Studio 16 2019"
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\%%E"   set "VS_GENERATOR=Visual Studio 16 2019"
        if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E"          set "VS_GENERATOR=Visual Studio 18 2026"
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\%%E"     set "VS_GENERATOR=Visual Studio 18 2026"
    )
)

if not defined VS_GENERATOR (
    echo   ^^! Visual Studio C++ toolchain not found.
    echo     Install Visual Studio 2022 Community with the
    echo     "Desktop development with C++" workload, then try again.
    set "EXIT_CODE=1"
    goto :end
)
echo   * Found Visual Studio toolchain: %VS_GENERATOR%

REM ---- 3. JUCE -----------------------------------------------------------
echo.
echo === Checking for JUCE framework ===
if not exist "JUCE\CMakeLists.txt" (
    echo   - JUCE not found - downloading JUCE 8.0.4 ^(~70MB, one-time^)...
    if exist JUCE rmdir /s /q JUCE
    git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
    if errorlevel 1 (
        echo   ^^! Failed to download JUCE.
        set "EXIT_CODE=1"
        goto :end
    )
)
echo   * JUCE present

REM ---- 4. configure ^& build ---------------------------------------------
if exist "build\CMakeCache.txt" (
    set "WIPE_BUILD=0"
    findstr /b /c:"# For build in directory: /" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=Unix Makefiles" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=NMake Makefiles" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=%VS_GENERATOR%" "build\CMakeCache.txt" >nul 2>&1
    if errorlevel 1 (
        findstr /b /c:"CMAKE_GENERATOR:INTERNAL=Visual Studio" "build\CMakeCache.txt" >nul 2>&1
        if not errorlevel 1 set "WIPE_BUILD=1"
    )
    findstr /b /c:"CMAKE_GENERATOR_PLATFORM:INTERNAL=x64" "build\CMakeCache.txt" >nul 2>&1
    if errorlevel 1 set "WIPE_BUILD=1"
    if "!WIPE_BUILD!"=="1" (
        echo   - Wiping stale build cache for a clean Release build...
        rmdir /s /q build
    )
)

set "CMAKE_GENERATOR="

echo.
echo === Configuring ^& building Release ===
cmake -B build -G "%VS_GENERATOR%" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo   ^^! cmake configure failed.
    set "EXIT_CODE=1"
    goto :end
)
cmake --build build --config Release --target LoopFinder_All
if errorlevel 1 (
    echo   ^^! Build failed.
    set "EXIT_CODE=1"
    goto :end
)
echo   * Build succeeded

REM ---- 5. assemble Release\Windows\ --------------------------------------
echo.
echo === Assembling release folder ===
set "BUNDLE_SRC=%~dp0build\LoopFinder_artefacts\Release\VST3\LoopFinder.vst3"
set "RELEASE_DIR=%~dp0Release\Windows"

if not exist "!BUNDLE_SRC!\Contents\x86_64-win\LoopFinder.vst3" (
    echo   ^^! Build did not produce the expected bundle:
    echo       !BUNDLE_SRC!
    set "EXIT_CODE=1"
    goto :end
)

if exist "!RELEASE_DIR!" rmdir /s /q "!RELEASE_DIR!"
mkdir "!RELEASE_DIR!"

REM Copy the pre-built VST3 bundle.
xcopy /E /I /Y "!BUNDLE_SRC!" "!RELEASE_DIR!\LoopFinder.vst3\" >nul
if errorlevel 1 (
    echo   ^^! Failed to copy bundle into the release folder.
    set "EXIT_CODE=1"
    goto :end
)
echo   * Copied LoopFinder.vst3

REM Copy the user-facing scripts.
copy /Y "%~dp0Install LoopFinder.bat"   "!RELEASE_DIR!\" >nul
copy /Y "%~dp0Uninstall LoopFinder.bat" "!RELEASE_DIR!\" >nul
echo   * Copied Install / Uninstall scripts

REM Drop a tiny readme so end users know what to do.
> "!RELEASE_DIR!\README.txt" (
    echo LoopFinder - Windows installer
    echo ==============================
    echo.
    echo To install:
    echo   1. Double-click "Install LoopFinder.bat"
    echo   2. Accept the User Account Control ^(UAC^) prompt that appears.
    echo   3. Press Y to confirm.
    echo.
    echo The plugin will be installed to:
    echo   C:\Program Files\Common Files\VST3\LoopFinder.vst3
    echo.
    echo To uninstall: double-click "Uninstall LoopFinder.bat".
    echo.
    echo No compilers or other developer tools are required.
)
echo   * Wrote README.txt

echo.
echo ===========================================
echo   Done^^!
echo ===========================================
echo.
echo Release folder: !RELEASE_DIR!
echo.
echo Next step: right-click that folder, "Send to ^> Compressed (zipped) folder",
echo and you have a single ZIP you can send to anyone with Windows.

:end
echo.
echo Press any key to close this window...
pause >nul
endlocal
exit /b %EXIT_CODE%
