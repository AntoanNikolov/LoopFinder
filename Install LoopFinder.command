#!/usr/bin/env bash
# =============================================================================
# Install LoopFinder.command
#
# Two modes:
#   1. Pre-built mode (end users)
#      If a LoopFinder.vst3 (and optionally a LoopFinder.component) bundle
#      sits next to this script, the script just copies them into
#      ~/Library/Audio/Plug-Ins/. No prerequisites required, no compilation.
#      ~5 seconds, plug-and-play.
#
#   2. Source-build mode (developers / source clone)
#      If no pre-built bundles are found, the script builds LoopFinder from
#      source. This requires CMake, git, and the Xcode Command Line Tools.
#      Takes a few minutes the first time.
#
# Plugins are installed into the per-user folders (no sudo / admin password
# required):
#   ~/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3
#   ~/Library/Audio/Plug-Ins/Components/LoopFinder.component
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

USER_VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
USER_AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

clear 2>/dev/null || true
cat <<EOF
${bold}=============================================
  LoopFinder — Installer
=============================================${reset}

This will install LoopFinder into:

  $USER_VST3_DIR/LoopFinder.vst3
  $USER_AU_DIR/LoopFinder.component

${yellow}Quit your DAW before continuing${reset} so the existing
plugin (if any) can be replaced cleanly.

EOF

read -r -p "Continue? [Y/n] " ans
ans=${ans:-Y}
if [[ "$ans" != "Y" && "$ans" != "y" ]]; then
    echo "Cancelled."
    pause; exit 0
fi

# ---- 1. pre-built bundles? ------------------------------------------------
# Anatomy of a JUCE macOS bundle:
#   LoopFinder.vst3/Contents/MacOS/LoopFinder           (executable)
#   LoopFinder.vst3/Contents/Info.plist
# We look for the inner executable as a sanity check that the bundle isn't
# truncated.
heading "Looking for pre-built bundles"

VST3_SRC=""
AU_SRC=""

if [[ -f "$SCRIPT_DIR/LoopFinder.vst3/Contents/MacOS/LoopFinder" ]]; then
    VST3_SRC="$SCRIPT_DIR/LoopFinder.vst3"
    ok "found pre-built VST3 next to this installer"
fi
if [[ -f "$SCRIPT_DIR/LoopFinder.component/Contents/MacOS/LoopFinder" ]]; then
    AU_SRC="$SCRIPT_DIR/LoopFinder.component"
    ok "found pre-built AU next to this installer"
fi

if [[ -n "$VST3_SRC" || -n "$AU_SRC" ]]; then
    note "Skipping compilation — going straight to install."
else
    note "No pre-built bundles found. Falling back to building from source"
    note "(requires CMake, git, and the Xcode Command Line Tools)."

    # ---- 2. tools (only required for source build) ------------------------
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

    # ---- 3. JUCE ----------------------------------------------------------
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

    # ---- 4. configure & build --------------------------------------------
    heading "Configuring & building (this may take a few minutes the first time)"

    # Detect a foreign / wrong-platform build cache (e.g. one that was generated
    # on Windows or with a different generator) and wipe it so we reconfigure
    # cleanly. Without this, cmake tries to use the cached build tree and fails.
    if [[ -f build/CMakeCache.txt ]]; then
        foreign=0
        if grep -qE '^# For build in directory: [A-Za-z]:[\\/]' build/CMakeCache.txt; then
            foreign=1   # Windows-style absolute path
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
        note "First-time configure — please be patient…"
        cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
    fi

    if ! cmake --build build --config Release --target LoopFinder_All; then
        fail "Build failed. Run ./Install\\ LoopFinder.command again from Terminal to see full output."
        pause; exit 1
    fi

    ok "Build succeeded"

    # Pick up the freshly-built artefacts.
    VST3_SRC="$SCRIPT_DIR/build/LoopFinder_artefacts/Release/VST3/LoopFinder.vst3"
    AU_SRC="$SCRIPT_DIR/build/LoopFinder_artefacts/Release/AU/LoopFinder.component"

    [[ -f "$VST3_SRC/Contents/MacOS/LoopFinder" ]] || VST3_SRC=""
    [[ -f "$AU_SRC/Contents/MacOS/LoopFinder"   ]] || AU_SRC=""
fi

# ---- 5. install (copy bundles into user plug-in folders) ------------------
heading "Installing plugins"

installed=0
failed=0

install_bundle () {
    local src="$1" target_dir="$2" label="$3"
    if [[ -z "$src" ]]; then
        warn "no $label bundle to install"
        return
    fi
    if [[ ! -d "$src" ]]; then
        fail "$label bundle missing or invalid: $src"
        failed=$((failed + 1))
        return
    fi

    mkdir -p "$target_dir"
    rm -rf "$target_dir/$(basename "$src")"
    if ! cp -R "$src" "$target_dir/"; then
        fail "could not copy $label bundle to $target_dir"
        failed=$((failed + 1))
        return
    fi

    # Strip the macOS quarantine attribute so Gatekeeper doesn't refuse to
    # load the (unsigned) plugin when it was downloaded from the internet.
    # This is a no-op on locally-built bundles.
    xattr -dr com.apple.quarantine "$target_dir/$(basename "$src")" 2>/dev/null || true

    ok "installed $label → $target_dir/$(basename "$src")"
    installed=$((installed + 1))
}

install_bundle "$VST3_SRC" "$USER_VST3_DIR" "VST3"
install_bundle "$AU_SRC"   "$USER_AU_DIR"   "AU"

if [[ $installed -eq 0 ]]; then
    fail "Nothing was installed."
    pause; exit 1
fi

# ---- 6. validate AU (best-effort) -----------------------------------------
if [[ -n "$AU_SRC" ]] && command -v auval >/dev/null 2>&1; then
    heading "Validating Audio Unit"
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    # LoopFinder is an instrument — AU type `aumu` (MusicDevice).
    # Manufacturer / subtype codes mirror PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE
    # in CMakeLists.txt.
    if auval -v aumu Lpfd Ynme >/dev/null 2>&1; then
        ok "auval reports the AU as valid"
    else
        warn "auval reported errors — the plugin will still appear in your DAW"
        warn "but may not show up in Logic / GarageBand until macOS re-scans."
    fi
fi

# ---- 7. done --------------------------------------------------------------
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
