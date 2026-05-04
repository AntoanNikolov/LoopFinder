@echo off
REM ===========================================================================
REM Install LoopFinder.bat
REM
REM Double-click in Explorer (or right-click -> "Run as administrator" the
REM first time so it can write to %CommonProgramFiles%\VST3) to build and
REM install the LoopFinder plugin into the standard system plugin folder.
REM
REM Requires: CMake, git, and Visual Studio (Community Edition is fine) with
REM the "Desktop development with C++" workload installed.
REM ===========================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "EXIT_CODE=0"

echo.
echo ===========================================
echo   LoopFinder - Installer
echo ===========================================
echo.
echo This will build and install LoopFinder into:
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

REM ---- 1. tools -----------------------------------------------------------
echo.
echo === Checking required tools ===
where cmake >nul 2>&1
if errorlevel 1 (
    echo   ! cmake is not installed or not on PATH.
    echo     Install it from https://cmake.org/download/ and tick
    echo     "Add CMake to the system PATH" during setup.
    set "EXIT_CODE=1"
    goto :end
)
where git >nul 2>&1
if errorlevel 1 (
    echo   ! git is not installed or not on PATH.
    echo     Install Git from https://git-scm.com/download/win
    set "EXIT_CODE=1"
    goto :end
)
echo   * cmake and git available

REM ---- 2. JUCE ------------------------------------------------------------
echo.
echo === Checking for JUCE framework ===
if not exist "JUCE\CMakeLists.txt" (
    echo   - JUCE not found - downloading JUCE 8.0.4 ^(~70MB, one-time^)...
    if exist JUCE rmdir /s /q JUCE
    git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
    if errorlevel 1 (
        echo   ! Failed to download JUCE. Check your internet connection.
        set "EXIT_CODE=1"
        goto :end
    )
)
echo   * JUCE present

REM ---- 3. configure ^& build ---------------------------------------------
REM Detect a stale or foreign build cache (e.g. one shipped from macOS / Linux,
REM or left over from a different generator) and wipe it so the configure step
REM produces a clean Windows / Visual Studio build tree. Without this, cmake
REM tries to invoke the cached generator (e.g. /usr/bin/make) and fails.
if exist "build\CMakeCache.txt" (
    set "WIPE_BUILD=0"
    findstr /b /c:"# For build in directory: /" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=Unix Makefiles" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    if "!WIPE_BUILD!"=="1" (
        echo   - Detected a foreign build cache - wiping 'build' folder so we can
        echo     reconfigure cleanly for Windows...
        rmdir /s /q build
    )
)

echo.
echo === Configuring ^& building (this may take a few minutes the first time) ===
cmake -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo.
    echo   ! cmake configure failed - see output above.
    echo     The most common cause is a missing Visual Studio C++ toolchain.
    echo     Install "Visual Studio 2022 Community" with the
    echo     "Desktop development with C++" workload, then try again.
    set "EXIT_CODE=1"
    goto :end
)
cmake --build build --config Release --target LoopFinder_All
if errorlevel 1 (
    echo.
    echo   ! Build failed - see output above.
    set "EXIT_CODE=1"
    goto :end
)
echo   * Build succeeded

REM ---- 4. install --------------------------------------------------------
echo.
echo === Installing plugin ===
call "%~dp0install.bat" build
if errorlevel 1 (
    echo.
    echo   ! Install step reported errors.
    echo     %CommonProgramFiles%\VST3 requires Administrator rights to write to,
    echo     so right-click "Install LoopFinder.bat" and choose
    echo     "Run as administrator", then run it again.
    set "EXIT_CODE=1"
    goto :end
)

REM ---- 5. done -----------------------------------------------------------
echo.
echo ===========================================
echo   All done!
echo ===========================================
echo.
echo LoopFinder is installed at:
echo   %CommonProgramFiles%\VST3\LoopFinder.vst3
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
