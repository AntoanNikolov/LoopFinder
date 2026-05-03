@echo off
REM ---------------------------------------------------------------------------
REM install.bat — copy built LoopFinder plugin into the standard system
REM VST3 directory on Windows.
REM
REM Usage:  install.bat [build-dir]
REM         (defaults to .\build)
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

set "INSTALLED=0"

echo Searching for built plugins in %BUILD_DIR% ...

REM ---- VST3 ----
for /r /d "%BUILD_DIR%" %%P in (*.vst3) do (
    if exist "%%P" (
        echo Copying %%P
        echo   to %VST3_TARGET%
        if not exist "%VST3_TARGET%" mkdir "%VST3_TARGET%"
        rmdir /s /q "%VST3_TARGET%\LoopFinder.vst3" 2>nul
        xcopy /E /I /Y "%%P" "%VST3_TARGET%\LoopFinder.vst3\" >nul
        if errorlevel 1 (
            echo   ! Copy failed — try running this script as Administrator.
        ) else (
            set /a INSTALLED+=1
        )
    )
)

REM ---- VST2 ----
for /r "%BUILD_DIR%" %%P in (LoopFinder.dll) do (
    if exist "%%P" (
        echo Copying %%P
        echo   to %VST2_TARGET%
        if not exist "%VST2_TARGET%" mkdir "%VST2_TARGET%"
        copy /Y "%%P" "%VST2_TARGET%\LoopFinder.dll" >nul
        if errorlevel 1 (
            echo   ! Copy failed — try running this script as Administrator.
        ) else (
            set /a INSTALLED+=1
        )
    )
)

echo.
echo ----- Summary -----
echo Installed !INSTALLED! plugin bundle(s).
echo VST3 directory: %VST3_TARGET%
echo VST2 directory: %VST2_TARGET%

if !INSTALLED! == 0 (
    echo No plugin bundles were installed.
    exit /b 1
)
exit /b 0
