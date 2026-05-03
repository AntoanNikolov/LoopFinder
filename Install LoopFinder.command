#!/usr/bin/env bash
# =============================================================================
# Install LoopFinder.command
#
# Double-click this file in Finder to build (if needed) and install the
# LoopFinder plugin into your system Audio Plug-Ins folders.
#
# What it does:
#   1. Checks for required tools (CMake, git, Xcode CLT)
#   2. Downloads JUCE if it isn't already present
#   3. Builds LoopFinder (incremental — only rebuilds changed files)
#   4. Copies the AU and VST3 bundles to ~/Library/Audio/Plug-Ins/...
#   5. Validates the AU
# =============================================================================

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---- pretty output --------------------------------------------------------
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

trap 'echo; fail "Installer aborted (line $LINENO)."; pause; exit 1' ERR

clear 2>/dev/null || true
cat <<EOF
${bold}=============================================
  LoopFinder — Installer
=============================================${reset}

This will build and install LoopFinder into your
user Audio Plug-Ins directories:

  ~/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3
  ~/Library/Audio/Plug-Ins/Components/LoopFinder.component

${yellow}Quit your DAW before continuing${reset} so the existing
plugin (if any) can be replaced cleanly.

EOF

read -r -p "Continue? [Y/n] " ans
ans=${ans:-Y}
if [[ "$ans" != "Y" && "$ans" != "y" ]]; then
    echo "Cancelled."
    pause; exit 0
fi

# ---- 1. tools ----------------------------------------------------------------
heading "Checking required tools"

if ! command -v cmake >/dev/null 2>&1; then
    fail "CMake is not installed."
    note  "Install it from https://cmake.org/download/ or via Homebrew:"
    note  "    brew install cmake"
    pause; exit 1
fi
ok "cmake $(cmake --version | head -1 | awk '{print $3}')"

if ! command -v git >/dev/null 2>&1; then
    fail "git is not installed (needed for downloading JUCE)."
    note  "Install Xcode Command Line Tools: xcode-select --install"
    pause; exit 1
fi
ok "git $(git --version | awk '{print $3}')"

if ! xcode-select -p >/dev/null 2>&1; then
    fail "Xcode Command Line Tools are not installed."
    note  "Run: xcode-select --install"
    pause; exit 1
fi
ok "Xcode CLT at $(xcode-select -p)"

# ---- 2. JUCE -----------------------------------------------------------------
heading "Checking for JUCE framework"

if [[ ! -f "JUCE/CMakeLists.txt" ]]; then
    note "JUCE not found — downloading JUCE 8.0.4 (~70MB, one-time)…"
    rm -rf JUCE
    if ! git clone --depth 1 --branch 8.0.4 \
            https://github.com/juce-framework/JUCE.git JUCE; then
        fail "Failed to download JUCE. Check your internet connection."
        pause; exit 1
    fi
fi
ok "JUCE present at ./JUCE"

# ---- 3. configure & build ----------------------------------------------------
heading "Configuring & building (this may take a few minutes the first time)"

if [[ ! -d build ]]; then
    note "First-time configure — please be patient…"
    cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
fi

if ! cmake --build build --config Release --target LoopFinder_All; then
    fail "Build failed. Run ./Install\\ LoopFinder.command again from Terminal to see full output."
    pause; exit 1
fi

ok "Build succeeded"

# ---- 4. install --------------------------------------------------------------
heading "Installing plugins"

if [[ ! -x ./install.sh ]]; then
    chmod +x ./install.sh 2>/dev/null || true
fi

if ! ./install.sh build; then
    fail "Install step reported errors — see output above."
    pause; exit 1
fi

# ---- 5. done ------------------------------------------------------------------
heading "All done!"
cat <<EOF

${green}LoopFinder is installed.${reset}

Open your DAW and rescan plugins:

  Ableton Live   →  Preferences ▸ Plug-Ins ▸ Rescan
  Reaper         →  Preferences ▸ Plug-ins ▸ VST ▸ Re-scan
  Cubase         →  Studio ▸ VST Plug-In Manager ▸ refresh
  FL Studio      →  Options ▸ Manage plugins ▸ Find more
  Logic / GarageBand → AUs are detected automatically

LoopFinder will appear in the ${bold}Instruments${reset} section of
your plugin browser, under the vendor folder ${bold}LoopFinder${reset}.

EOF
pause
