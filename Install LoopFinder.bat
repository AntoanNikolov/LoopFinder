@echo off
REM ===========================================================================
REM Install LoopFinder.bat
REM
REM Two modes:
REM   1. Pre-built mode (end users)
REM      If a LoopFinder.vst3 bundle sits next to this script, the script just
REM      copies it into %CommonProgramFiles%\VST3. No prerequisites required,
REM      no compilation. ~5 seconds, plug-and-play.
REM
REM   2. Source-build mode (developers / source clone)
REM      If no pre-built bundle is found, the script builds LoopFinder from
REM      source. This requires CMake, Git, and Visual Studio with the
REM      "Desktop development with C++" workload. Takes a few minutes.
REM
REM The script self-elevates to Administrator (one UAC prompt) because writing
REM to %CommonProgramFiles%\VST3 requires admin rights.
REM ===========================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

REM ---- 0. self-elevate to Administrator ---------------------------------
REM Writing to C:\Program Files\Common Files\VST3 needs admin rights, so
REM relaunch ourselves with elevation if we aren't already admin. The user
REM sees one UAC prompt and then the installer runs.
net session >nul 2>&1
if errorlevel 1 (
    echo.
    echo Administrator rights are required to install plugins into
    echo   %CommonProgramFiles%\VST3
    echo.
    echo Re-launching with elevation - please accept the User Account Control prompt...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs" >nul 2>&1
    if errorlevel 1 (
        echo.
        echo   ^^! Could not self-elevate automatically. Please right-click
        echo     "Install LoopFinder.bat" and choose "Run as administrator".
        echo.
        pause
    )
    exit /b 0
)

set "EXIT_CODE=0"
set "BUNDLE_SRC="

echo.
echo ===========================================
echo   LoopFinder - Installer
echo ===========================================
echo.
echo This will install LoopFinder into:
echo   %CommonProgramFiles%\VST3\LoopFinder.vst3
echo.
echo Quit your DAW before continuing so the existing
echo plugin (if any) can be replaced cleanly.
echo.
choice /c YN /m "Continue?"
if errorlevel 2 (
    echo Cancelled.
    set "EXIT_CODE=0"
    goto :end
)

REM ---- 1. pre-built bundle? ----------------------------------------------
REM Anatomy of a JUCE Windows VST3 bundle:
REM   LoopFinder.vst3\Contents\x86_64-win\LoopFinder.vst3   (the actual DLL)
REM   LoopFinder.vst3\Contents\Resources\moduleinfo.json
REM We test for the inner DLL because just LoopFinder.vst3\Contents could
REM exist on a partially-extracted ZIP.
echo.
echo === Looking for a pre-built bundle ===
if exist "%~dp0LoopFinder.vst3\Contents\x86_64-win\LoopFinder.vst3" (
    set "BUNDLE_SRC=%~dp0LoopFinder.vst3"
    echo   * Found pre-built bundle next to this installer:
    echo       !BUNDLE_SRC!
    echo     Skipping compilation - going straight to install.
    goto :do_install
)
echo   - No pre-built bundle found next to this installer.
echo     Falling back to building from source (requires CMake / Git / VS C++).

REM ---- 2. tools (only required for source build) -------------------------
echo.
echo === Checking required tools ===
where cmake >nul 2>&1
if errorlevel 1 (
    echo   ^^! CMake is not installed or not on PATH.
    echo     Install it from https://cmake.org/download/ and tick
    echo     "Add CMake to the system PATH" during setup.
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

REM ---- Visual Studio C++ toolchain ---------------------------------------
REM Without an explicit generator, cmake on a machine that has cmake but no
REM Visual Studio falls back to "NMake Makefiles" and then dies because
REM nmake.exe (which ships with VS) isn't installed either. Detect VS up
REM front so we can fail fast with a helpful message AND so we can pass
REM the right -G to cmake regardless of what cmake's auto-detect would do.
set "VS_GENERATOR="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    REM -prerelease -all -products * is the most permissive query: it finds
    REM Community / Professional / Enterprise / Build Tools, prerelease, etc.
    for /f "usebackq tokens=1 delims=." %%v in (`"%VSWHERE%" -latest -prerelease -all -products * -property installationVersion 2^>nul`) do set "VS_MAJOR=%%v"
)

if "%VS_MAJOR%"=="18" set "VS_GENERATOR=Visual Studio 18 2026"
if "%VS_MAJOR%"=="17" set "VS_GENERATOR=Visual Studio 17 2022"
if "%VS_MAJOR%"=="16" set "VS_GENERATOR=Visual Studio 16 2019"
if "%VS_MAJOR%"=="15" set "VS_GENERATOR=Visual Studio 15 2017"

