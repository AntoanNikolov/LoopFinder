# LoopFinder

LoopFinder is a JUCE-based instrument plug-in. Load an audio file, run analysis, choose a suggested loop region, and play it from a MIDI keyboard (pitch-shifted per note).

## Workflow

1. Insert LoopFinder on an instrument track.
2. Drag audio onto the waveform or use **Load File**.
3. Press **Analyze** (up to eight candidate regions).
4. Select a region in the waveform or list.
5. Play MIDI to hear the chosen loop (hold or one-shot depending on **Loop**).

Formats: **VST3**, optional **VST2** (`VST2_SDK_PATH`), **AU** on macOS, plus a Standalone debug target.

## Requirements

macOS 10.13+, Windows 10+. JUCE 8.0.4+ is required for modern Apple SDK behaviour (see CMake notes).

## Install

| OS | Steps |
|---|---|
| Windows | Double-click **`Install LoopFinder.bat`**. Approve elevation (writes under `Common Files\VST3`). |
| macOS | Double-click **`Install LoopFinder.command`**. If Gatekeeper blocks it, Context menu → Open. |

Installer behaviour:

- If **`Prebuilt/…` bundles exist**, they are copied only (no compilers).
- If they are absent, Windows can offer **`winget`** installs for Git + CMake + **Visual Studio 2022 Build Tools** ( MSVC / MSBuild ), prompt you to re-run once, then build with the proper Visual Studio generator (avoids the `NMake` / missing `nmake` failure).
- On macOS, you can approve an optional Xcode Command Line Tools + CMake path; Apple’s CLT installer is interactive and cannot run fully unattended.

**Uninstall**: `Uninstall LoopFinder.bat` or `Uninstall LoopFinder.command`.

## Bundled artefacts

```
Prebuilt/Windows/LoopFinder.vst3/
Prebuilt/macOS/LoopFinder.vst3/
Prebuilt/macOS/LoopFinder.component/
```

Maintainers overwrite these folders after CMake builds (`Prebuilt/macOS/README_MAINTAINER.txt`).

## Repository layout

```
LoopFinder/
  CMakeLists.txt
  Source/           Plug-in DSP and UI
  Resources/
  Tests/
  Prebuilt/         Shipped binaries
  Install LoopFinder.*
  install.sh / install.bat
```

`JUCE/` is `.gitignored`; clone when building (below).

---

## Maintainer build

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
```

### Debug

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target LoopFinder_VST3
```

### Release (all formats your platform supports)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVST2_SDK_PATH=/path/to/vst2sdk
cmake --build build --config Release --target LoopFinder_All
```

`VST2_SDK_PATH` is optional. Omit it to skip VST2. AU builds on macOS only.

Post-build copy helpers (after building):

```bash
./install.sh build        # macOS / Linux
install.bat   build       # Windows (Administrator)
```

### Tests (`on` unless `-DLOOPFINDER_BUILD_TESTS=OFF`)

```bash
cmake --build build --target LoopDetectorTests
ctest --test-dir build --output-on-failure
```

`-DLOOPFINDER_AUTO_COPY=ON` makes CMake deploy into user/system plug-in paths (writes need permissions).

## Default install destinations

| OS | VST3 | AU | VST2 |
|---|---|---|---|
| macOS | `~/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3` | `~/Library/Audio/Plug-Ins/Components/LoopFinder.component` | `~/Library/Audio/Plug-Ins/VST/LoopFinder.vst` |
| Windows | `%CommonProgramFiles%\VST3\LoopFinder.vst3` | n/a | `%CommonProgramFiles%\VST2\LoopFinder.dll` |

## macOS code signing

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_CODESIGN=ON \
      -DCODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
```

Empty identity ad-hoc signs (local testing). Public distribution expects Developer ID and notarisation. Entitlements template: `Resources/entitlements.plist`. Unsigned binaries may require a one-time **Open** in Finder for downloaded copies.

---

## Compatibility notes

**VST3**: Instrument / sampler category; validated with the official tooling. Checked in Ableton Live 11+, FL Studio 21+, Cubase 12+, Reaper 6+.

**VST2**: Legacy SDK builds only (`VST2_SDK_PATH`). Stereo outs, no audio input bus.

**AU**: Instrument (`aumu Lpfd Ynme`). Tested Logic 10.7+ / GarageBand. Change `com.yourname.loopfinder` before wide release.

## MIDI engine

Sixteen voices, root-note pitch shifting `2^((noteNumber - rootNote) / 12)`, 5 ms attack / 50 ms release, Preview / Stop trigger a synthetic note at the root. **Loop** toggles looping vs one-shot. Voice stealing favors oldest releasing first.

---

## State save format

Saved state packs APVTS, optional embedded WAV (≤ ~5 MB) or filesystem path reference, detected regions with wrap metadata, selected index. Broken paths trigger the **Linked file is missing — please re-load** warning.

---

## Planned work

780×480 fixed window; optional waveform hover parity with list selection; Rubber Band pitch/time hooks.
