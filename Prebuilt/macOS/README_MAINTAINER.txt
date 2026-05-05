macOS binary layout
===================

Maintain these paths so Install LoopFinder.command can install without compiling:

  Prebuilt/macOS/LoopFinder.vst3/
  Prebuilt/macOS/LoopFinder.component/   (optional but recommended)

After editing C++ sources, rebuild and refresh:

  git clone … JUCE …   # see README
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --config Release --target LoopFinder_All

Copy from:

  build/LoopFinder_artefacts/Release/VST3/LoopFinder.vst3
  build/LoopFinder_artefacts/Release/AU/LoopFinder.component

into Prebuilt/macOS/, then commit.
