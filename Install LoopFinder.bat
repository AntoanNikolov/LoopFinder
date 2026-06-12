@echo off
REM ===========================================================================
REM Install LoopFinder.bat  (Windows)
REM
REM Double-click to install. Copies the bundled VST3 into the standard
REM system plug-in folder - no compilers, no downloads.
REM
REM   VST3 -> %CommonProgramFiles%\VST3\LoopFinder.vst3
REM ===========================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

REM ---- Elevate (writing to Common Files\VST3 needs admin) -------------------
net session >nul 2>&1
if errorlevel 1 (
    echo.
    echo Installing to %CommonProgramFiles%\VST3 requires administrator rights.
    echo Accept the User Account Control prompt to continue...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs" >nul 2>&1
    if errorlevel 1 (
        echo.
        echo   Could not elevate. Right-click this file and choose
        echo   "Run as administrator".
        pause
    )
    exit /b 0
)

echo.
echo ===========================================
echo   LoopFinder - Installer (Windows)
echo ===========================================
echo.
echo This will install:
echo   %CommonProgramFiles%\VST3\LoopFinder.vst3
echo.
echo Quit your DAW first so the open plug-in can be replaced.
echo.

REM ---- Locate the bundled VST3 ----------------------------------------------
set "BUNDLE_SRC="
if exist "%~dp0Prebuilt\Windows\LoopFinder.vst3\Contents\x86_64-win\LoopFinder.vst3" (
    set "BUNDLE_SRC=%~dp0Prebuilt\Windows\LoopFinder.vst3"
) else if exist "%~dp0LoopFinder.vst3\Contents\x86_64-win\LoopFinder.vst3" (
    set "BUNDLE_SRC=%~dp0LoopFinder.vst3"
) else if exist "%~dp0build\LoopFinder_artefacts\Release\VST3\LoopFinder.vst3\Contents\x86_64-win\LoopFinder.vst3" (
    set "BUNDLE_SRC=%~dp0build\LoopFinder_artefacts\Release\VST3\LoopFinder.vst3"
)

if not defined BUNDLE_SRC (
    echo error: no LoopFinder.vst3 found in this folder.
    echo.
    echo   Expected: Prebuilt\Windows\LoopFinder.vst3
    echo   Download the latest Windows release from the project page, or build
    echo   from source first ^(see README.md, "Building from source"^).
    echo.
    pause
    exit /b 1
)

choice /c YN /m "Continue?"
if errorlevel 2 exit /b 0

echo.
set "VST3_TARGET=%CommonProgramFiles%\VST3"
if not exist "%VST3_TARGET%" mkdir "%VST3_TARGET%" 2>nul
rmdir /s /q "%VST3_TARGET%\LoopFinder.vst3" 2>nul
xcopy /E /I /Y "!BUNDLE_SRC!" "%VST3_TARGET%\LoopFinder.vst3\" >nul
if errorlevel 1 (
    echo   ! Copy failed. Close your DAW and run this installer again.
    pause
    exit /b 1
)

echo   + installed %VST3_TARGET%\LoopFinder.vst3
echo.
echo Done. Open your DAW and rescan plug-ins if LoopFinder doesn't appear.
echo.
pause
exit /b 0
