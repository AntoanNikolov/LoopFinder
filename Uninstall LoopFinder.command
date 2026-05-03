#!/usr/bin/env bash
# =============================================================================
# Uninstall LoopFinder.command
#
# Double-click in Finder to remove LoopFinder from the system Audio Plug-Ins
# folders. Build artefacts in ./build/ and the JUCE/ submodule are left
# untouched (delete those manually if you want a fully clean checkout).
# =============================================================================

set -uo pipefail

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

USER_TARGETS=(
    "$HOME/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3"
    "$HOME/Library/Audio/Plug-Ins/VST/LoopFinder.vst"
    "$HOME/Library/Audio/Plug-Ins/Components/LoopFinder.component"
)

SYSTEM_TARGETS=(
    "/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3"
    "/Library/Audio/Plug-Ins/VST/LoopFinder.vst"
    "/Library/Audio/Plug-Ins/Components/LoopFinder.component"
)

clear 2>/dev/null || true
cat <<EOF
${bold}=============================================
  LoopFinder — Uninstaller
=============================================${reset}

This will remove the following bundles, if present:

EOF

found_any=0
for path in "${USER_TARGETS[@]}" "${SYSTEM_TARGETS[@]}"; do
    if [[ -e "$path" ]]; then
        echo "  • $path"
        found_any=1
    fi
done

if [[ $found_any -eq 0 ]]; then
    echo "  (no LoopFinder bundles found — nothing to uninstall)"
    echo
    pause; exit 0
fi

echo
echo "${yellow}Quit your DAW first${reset} so the bundles aren't held open."
echo
read -r -p "Continue? [y/N] " ans
if [[ "$ans" != "y" && "$ans" != "Y" ]]; then
    echo "Cancelled."
    pause; exit 0
fi

heading "Removing user bundles"
removed=0
failed=0
for path in "${USER_TARGETS[@]}"; do
    if [[ -e "$path" ]]; then
        if rm -rf "$path" 2>/dev/null && [[ ! -e "$path" ]]; then
            ok "removed $path"
            removed=$((removed + 1))
        else
            fail "could not remove $path (close your DAW and try again)"
            failed=$((failed + 1))
        fi
    fi
done

# System paths (require sudo) — only attempt if anything is actually present.
needs_sudo=0
for path in "${SYSTEM_TARGETS[@]}"; do
    [[ -e "$path" ]] && needs_sudo=1
done

if [[ $needs_sudo -eq 1 ]]; then
    heading "Removing system bundles (requires admin password)"
    for path in "${SYSTEM_TARGETS[@]}"; do
        if [[ -e "$path" ]]; then
            if sudo rm -rf "$path" 2>/dev/null && [[ ! -e "$path" ]]; then
                ok "removed $path"
                removed=$((removed + 1))
            else
                fail "could not remove $path"
                failed=$((failed + 1))
            fi
        fi
    done
fi

# Refresh AU registration so Logic / GarageBand drop the cached entry.
heading "Refreshing Audio Unit registry"
if killall -9 AudioComponentRegistrar 2>/dev/null; then
    ok "AudioComponentRegistrar restarted"
else
    note "AudioComponentRegistrar wasn't running (this is fine)"
fi

heading "Summary"
ok "Removed $removed bundle(s)"
[[ $failed -gt 0 ]] && fail "$failed bundle(s) could not be removed"

echo
echo "Open your DAW and rescan plugins to drop the cached entries."
pause
