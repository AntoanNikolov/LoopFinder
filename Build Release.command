#!/usr/bin/env bash
# =============================================================================
# Build Release.command
#
# Developer-side script. Builds LoopFinder from source and produces a
# Release/macOS/ folder containing only what an end user needs:
#
#   Release/macOS/
#     Install LoopFinder.command         (the same installer, but it'll skip
#     Uninstall LoopFinder.command        the build because the bundles are
#     LoopFinder.vst3/                    right next to it - plug-and-play, no
#     LoopFinder.component/               CMake / Xcode CLT / git required for
#                                         the person who runs it)
#
# Zip up that folder, send it to a friend, they double-click the installer
# and they're done in 5 seconds.
# =============================================================================

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

bold=$'\033[1m'
green=$'\033[32m'
red=$'\033[31m'
yellow=$'\033[33m'
dim=$'\033[2m'
reset=$'\033[0m'

heading () { printf "\n${bold}%s${reset}\n" "$*"; }
ok      () { printf "  ${green}✓${reset} %s\n" "$*"; }
warn    () { printf "  ${yellow}!${reset} %s\n" "$*"; }
fail    () { printf "  ${red}✗${reset} %s\n" "$*"; }
note    () { printf "  ${dim}%s${reset}\n" "$*"; }

pause () {
    echo
    read -r -p "Press Return to close this window…" _ || true
}

trap 'echo; fail "Release builder aborted (line $LINENO)."; pause; exit 1' ERR

clear 2>/dev/null || true
cat <<EOF
${bold}=============================================
  LoopFinder — Release Builder (macOS)
=============================================${reset}

This will build LoopFinder and assemble a redistributable folder at:

  $SCRIPT_DIR/Release/macOS/

You can then ZIP that folder and send it to anyone with a Mac. They will be
able to install LoopFinder by double-clicking "Install LoopFinder.command" —
no compilers or other tools required.

EOF

read -r -p "Continue? [Y/n] " ans
ans=${ans:-Y}
if [[ "$ans" != "Y" && "$ans" != "y" ]]; then
    echo "Cancelled."
    pause; exit 0
fi

# ---- 1. tools -------------------------------------------------------------
heading "Checking required tools"

if ! command -v cmake >/dev/null 2>&1; then
    fail "CMake is not installed (brew install cmake)."
    pause; exit 1
fi
ok "cmake $(cmake --version | head -1 | awk '{print $3}')"

if ! command -v git >/dev/null 2>&1; then
    fail "git is not installed (xcode-select --install)."
    pause; exit 1
fi
ok "git $(git --version | awk '{print $3}')"

if ! xcode-select -p >/dev/null 2>&1; then
    fail "Xcode Command Line Tools are not installed (xcode-select --install)."
    pause; exit 1
fi
ok "Xcode CLT at $(xcode-select -p)"

# ---- 2. JUCE --------------------------------------------------------------
heading "Checking for JUCE framework"
if [[ ! -f "JUCE/CMakeLists.txt" ]]; then
    note "JUCE not found — downloading JUCE 8.0.4 (~70MB, one-time)…"
    rm -rf JUCE
    git clone --depth 1 --branch 8.0.4 \
        https://github.com/juce-framework/JUCE.git JUCE
fi
ok "JUCE present at ./JUCE"

# ---- 3. configure & build -------------------------------------------------
heading "Configuring & building Release"

if [[ -f build/CMakeCache.txt ]]; then
    foreign=0
    if grep -qE '^# For build in directory: [A-Za-z]:[\\/]' build/CMakeCache.txt; then
        foreign=1
    fi
    if grep -qE '^CMAKE_GENERATOR:INTERNAL=Visual Studio' build/CMakeCache.txt; then
        foreign=1
    fi
    if [[ $foreign -eq 1 ]]; then
        note "Detected a foreign build cache — wiping ./build for a clean reconfigure…"
        rm -rf build
    fi
fi

if [[ ! -d build ]]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
fi

if ! cmake --build build --config Release --target LoopFinder_All; then
    fail "Build failed."
    pause; exit 1
fi
ok "Build succeeded"

# ---- 4. assemble Release/macOS/ -------------------------------------------
heading "Assembling release folder"

VST3_SRC="$SCRIPT_DIR/build/LoopFinder_artefacts/Release/VST3/LoopFinder.vst3"
AU_SRC="$SCRIPT_DIR/build/LoopFinder_artefacts/Release/AU/LoopFinder.component"
RELEASE_DIR="$SCRIPT_DIR/Release/macOS"

if [[ ! -f "$VST3_SRC/Contents/MacOS/LoopFinder" ]]; then
    fail "Build did not produce the expected VST3 bundle:"
    fail "  $VST3_SRC"
    pause; exit 1
fi

rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

cp -R "$VST3_SRC" "$RELEASE_DIR/"
ok "copied LoopFinder.vst3"

if [[ -f "$AU_SRC/Contents/MacOS/LoopFinder" ]]; then
    cp -R "$AU_SRC" "$RELEASE_DIR/"
    ok "copied LoopFinder.component"
else
    warn "AU bundle not found at $AU_SRC — release folder will be VST3-only."
fi

# Copy the user-facing scripts and make them executable.
cp "$SCRIPT_DIR/Install LoopFinder.command"   "$RELEASE_DIR/"
cp "$SCRIPT_DIR/Uninstall LoopFinder.command" "$RELEASE_DIR/"
chmod +x "$RELEASE_DIR/Install LoopFinder.command" \
         "$RELEASE_DIR/Uninstall LoopFinder.command"
ok "copied Install / Uninstall scripts"

cat > "$RELEASE_DIR/README.txt" <<'EOF'
LoopFinder — macOS installer
============================

To install:
  1. Double-click "Install LoopFinder.command".
     (If macOS warns "cannot be opened because it is from an unidentified
      developer", right-click the file → Open → Open. You only need to do
      this once.)
  2. Press Return / Y to confirm.

The plugin will be installed to:
  ~/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3
  ~/Library/Audio/Plug-Ins/Components/LoopFinder.component

To uninstall: double-click "Uninstall LoopFinder.command".

No compilers or other developer tools are required.
EOF
ok "wrote README.txt"

heading "Done!"
cat <<EOF
Release folder:
  $RELEASE_DIR

Next step: in Finder, right-click the folder → "Compress" to produce a
single ZIP you can send to anyone with a Mac.
EOF
pause
