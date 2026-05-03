#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# install.sh — copy built LoopFinder plugins into the system plugin
# directories on macOS / Linux, then run auval to validate the AU.
#
# Usage:  ./install.sh [build-dir]
#         (defaults to ./build)
# -----------------------------------------------------------------------------

set -euo pipefail

BUILD_DIR="${1:-build}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -d "$SCRIPT_DIR/$BUILD_DIR" ]]; then
    echo "Build directory not found: $BUILD_DIR"
    echo "Run 'cmake -B $BUILD_DIR && cmake --build $BUILD_DIR --config Release' first."
    exit 1
fi

OS="$(uname -s)"
echo "Detected OS: $OS"
echo

installed=()

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------
copy_bundle () {
    local src="$1" dst_dir="$2"
    if [[ -e "$src" ]]; then
        mkdir -p "$dst_dir"
        rm -rf "$dst_dir/$(basename "$src")"
        cp -R "$src" "$dst_dir/"
        installed+=("$dst_dir/$(basename "$src")")
        echo "  ✓ installed → $dst_dir/$(basename "$src")"
    fi
}

# -----------------------------------------------------------------------------
# macOS
# -----------------------------------------------------------------------------
if [[ "$OS" == "Darwin" ]]; then
    HOME_VST3="$HOME/Library/Audio/Plug-Ins/VST3"
    HOME_VST2="$HOME/Library/Audio/Plug-Ins/VST"
    HOME_AU="$HOME/Library/Audio/Plug-Ins/Components"

    echo "Installing macOS plugin bundles…"
    for fmt in VST3 VST AU; do
        case "$fmt" in
            VST3) ext="vst3";  target="$HOME_VST3" ;;
            VST)  ext="vst";   target="$HOME_VST2" ;;
            AU)   ext="component"; target="$HOME_AU" ;;
        esac
        # Look in the canonical JUCE artefact location.
        candidate="$(find "$BUILD_DIR" -type d -name "LoopFinder.${ext}" -print -quit || true)"
        if [[ -n "$candidate" ]]; then
            copy_bundle "$candidate" "$target"
        fi
    done

    echo
    echo "Validating Audio Unit with auval…"
    if command -v auval >/dev/null 2>&1; then
        # Allow auval to repopulate its cache.
        killall -9 AudioComponentRegistrar 2>/dev/null || true
        # LoopFinder is now an instrument — AU type is `aumu` (MusicDevice).
        # Manufacturer / subtype codes mirror PLUGIN_MANUFACTURER_CODE /
        # PLUGIN_CODE in CMakeLists.txt.
        if auval -v aumu Lpfd Ynme; then
            echo "  ✓ auval reports the AU as valid"
        else
            echo "  ✗ auval reported errors — see output above"
        fi
    else
        echo "  (auval not available — skipping AU validation)"
    fi

# -----------------------------------------------------------------------------
# Linux
# -----------------------------------------------------------------------------
elif [[ "$OS" == "Linux" ]]; then
    HOME_VST3="$HOME/.vst3"
    HOME_VST2="$HOME/.vst"

    echo "Installing Linux plugin bundles…"
    for fmt in VST3 VST; do
        case "$fmt" in
            VST3) ext="vst3"; target="$HOME_VST3" ;;
            VST)  ext="so";   target="$HOME_VST2" ;;
        esac
        candidate="$(find "$BUILD_DIR" -type d -name "LoopFinder.${ext}" -print -quit || true)"
        [[ -z "$candidate" ]] && \
            candidate="$(find "$BUILD_DIR" -type f -name "LoopFinder.${ext}" -print -quit || true)"
        if [[ -n "$candidate" ]]; then
            copy_bundle "$candidate" "$target"
        fi
    done
fi

echo
echo "----- Summary -----"
if [[ ${#installed[@]} -eq 0 ]]; then
    echo "No plugin bundles found in '$BUILD_DIR'. Did the build succeed?"
    exit 1
else
    printf '%s\n' "${installed[@]}"
fi
