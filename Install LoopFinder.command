#!/usr/bin/env bash
# Installs bundled plug-ins under Prebuilt/macOS/, or builds from source if
# those bundles are missing (requires Xcode Command Line Tools and CMake).

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

USER_VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
USER_AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
PRE_VST="$SCRIPT_DIR/Prebuilt/macOS/LoopFinder.vst3"
PRE_AU="$SCRIPT_DIR/Prebuilt/macOS/LoopFinder.component"
ALT_VST="$SCRIPT_DIR/LoopFinder.vst3"
ALT_AU="$SCRIPT_DIR/LoopFinder.component"

heading () { printf "\n%s\n" "$*"; }
fail () { echo "error: $*"; pause; exit 1; }

pause () {
    echo
    read -r -p "Press Return to close..." _ || true
}

ensure_cmake () {
    if command -v cmake >/dev/null 2>&1; then
        return 0
    fi
    if command -v brew >/dev/null 2>&1; then
        echo "CMake not found. Installing with Homebrew..."
        brew install cmake
    fi
    command -v cmake >/dev/null 2>&1 || fail "CMake is required. Install from https://cmake.org/download or run: brew install cmake"
}

ensure_git () {
    command -v git >/dev/null 2>&1 || fail "Git is required. Install Xcode Command Line Tools (below) includes git."
}

build_from_source_mac () {
    heading "Configure and build"

    xcode-select -p >/dev/null 2>&1 || fail \
        "Install Xcode Command Line Tools: sudo xcode-select --install. When Apple reports success, reopen Terminal and run this installer again."

    ensure_git
    ensure_cmake

    if [[ ! -f "JUCE/CMakeLists.txt" ]]; then
        echo "Cloning JUCE 8.0.4..."
        rm -rf JUCE
        git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE \
            || fail "Could not clone JUCE (network?)."
    fi

    if [[ -f build/CMakeCache.txt ]]; then
        if grep -qE '^# For build in directory: [A-Za-z]:[/\\]' build/CMakeCache.txt \
            || grep -q '^CMAKE_GENERATOR:INTERNAL=Visual Studio' build/CMakeCache.txt; then
            echo "Removing incompatible build cache."
            rm -rf build
        fi
    fi

    cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null \
        || fail "CMake configure failed."
    cmake --build build --config Release --target LoopFinder_All \
        || fail "Build failed."
}

resolve_sources () {
    VST3_SRC=""
    AU_SRC=""

    if [[ -f "$PRE_VST/Contents/MacOS/LoopFinder" ]]; then
        VST3_SRC="$PRE_VST"
    elif [[ -f "$ALT_VST/Contents/MacOS/LoopFinder" ]]; then
        VST3_SRC="$ALT_VST"
    fi

    if [[ -f "$PRE_AU/Contents/MacOS/LoopFinder" ]]; then
        AU_SRC="$PRE_AU"
    elif [[ -f "$ALT_AU/Contents/MacOS/LoopFinder" ]]; then
        AU_SRC="$ALT_AU"
    fi

    if [[ -z "$VST3_SRC" ]]; then
        heading "Bundled plug-in not found"
        echo "This repository checkout has no LoopFinder.vst3 under Prebuilt/macOS/."
        read -r -p "Download tools and compile now? This needs Xcode CLI tools and CMake. [y/N] " ans
        if [[ "$ans" != "y" && "$ans" != "Y" ]]; then
            echo "Stopped. Ask the maintainer to add Prebuilt/macOS or build locally via CMake."
            pause; exit 1
        fi
        build_from_source_mac

        cand_vst="$SCRIPT_DIR/build/LoopFinder_artefacts/Release/VST3/LoopFinder.vst3"
        cand_au="$SCRIPT_DIR/build/LoopFinder_artefacts/Release/AU/LoopFinder.component"
        [[ -f "$cand_vst/Contents/MacOS/LoopFinder" ]] && VST3_SRC="$cand_vst"
        [[ -f "$cand_au/Contents/MacOS/LoopFinder" ]] && AU_SRC="$cand_au"

        [[ -n "$VST3_SRC" ]] || fail "Build finished but LoopFinder.vst3 was not found."
    fi
}

clear 2>/dev/null || true
cat <<EOF
=============================================
LoopFinder Installer (macOS)
=============================================

Installation targets:

  ${USER_VST3_DIR}/LoopFinder.vst3
  ${USER_AU_DIR}/LoopFinder.component  (if present in this repo)

Close your DAW first so locked plug-ins can be replaced.

EOF

read -r -p "Continue? [Y/n] " ans
ans=${ans:-Y}
if [[ "$ans" != "Y" && "$ans" != "y" ]]; then
    pause; exit 0
fi

resolve_sources

failed=0
install_bundle () {
    local src="$1"
    local dest_parent="$2"
    local nm
    nm="$(basename "$src")"
    mkdir -p "$dest_parent"
    rm -rf "$dest_parent/$nm"
    cp -R "$src" "$dest_parent/" || { echo "copy failed for $nm"; failed=1; return 1; }
    xattr -dr com.apple.quarantine "$dest_parent/$nm" 2>/dev/null || true
}

heading "Installing"
install_bundle "$VST3_SRC" "$USER_VST3_DIR" || true
[[ -n "$AU_SRC" ]] && install_bundle "$AU_SRC" "$USER_AU_DIR" || true
[[ "$failed" -eq 1 ]] && { pause; exit 1; }

if [[ -n "$AU_SRC" ]] && command -v auval >/dev/null 2>&1; then
    heading "Validating AU"
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    auval -v aumu Lpfd Ynme >/dev/null 2>&1 \
        || echo "note: auval reported warnings; try a plug-in rescan."
fi

heading "Finished"
echo "Plug-ins copied. Use your host's scanner if they do not appear."
pause
