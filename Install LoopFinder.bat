@echo off
REM Install LoopFinder (Windows): copies Prebuilt\VST bundle, OR builds from
REM source after optionally installing toolchain via winget.

setlocal enabledelayedexpansion
cd /d "%~dp0"

REM ---- Elevate (writes to Common Files\VST3) -------------------------------
net session >nul 2>&1
if errorlevel 1 (
    echo.
    echo Installing to %CommonProgramFiles%\VST3 requires administrator rights.
    echo Opening an elevated prompt. Accept User Account Control to continue...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs" >nul 2>&1
    if errorlevel 1 (
        echo.
        echo   Could not elevate. Right-click this file, choose Run as administrator.
        pause
    )
    exit /b 0
)

set "EXIT_CODE=0"
set "BUNDLE_SRC="
set "BUNDLE_FROM_BUILD=0"

echo.
echo ===========================================
echo   LoopFinder - Installer
echo ===========================================
echo Install target:
echo   %CommonProgramFiles%\VST3\LoopFinder.vst3
echo.
echo Close any DAW that may lock the existing plug-in.
echo.
choice /c YN /m "Continue?"
if errorlevel 2 (
    set "EXIT_CODE=0"
    goto :eof_footer
)

REM ---- Locate pre-built bundle ----------------------------------------------
echo.
echo === Locating bundled VST3 ===
if exist "%~dp0Prebuilt\Windows\LoopFinder.vst3\Contents\x86_64-win\LoopFinder.vst3" (
    set "BUNDLE_SRC=%~dp0Prebuilt\Windows\LoopFinder.vst3"
)
if not defined BUNDLE_SRC (
    if exist "%~dp0LoopFinder.vst3\Contents\x86_64-win\LoopFinder.vst3" (
        set "BUNDLE_SRC=%~dp0LoopFinder.vst3"
    )
)

if defined BUNDLE_SRC (
    echo   Using: !BUNDLE_SRC!
    goto :install_bundle
)

echo   Bundled LoopFinder.vst3 not found. Build from source instead.
goto :needs_buildchain

REM --------------------------------------------------------------------------
:needs_buildchain
call :detect_vs_generator
call :detect_cmake_exe
call :detect_git_exe

if defined CMAKE_EXE if defined GIT_EXE if defined VS_GENERATOR (
    echo   CMake, Git, and Visual Studio toolchain detected.
    goto :do_source_build
)

echo.
echo === Optional: install toolchain (winget^) =================================
echo   Git, CMake, and Visual Studio 2022 C++ Build Tools can be installed
echo   automatically. Download size is large (multiple GB).
echo.
choice /c YN /m "Install these with winget, then rerun this installer?"
if errorlevel 2 (
    echo.
    echo   Manual option: Visual Studio Build Tools ^+ CMake ^+ Git, then rerun.
    echo   https://visualstudio.microsoft.com/visual-cpp-build-tools/
    set "EXIT_CODE=1"
    goto :eof_footer
)

where winget >nul 2>&1
if errorlevel 1 (
    echo   winget was not found. Install "App Installer" from the Microsoft Store
    echo   ^(updates Windows Package Manager^), then run this installer again.
    set "EXIT_CODE=1"
    goto :eof_footer
)

echo.
echo Installing Git...
winget install -e --id Git.Git --source winget --accept-package-agreements --accept-source-agreements
echo Installing CMake...
winget install -e --id Kitware.CMake --source winget --accept-package-agreements --accept-source-agreements
echo Installing Visual Studio 2022 Build Tools ^( C++ workload, may take several minutes^)...
winget install -e --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

echo.
echo   Toolchain install attempted. Close this window, open a NEW copy of this
echo   installer, and choose Continue again so PATH and MSVC are detected.
set "EXIT_CODE=0"
goto :eof_footer

REM --------------------------------------------------------------------------
:do_source_build
REM ---- Ensure JUCE ----------------------------------------------------------
echo.
echo === JUCE ================================================================
if exist "JUCE\CMakeLists.txt" (
    echo   JUCE present.
) else (
    echo   Cloning JUCE 8.0.4...
    if exist JUCE rmdir /s /q JUCE
    "!GIT_EXE!" clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
    if errorlevel 1 (
        echo   Git clone failed. Check internet and try again.
        set "EXIT_CODE=1"
        goto :eof_footer
    )
)

