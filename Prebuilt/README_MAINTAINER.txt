Prebuilt binaries — maintainer notes
====================================

The install scripts ("Install LoopFinder.command" / "Install LoopFinder.bat")
are copy-only: they install whatever is committed here. Keep these paths
up to date whenever the C++ sources change:

  Prebuilt/macOS/LoopFinder.vst3/
  Prebuilt/macOS/LoopFinder.component/
  Prebuilt/Windows/LoopFinder.vst3/

How to refresh
--------------

Option A — CI (easiest): push to GitHub, then download the
"LoopFinder-macOS" and "LoopFinder-Windows" artifacts from the Build
workflow and copy them into the folders above.

Option B — local build:

  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --config Release --target LoopFinder_All

  Copy from build/LoopFinder_artefacts/Release/VST3/ (and AU/ on macOS)
  into the matching Prebuilt/ folder, then commit.

The macOS bundles are universal (arm64 + x86_64).
