#!/usr/bin/env bash
# =============================================================================
# Install LoopFinder.command  (macOS)
#
# Double-click in Finder. Copies the bundled plug-ins into your user
# plug-in folders — no admin password, no compilers, no downloads.
#
#   VST3 → ~/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3
#   AU   → ~/Library/Audio/Plug-Ins/Components/LoopFinder.component
# =============================================================================

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"

pause () {
    echo
    read -r -p "Press Return to close this window..." _ || true
}

# Locate the bundles: prefer Prebuilt/, fall back to a local CMake build.
find_bundle () {
    for candidate in \
        "$SCRIPT_DIR/Prebuilt/macOS/$1" \
        "$SCRIPT_DIR/$1" \
        "$SCRIPT_DIR/build/LoopFinder_artefacts/Release/$2/$1"
    do
        if [[ -f "$candidate/Contents/MacOS/LoopFinder" ]]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

VST3_SRC="$(find_bundle "LoopFinder.vst3" "VST3" || true)"
AU_SRC="$(find_bundle "LoopFinder.component" "AU" || true)"

clear 2>/dev/null || true
cat <<EOF
=============================================
  LoopFinder — Installer (macOS)
=============================================

This will install:

  VST3 → $VST3_DEST/LoopFinder.vst3
  AU   → $AU_DEST/LoopFinder.component

Quit your DAW first so open plug-ins can be replaced.

EOF

if [[ -z "$VST3_SRC" && -z "$AU_SRC" ]]; then
    echo "error: no LoopFinder plug-in found in this folder."
    echo
    echo "  Expected: Prebuilt/macOS/LoopFinder.vst3"
    echo "  Download the latest macOS release from the project page, or build"
    echo "  from source first (see README.md, 'Building from source')."
    pause
    exit 1
fi

read -r -p "Continue? [Y/n] " ans
ans=${ans:-Y}
if [[ "$ans" != "Y" && "$ans" != "y" ]]; then
    pause; exit 0
fi

echo
failed=0
install_bundle () {
    local src="$1" dest="$2"
    local name; name="$(basename "$src")"
    mkdir -p "$dest"
    rm -rf "${dest:?}/$name"
    if cp -R "$src" "$dest/"; then
        # Clear quarantine so Gatekeeper doesn't block the freshly copied bundle.
        xattr -dr com.apple.quarantine "$dest/$name" 2>/dev/null || true
        echo "  + installed $dest/$name"
    else
        echo "  ! copy failed for $name (close your DAW and retry)"
        failed=1
    fi
}

[[ -n "$VST3_SRC" ]] && install_bundle "$VST3_SRC" "$VST3_DEST"
[[ -n "$AU_SRC"   ]] && install_bundle "$AU_SRC"   "$AU_DEST"

# Nudge the AU registry so Logic / GarageBand pick up the new component.
if [[ -n "$AU_SRC" ]]; then
    killall -9 AudioComponentRegistrar 2>/dev/null || true
fi

echo
if [[ "$failed" -eq 0 ]]; then
    echo "Done. Open your DAW and rescan plug-ins if LoopFinder doesn't appear."
else
    echo "Finished with errors — see messages above."
fi
pause
exit $failed
