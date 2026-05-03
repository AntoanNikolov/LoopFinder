# LoopFinder

A sampler-style audio plugin that automatically detects regions in any
loaded audio file that can loop seamlessly, then lets you **play those
loops on a MIDI keyboard**. Designed primarily for finding perfect loop
points in 808 bass samples, but works on any sustained audio (pads,
strings, synths, vocal tones).

Workflow:

1. Open the plugin on an instrument track.
2. Drag an audio file onto the waveform display (or click **Load File**).
3. Click **Analyze** — the plugin proposes up to eight seamless loop regions.
4. Click a region in the waveform or the side panel to select it.
5. Play any MIDI note (external keyboard, on-screen keyboard, or DAW MIDI
   clip) — the selected loop plays pitched to that note for as long as the
   key is held, looping seamlessly.

Ships as **VST3**, **VST2** (when an SDK path is supplied) and **AU**
(macOS), plus a Standalone application target for development and testing.
On macOS the AU registers as an `aumu` MusicDevice (instrument).

---

## Tech

| Component  | Choice                                                              |
|------------|---------------------------------------------------------------------|
| Framework  | JUCE 8.x (≥ 8.0.4 — required on macOS 15+ Xcode/SDK)                |
| Language   | C++17                                                               |
| DSP        | Pure-C++ autocorrelation detector                                   |
| Build      | CMake 3.22+                                                         |
| UI         | JUCE native components                                              |

> **Why JUCE 8?** JUCE 7.0.12 (the last 7.x release) still uses
> `CGWindowListCreateImage`, which Apple removed in macOS SDK 15. JUCE 8.x
> replaces it with ScreenCaptureKit and is the only line that builds on
> current Xcode. Older macOS SDKs and JUCE 7 should still work fine.

Minimum OS: macOS 10.13+, Windows 10+.

---

## Repository layout

```
LoopFinder/
├── CMakeLists.txt
├── Source/
│   ├── PluginProcessor.{h,cpp}
│   ├── PluginEditor.{h,cpp}
│   ├── LoopDetector.{h,cpp}    ← pure C++17, zero JUCE deps
│   ├── LoopRegion.h
│   ├── AudioFileManager.{h,cpp}
│   ├── PlaybackEngine.{h,cpp}
│   ├── WaveformDisplay.{h,cpp}
│   ├── RegionListPanel.{h,cpp}
│   ├── LoopExporter.{h,cpp}
│   └── Theme.h
├── Resources/
│   └── entitlements.plist        ← used when ENABLE_CODESIGN=ON (macOS)
├── Tests/
│   ├── CMakeLists.txt
│   └── LoopDetectorTests.cpp     ← JUCE UnitTest framework
├── Install LoopFinder.command    ← double-click on macOS
├── Uninstall LoopFinder.command
├── Install LoopFinder.bat        ← double-click on Windows
├── Uninstall LoopFinder.bat
├── install.sh    install.bat     ← lower-level scripts called by the above
├── .gitignore
└── JUCE/                         ← git submodule (auto-cloned by installer)
```

---

## Quick install (recommended)

You don't need to touch the terminal — just double-click the installer for
your platform:

| Platform | Installer                       | Uninstaller                       |
|----------|---------------------------------|-----------------------------------|
| macOS    | `Install LoopFinder.command`    | `Uninstall LoopFinder.command`    |
| Windows  | `Install LoopFinder.bat`        | `Uninstall LoopFinder.bat`        |

The installer will:

1. Verify CMake / git / Xcode CLT are present (Visual Studio on Windows).
2. Download JUCE 8.0.4 the first time (~70 MB, one-time).
3. Build LoopFinder (incremental — only changed files on subsequent runs).
4. Copy the AU and VST3 bundles to your user plug-in folders.
5. Run `auval` on macOS to confirm the AU is valid.

> **macOS Gatekeeper note:** the first time you double-click `.command` files
> downloaded from the internet, macOS may show a "cannot be opened because the
> developer cannot be verified" dialog. Right-click the file → **Open** to
> grant a one-time exception, or remove the quarantine flag via:
> `xattr -d com.apple.quarantine "Install LoopFinder.command"`.
>
> **Windows note:** the first install may need to be run as Administrator so
> the script can write to `C:\Program Files\Common Files\VST3`.

---

## Manual setup (advanced)

If you'd rather drive the build yourself, clone JUCE into the project directory
and use CMake directly:

```bash
cd LoopFinder
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
```

Then build as described below.

---

## Build

### Debug

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target LoopFinder_VST3
```

### Release (all formats)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
              -DVST2_SDK_PATH=/path/to/vst2sdk
cmake --build build --config Release --target LoopFinder_All
```

`VST2_SDK_PATH` is **optional** — leave it unset (or invalid) to skip the VST2
build. AU is built automatically on macOS only.

