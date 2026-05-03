@echo off
REM ===========================================================================
REM Uninstall LoopFinder.bat
REM
REM Removes LoopFinder from the standard Windows VST3 / VST2 directories.
REM Run as Administrator the first time so it can write to %CommonProgramFiles%.
REM ===========================================================================

setlocal enabledelayedexpansion

set "VST3_PATH=%CommonProgramFiles%\VST3\LoopFinder.vst3"
set "VST2_PATH=%CommonProgramFiles%\VST2\LoopFinder.dll"

echo.
echo ===========================================
echo   LoopFinder - Uninstaller
echo ===========================================
echo.
echo This will remove the following bundles, if present:
echo.

set FOUND=0
if exist "%VST3_PATH%"  ( echo   * %VST3_PATH%  & set FOUND=1 )
if exist "%VST2_PATH%"  ( echo   * %VST2_PATH%  & set FOUND=1 )

if "%FOUND%"=="0" (
    echo   (no LoopFinder bundles found - nothing to uninstall^)
    echo.
    pause
    exit /b 0
)

echo.
echo Quit your DAW first so the bundles aren't held open.
echo.
choice /c YN /m "Continue?"
if errorlevel 2 (
    echo Cancelled.
    pause
    exit /b 0
)

set REMOVED=0
set FAILED=0

if exist "%VST3_PATH%" (
    rmdir /s /q "%VST3_PATH%" 2>nul
    if exist "%VST3_PATH%" (
        echo   ! could not remove %VST3_PATH% - try running as Administrator
        set /a FAILED+=1
    ) else (
        echo   * removed %VST3_PATH%
        set /a REMOVED+=1
    )
)

if exist "%VST2_PATH%" (
    del /q "%VST2_PATH%" 2>nul
    if exist "%VST2_PATH%" (
        echo   ! could not remove %VST2_PATH% - try running as Administrator
        set /a FAILED+=1
    ) else (
        echo   * removed %VST2_PATH%
        set /a REMOVED+=1
    )
)

echo.
echo ===========================================
echo Removed %REMOVED% bundle(s).
if not "%FAILED%"=="0" echo %FAILED% bundle(s) could not be removed.
echo ===========================================
echo.
echo Open your DAW and rescan plugins to drop cached entries.
echo.
pause