REM Foreign / stale CMake cache ------------------------------------------------
if exist "build\CMakeCache.txt" (
    set "WIPE_BUILD=0"
    findstr /b /c:"# For build in directory: /" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=Unix Makefiles" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=NMake Makefiles" "build\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 set "WIPE_BUILD=1"
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=!VS_GENERATOR!" "build\CMakeCache.txt" >nul 2>&1
    if errorlevel 1 (
        findstr /b /c:"CMAKE_GENERATOR:INTERNAL=Visual Studio" "build\CMakeCache.txt" >nul 2>&1
        if not errorlevel 1 set "WIPE_BUILD=1"
    )
    findstr /b /c:"CMAKE_GENERATOR_PLATFORM:INTERNAL=x64" "build\CMakeCache.txt" >nul 2>&1
    if errorlevel 1 set "WIPE_BUILD=1"
    if "!WIPE_BUILD!"=="1" (
        echo   Removing incompatible build folder...
        rmdir /s /q build
    )
)

set "CMAKE_GENERATOR="
echo.
echo === Configuring and building ============================================
echo   Generator: !VS_GENERATOR! (x64)

"!CMAKE_EXE!" -B build -G "!VS_GENERATOR!" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo   CMake configure failed. Open Visual Studio Installer, modify Build Tools,
    echo   ensure Desktop development with C++/MSVC workload is checked, rerun.
    set "EXIT_CODE=1"
    goto :eof_footer
)
"!CMAKE_EXE!" --build build --config Release --target LoopFinder_All
if errorlevel 1 (
    echo   Build failed. See messages above.
    set "EXIT_CODE=1"
    goto :eof_footer
)

set "BUNDLE_SRC=%~dp0build\LoopFinder_artefacts\Release\VST3\LoopFinder.vst3"
set "BUNDLE_FROM_BUILD=1"

if not exist "!BUNDLE_SRC!\Contents\x86_64-win\LoopFinder.vst3" (
    echo   Expected VST3 bundle missing after build.
    set "EXIT_CODE=1"
    goto :eof_footer
)

echo   Build OK: !BUNDLE_SRC!

REM --------------------------------------------------------------------------
:install_bundle
echo.
echo === Installing ==========================================================

set "VST3_TARGET=%CommonProgramFiles%\VST3"
if not exist "%VST3_TARGET%" mkdir "%VST3_TARGET%" 2>nul
rmdir /s /q "%VST3_TARGET%\LoopFinder.vst3" 2>nul
xcopy /E /I /Y "!BUNDLE_SRC!" "%VST3_TARGET%\LoopFinder.vst3\" >nul
if errorlevel 1 (
    echo   Copy failed to %VST3_TARGET%. Close your DAW and retry.
    set "EXIT_CODE=1"
    goto :eof_footer
)
echo   Installed %VST3_TARGET%\LoopFinder.vst3
if "!BUNDLE_FROM_BUILD!"=="1" (
    echo   Tip: run Build then copy artefacts into Prebuilt\Windows\ so the next install is copy-only.
)

echo.
echo ===========================================
echo   Done.
echo ===========================================
echo Rescan plug-ins in your host.

:eof_footer
echo.
echo Press any key to close...
pause >nul
endlocal
exit /b %EXIT_CODE%

:detect_vs_generator
set "VS_GENERATOR="
set "VS_MAJOR="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
    for /f "usebackq tokens=1 delims=." %%v in (`"!VSWHERE!" -latest -prerelease -all -products * -property installationVersion 2^>nul`) do set "VS_MAJOR=%%v"
)
if "!VS_MAJOR!"=="18" set "VS_GENERATOR=Visual Studio 18 2026"
if "!VS_MAJOR!"=="17" set "VS_GENERATOR=Visual Studio 17 2022"
if "!VS_MAJOR!"=="16" set "VS_GENERATOR=Visual Studio 16 2019"
if "!VS_MAJOR!"=="15" set "VS_GENERATOR=Visual Studio 15 2017"

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
exit /b 0

:detect_cmake_exe
set "CMAKE_EXE="
for /f "delims=" %%C in ('where cmake 2^>nul') do (
    set "CMAKE_EXE=%%C"
    goto :cmake_exe_done
)
:cmake_exe_done
if not defined CMAKE_EXE if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
if not defined CMAKE_EXE if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles(x86)%\CMake\bin\cmake.exe"
exit /b 0

:detect_git_exe
set "GIT_EXE="
for /f "delims=" %%G in ('where git 2^>nul') do (
    set "GIT_EXE=%%G"
    goto :git_exe_done
)
:git_exe_done
if not defined GIT_EXE if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\bin\git.exe" set "GIT_EXE=%ProgramFiles%\Git\bin\git.exe"
exit /b 0