By default the build does **not** copy plugins into the system plugin
directories — use `install.sh` / `install.bat` for that, or pass
`-DLOOPFINDER_AUTO_COPY=ON` at configure time to have CMake do it as a
post-build step (requires write access to `~/Library/Audio/Plug-Ins/...` on
macOS or `C:\Program Files\Common Files\VST3` on Windows).

### Tests

Tests are enabled by default (`-DLOOPFINDER_BUILD_TESTS=OFF` to disable):

```bash
cmake --build build --target LoopDetectorTests
ctest --test-dir build --output-on-failure
```

---

## Install (manual)

For most users, the [Quick install](#quick-install-recommended) section above
is all you need. If you'd rather call the lower-level scripts directly after
building, run:

```bash
./install.sh build         # macOS / Linux
install.bat   build        # Windows (run as Administrator)
```

On macOS the script also runs `auval` to validate the AU.

To uninstall manually, use the `Uninstall LoopFinder.command` /
`Uninstall LoopFinder.bat` scripts in the project root, or simply delete the
bundles from the locations in the table below.

Default install locations:

| Platform | VST3                                                | AU                                                   | VST2                                |
|----------|-----------------------------------------------------|------------------------------------------------------|-------------------------------------|
| macOS    | `~/Library/Audio/Plug-Ins/VST3/LoopFinder.vst3`     | `~/Library/Audio/Plug-Ins/Components/LoopFinder.component` | `~/Library/Audio/Plug-Ins/VST/LoopFinder.vst` |
| Windows  | `C:\Program Files\Common Files\VST3\LoopFinder.vst3` | n/a                                                | `C:\Program Files\Common Files\VST2\LoopFinder.dll` |

---

## Code signing (macOS)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
              -DENABLE_CODESIGN=ON \
              -DCODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
```

When `ENABLE_CODESIGN=ON` and `CODESIGN_IDENTITY` is empty, **ad-hoc** signing
is used — fine for local testing, not for distribution.

For distribution you need an Apple Developer ID and a notarised build. The
post-build step uses the entitlements file at `Resources/entitlements.plist`
(audio-input + library-validation disable).

> **Gatekeeper note:** Unsigned plugins downloaded by other users will be
> blocked the first time. End users can right-click → **Open** in Finder
> once to grant trust. Properly signed and notarised builds avoid this.

---

## DAW compatibility checklist

- **VST3** — passes the official VST3 validator. Categorised as
  `Instrument` / `Sampler`. Tested in Ableton Live 11+, FL Studio 21+,
  Cubase 12+, Reaper 6+.
- **VST2** — requires the legacy SDK supplied via `VST2_SDK_PATH`. Output
  channel count is up to stereo; no audio input bus.
- **AU**  — passes `auval -v aumu Lpfd Ynme`. Registers as an `aumu`
  MusicDevice (instrument). Tested in Logic Pro 10.7+ and GarageBand. The
  bundle ID `com.yourname.loopfinder` should be changed to something
  unique to you before public distribution.

## MIDI behaviour

- **Polyphony**: 16 simultaneous voices.
- **Pitch tracking**: each MIDI note plays the selected loop region
  resampled by the ratio `2^((noteNumber − rootNote) / 12)`.
- **Root note**: configurable in the footer (slider + note-name display) and
  exposed as a DAW-automatable `rootNote` parameter (default C4 = 60).
- **Envelope**: linear 5 ms attack, 50 ms release — eliminates clicks on
  note-on / note-off without obscuring transients.
- **Held notes** loop the region with the on-the-fly crossfade applied
  to the last K source samples for click-free wraps. Switch the **Loop**
  toggle off to play one-shot instead.
- **Preview**: the on-screen keyboard plus the **▶ Preview / ■ Stop**
  buttons (which trigger a synthetic note at the root pitch) let you
  audition without external MIDI.
- **Voice stealing**: oldest releasing voice first, then overall oldest.

---

## State persistence

`LoopFinderProcessor::getStateInformation` / `setStateInformation` writes an
XML tree containing:

* APVTS parameter state (output volume).
* Audio file — embedded as base64 if ≤ 5 MB, otherwise saved by absolute path.
* All detected loop regions (start / end / score / duration / crossfade flag).
* The currently-selected region index.

When a file is saved by path and the path no longer resolves on restore, the
editor displays a **“Linked file is missing — please re-link via Load File.”**
warning so the user can re-locate it.

---

## Known limitations / v2 ideas

* The window is currently fixed at 780 × 480 (resize is reserved for v2).
* Region hover-highlighting in the waveform from the list panel is wired up
  in the callbacks structure but not yet implemented visually.
* Future: integrate Rubber Band Library for pitch / time options.
