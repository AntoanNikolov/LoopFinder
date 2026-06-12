# LoopFinder

LoopFinder is a sampler-style instrument plug-in (VST3 / AU). Load a sample — an 808, a pad, anything sustained — and it finds seamless loop points automatically, detects the sample's key, and can retune it to C in one click. Pick a loop, then play it from MIDI with per-note pitch shifting.

## Install

### Windows

1. Download / clone this repository.
2. Double-click **`Install LoopFinder.bat`** and accept the admin prompt.
3. Rescan plug-ins in your DAW.

Installs to `C:\Program Files\Common Files\VST3\LoopFinder.vst3`.

### macOS

1. Download / clone this repository.
2. Double-click **`Install LoopFinder.command`**.
   (If Gatekeeper blocks it: right-click → **Open** → **Open**.)
3. Rescan plug-ins in your DAW.

Installs to your user library — no admin password needed:

- `~/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3`
- `~/Library/Audio/Plug-Ins/Components/LoopFinder.component` (AU, for Logic/GarageBand)

The installers simply copy the bundles from `Prebuilt/` — no compilers, no downloads.

### Uninstall

Double-click `Uninstall LoopFinder.bat` (Windows) or `Uninstall LoopFinder.command` (macOS).

## How to use

1. Insert LoopFinder on an instrument track.
2. Drag an audio file onto the waveform (WAV, AIFF, FLAC, MP3) or click **Load File**. The **KEY** badge in the header shows the detected fundamental (e.g. `F1 +23` = F1, 23 cents sharp). Click **Tune to C** to snap the sample onto the nearest C; the **Tune** slider (in cents, double-click to reset) is there for manual or fine adjustment.
3. *(Optional but recommended for 808s)* **Drag across the waveform to highlight where the loop should live.** Looping early in the sample keeps the level hot, so a held note doesn't dip in volume the way a late, decayed loop does. Drag the amber handles to adjust the highlight; click empty waveform space to clear it.
4. Click **Analyze** (it reads **Analyze Area** while a highlight is active). Up to eight candidate loops appear, ranked by how seamless they are. If no clean loop exists inside your highlight, LoopFinder tells you — widen the highlight or pick another section.
5. Click a loop in the waveform or in the **Detected Loops** list to select it. Double-click a loop to audition it. Drag a loop's edges to fine-tune (snaps to zero-crossings).
6. Play MIDI (or the on-screen keyboard). **Root** sets which key plays at native pitch; **Loop** toggles looping vs one-shot; **From start** replays the sample's attack before looping.

Other waveform controls: mouse-wheel to zoom, drag the overview strip (bottom) to navigate, Alt-drag to pan.

## Building from source

Requires CMake 3.22+ and a C++17 toolchain (Xcode CLT on macOS, Visual Studio 2022 on Windows).

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target LoopFinder_All
```

Bundles land in `build/LoopFinder_artefacts/Release/`. The install scripts pick them up from there automatically if `Prebuilt/` is empty.

Optional flags:

| Flag | Purpose |
|---|---|
| `-DVST2_SDK_PATH=/path/to/vst2sdk` | Also build the legacy VST2 format |
| `-DLOOPFINDER_AUTO_COPY=ON` | Copy bundles into your plug-in folders after every build |
| `-DENABLE_CODESIGN=ON -DCODESIGN_IDENTITY="Developer ID Application: …"` | Codesign the macOS bundles (ad-hoc if identity is empty) |

### Tests

```bash
cmake --build build --target LoopDetectorTests
ctest --test-dir build --output-on-failure
```

### Continuous integration

`.github/workflows/build.yml` builds and tests on macOS and Windows for every push, and uploads ready-to-ship `LoopFinder-macOS` / `LoopFinder-Windows` artifacts. Maintainers: copy those into `Prebuilt/` when cutting a release (see `Prebuilt/README_MAINTAINER.txt`).

## How it works

- **Loop detection** — finds rising zero-crossings, pairs them within a 50 ms–4 s loop-length window, scores each pair with normalised cross-correlation, and returns the top eight de-duplicated candidates. When you highlight a search area, only crossings inside it are considered.
- **Key detection** — McLeod-style normalised autocorrelation (NSDF) over a window placed just after the attack transient, reliable down to 25 Hz sub-bass. Runs automatically on every file load.
- **Playback** — 16-voice sampler; pitch shift is `2^((note − root) / 12)` plus a global tune in cents; 5 ms attack / 50 ms release; loops with a borderline score get a 2 ms crossfade at the wrap point to mask the splice.
- **State** — the DAW project stores parameters, the loaded file (embedded if ≤ 5 MB, otherwise a path), detected loops and your selection.

## Compatibility

macOS 10.13+ (universal: Apple Silicon + Intel), Windows 10+. Checked in Ableton Live, FL Studio, Cubase, Reaper (VST3) and Logic/GarageBand (AU).
