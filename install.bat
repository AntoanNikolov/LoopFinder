@echo off
REM ---------------------------------------------------------------------------
REM install.bat - copy the built LoopFinder plugin into the standard system
REM VST3 / VST2 directories on Windows.
REM
REM Usage:  install.bat [build-dir]
REM         (defaults to .\build)
REM
REM Note: %CommonProgramFiles%\VST3 normally requires Administrator rights to
REM write to. If you double-click this script and copies fail, right-click and
REM choose "Run as administrator".
REM ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

set "BUILD_DIR=%~1"
if "%BUILD_DIR%"=="" set "BUILD_DIR=build"

if not exist "%BUILD_DIR%" (
    echo Build directory not found: %BUILD_DIR%
    echo Run: cmake -B %BUILD_DIR% ^&^& cmake --build %BUILD_DIR% --config Release
    exit /b 1
)

set "VST3_TARGET=%CommonProgramFiles%\VST3"
set "VST2_TARGET=%CommonProgramFiles%\VST2"

set /a INSTALLED=0
set /a FAILED=0

echo Searching for built plugins under "%BUILD_DIR%" ...

REM ---- VST3 ----------------------------------------------------------------
REM A JUCE-built VST3 is the OUTER directory named LoopFinder.vst3 that
REM contains a "Contents" subfolder (the inner LoopFinder.vst3 inside that
REM bundle is the DLL, not what we want to copy by itself). The canonical
REM JUCE artefact location is
REM   <build>\LoopFinder_artefacts\<Config>\VST3\LoopFinder.vst3
REM but we try a few configs in case the user built something other than
REM Release.
set "VST3_SRC="
for %%C in (Release RelWithDebInfo MinSizeRel Debug) do (
    if not defined VST3_SRC (
        set "_candidate=%BUILD_DIR%\LoopFinder_artefacts\%%C\VST3\LoopFinder.vst3"
        if exist "!_candidate!\Contents" set "VST3_SRC=!_candidate!"
    )
)

if defined VST3_SRC (
    echo.
    echo Found VST3 bundle:
    echo   !VST3_SRC!
    echo Copying to:
    echo   %VST3_TARGET%\LoopFinder.vst3
    if not exist "%VST3_TARGET%" mkdir "%VST3_TARGET%" 2>nul
    rmdir /s /q "%VST3_TARGET%\LoopFinder.vst3" 2>nul
    xcopy /E /I /Y "!VST3_SRC!" "%VST3_TARGET%\LoopFinder.vst3\" >nul
    if errorlevel 1 (
        echo   ! Copy failed - try running this script as Administrator.
        set /a FAILED+=1
    ) else (
        echo   * VST3 installed
        set /a INSTALLED+=1
    )
) else (
    echo.
    echo   ! No VST3 bundle found at expected JUCE artefact paths under
    echo     %BUILD_DIR%\LoopFinder_artefacts\^<Config^>\VST3\LoopFinder.vst3
    echo     Did the build step produce a VST3? Check the build log above.
)

REM ---- VST2 (only present if VST2 SDK was provided to cmake) --------------
set "VST2_SRC="
for %%C in (Release RelWithDebInfo MinSizeRel Debug) do (
    if not defined VST2_SRC (
        set "_candidate=%BUILD_DIR%\LoopFinder_artefacts\%%C\VST\LoopFinder.dll"
        if exist "!_candidate!" set "VST2_SRC=!_candidate!"
    )
)

if defined VST2_SRC (
    echo.
    echo Found VST2 dll:
    echo   !VST2_SRC!
    echo Copying to:
    echo   %VST2_TARGET%\LoopFinder.dll
    if not exist "%VST2_TARGET%" mkdir "%VST2_TARGET%" 2>nul
    copy /Y "!VST2_SRC!" "%VST2_TARGET%\LoopFinder.dll" >nul
    if errorlevel 1 (
        echo   ! Copy failed - try running this script as Administrator.
        set /a FAILED+=1
    ) else (
        echo   * VST2 installed
        set /a INSTALLED+=1
    )
)

echo.
echo ----- Summary -----
echo Installed !INSTALLED! plugin bundle(s).
if !FAILED! gtr 0 echo !FAILED! copy operation(s) failed - rerun as Administrator.
echo VST3 directory: %VST3_TARGET%
if defined VST2_SRC echo VST2 directory: %VST2_TARGET%

if !INSTALLED! == 0 (
    echo.
    echo No plugin bundles were installed.
    if !FAILED! == 0 (
        echo No LoopFinder.vst3 / LoopFinder.dll was produced by the build.
        echo Make sure the build step actually succeeded - look for errors above.
    )
    exit /b 1
)
exit /b 0