REM Directory-scan fallback for unusual installs that vswhere can't see.
if not defined VS_GENERATOR (
    for %%E in (Community Professional Enterprise BuildTools Preview) do (
        if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E"        set "VS_GENERATOR=Visual Studio 17 2022"
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\%%E"   set "VS_GENERATOR=Visual Studio 17 2022"
        if exist "%ProgramFiles%\Microsoft Visual Studio\2019\%%E"        set "VS_GENERATOR=Visual Studio 16 2019"
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\%%E"   set "VS_GENERATOR=Visual Studio 16 2019"
        if exist "%ProgramFiles%\Microsoft Visual Studio\2017\%%E"        set "VS_GENERATOR=Visual Studio 15 2017"
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\%%E"   set "VS_GENERATOR=Visual Studio 15 2017"
        if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E"          set "VS_GENERATOR=Visual Studio 18 2026"
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\%%E"     set "VS_GENERATOR=Visual Studio 18 2026"
    )
)

if not defined VS_GENERATOR (
    echo   ^^! No Visual Studio C++ toolchain was found on this machine.
    echo.
    echo     This source-tree install needs Microsoft's C++ compiler. Either:
    echo.
    echo       (a) Get a pre-built release ZIP from whoever sent you LoopFinder
    echo           - it will contain a LoopFinder.vst3 bundle next to the
    echo           installer and won't need any compilers, OR
    echo.
    echo       (b) Install Visual Studio 2022 Community for free:
    echo             1. https://visualstudio.microsoft.com/downloads/
    echo             2. In the installer, tick "Desktop development with C++"
    echo                (other defaults are fine, ^~7 GB download)
    echo             3. Run this installer again when it finishes.
    echo.
    choice /c YN /m "Open the Visual Studio download page in your browser now?"
    if not errorlevel 2 start "" "https://visualstudio.microsoft.com/downloads/"
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
        echo   ^^! Failed to download JUCE. Check your internet connection.
        set "EXIT_CODE=1"
        goto :end
    )
)
echo   * JUCE present

REM ---- 4. configure ^& build ---------------------------------------------
REM Detect a stale or foreign build cache (macOS / Linux / different VS /
REM different platform) and wipe so cmake can reconfigure cleanly.
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
        echo   - Detected a stale or foreign build cache - wiping 'build' folder
        echo     so we can reconfigure cleanly for !VS_GENERATOR!...
        rmdir /s /q build
    )
)

REM Make sure no inherited CMAKE_GENERATOR env var overrides our explicit -G.
set "CMAKE_GENERATOR="

echo.
echo === Configuring ^& building (this may take a few minutes the first time) ===
echo   - Using generator: !VS_GENERATOR! (x64)
cmake -B build -G "%VS_GENERATOR%" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo.
    echo   ^^! cmake configure failed - see output above.
    echo     If the error mentions a missing C++ component, open the Visual
    echo     Studio Installer, click "Modify" on your VS install, and tick
    echo     the "Desktop development with C++" workload, then try again.
    set "EXIT_CODE=1"
    goto :end
)
cmake --build build --config Release --target LoopFinder_All
if errorlevel 1 (
    echo.
    echo   ^^! Build failed - see output above.
    set "EXIT_CODE=1"
    goto :end
)
set "BUNDLE_SRC=%~dp0build\LoopFinder_artefacts\Release\VST3\LoopFinder.vst3"
echo   * Build succeeded
echo   * Bundle: !BUNDLE_SRC!

REM ---- 5. install (copy bundle to system VST3 dir) -----------------------
:do_install
echo.
echo === Installing plugin ===
set "VST3_TARGET=%CommonProgramFiles%\VST3"

if not exist "!BUNDLE_SRC!\Contents\x86_64-win\LoopFinder.vst3" (
    echo   ^^! Bundle not found or is incomplete:
    echo       !BUNDLE_SRC!
    set "EXIT_CODE=1"
    goto :end
)

if not exist "%VST3_TARGET%" mkdir "%VST3_TARGET%" 2>nul
rmdir /s /q "%VST3_TARGET%\LoopFinder.vst3" 2>nul
xcopy /E /I /Y "!BUNDLE_SRC!" "%VST3_TARGET%\LoopFinder.vst3\" >nul
if errorlevel 1 (
    echo   ^^! Failed to copy plugin to %VST3_TARGET%.
    echo     Most likely cause: your DAW is still running and holding the
    echo     existing plugin open. Close it and try again.
    set "EXIT_CODE=1"
    goto :end
)
echo   * Installed to %VST3_TARGET%\LoopFinder.vst3

REM ---- 6. done -----------------------------------------------------------
echo.
echo ===========================================
echo   All done^^!
echo ===========================================
echo.
echo Open your DAW and rescan plugins:
echo.
echo   Ableton Live   -^>  Preferences ^| Plug-Ins ^| Rescan
echo   Reaper         -^>  Preferences ^| Plug-ins ^| VST ^| Re-scan
echo   Cubase         -^>  Studio ^| VST Plug-In Manager ^| refresh
echo   FL Studio      -^>  Options ^| Manage plugins ^| Find more
echo.
echo LoopFinder will appear in the Instruments section of your
echo plugin browser, under the vendor folder LoopFinder.

:end
echo.
echo ===========================================================================
echo   Press any key to close this window...
echo ===========================================================================
pause >nul
endlocal
exit /b %EXIT_CODE%
