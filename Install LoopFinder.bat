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
    pause
    exit /b 0
)

REM ---- 1. tools -----------------------------------------------------------
echo.
echo === Checking required tools ===
where cmake >nul 2>&1
if errorlevel 1 (
    echo   ! cmake is not installed.
    echo     Install it from https://cmake.org/download/
    pause
    exit /b 1
)
where git >nul 2>&1
if errorlevel 1 (
    echo   ! git is not installed.
    echo     Install Git from https://git-scm.com/download/win
    pause
    exit /b 1
)
echo   * cmake and git available

REM ---- 2. JUCE ------------------------------------------------------------
echo.
echo === Checking for JUCE framework ===
if not exist "JUCE\CMakeLists.txt" (
    echo   - JUCE not found - downloading JUCE 8.0.4 (~70MB, one-time)...
    if exist JUCE rmdir /s /q JUCE
    git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
    if errorlevel 1 (
        echo   ! Failed to download JUCE. Check your internet connection.
        pause
        exit /b 1
    )
)
echo   * JUCE present

REM ---- 3. configure & build ----------------------------------------------
echo.
echo === Configuring ^& building (this may take a few minutes the first time) ===
if not exist "build" (
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 (
        echo   ! cmake configure failed.
        pause
        exit /b 1
    )
)
cmake --build build --config Release --target LoopFinder_All
if errorlevel 1 (
    echo   ! Build failed - see output above.
    pause
    exit /b 1
)
echo   * Build succeeded

REM ---- 4. install --------------------------------------------------------
echo.
echo === Installing plugin ===
call install.bat build
if errorlevel 1 (
    echo   ! Install step reported errors - try running this script as Administrator.
    pause
    exit /b 1
)

REM ---- 5. done -----------------------------------------------------------
echo.
echo ===========================================
echo   All done!
echo ===========================================
echo.
echo LoopFinder is installed. Open your DAW and rescan plugins:
echo.
echo   Ableton Live   -^>  Preferences ^| Plug-Ins ^| Rescan
echo   Reaper         -^>  Preferences ^| Plug-ins ^| VST ^| Re-scan
echo   Cubase         -^>  Studio ^| VST Plug-In Manager ^| refresh
echo   FL Studio      -^>  Options ^| Manage plugins ^| Find more
echo.
echo LoopFinder will appear in the Instruments section of your
echo plugin browser, under the vendor folder LoopFinder.
echo.
pause
