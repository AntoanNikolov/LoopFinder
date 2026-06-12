# Standalone console executable that runs all LoopDetector unit tests.
# It links against juce_core (for the UnitTest framework) and includes
# LoopDetector.cpp directly so it has no plugin dependency.

juce_add_console_app(LoopDetectorTests
    PRODUCT_NAME "LoopDetectorTests"
)

target_sources(LoopDetectorTests PRIVATE
    LoopDetectorTests.cpp
    ${CMAKE_SOURCE_DIR}/Source/LoopDetector.cpp
    ${CMAKE_SOURCE_DIR}/Source/PitchDetector.cpp
)

target_include_directories(LoopDetectorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(LoopDetectorTests
    PRIVATE
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_test(NAME LoopDetectorTests
         COMMAND $<TARGET_FILE:LoopDetectorTests>)

# ----------------------------------------------------------------------------
# LoopFinderSnapshot — developer tool that renders the editor offscreen to a
# PNG (optionally after loading + analysing a file). Not part of `all`.
# ----------------------------------------------------------------------------
juce_add_console_app(LoopFinderSnapshot PRODUCT_NAME "LoopFinderSnapshot")
set_target_properties(LoopFinderSnapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)

target_sources(LoopFinderSnapshot PRIVATE
    SnapshotTool.cpp
    ${CMAKE_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/Source/AudioFileManager.cpp
    ${CMAKE_SOURCE_DIR}/Source/LoopDetector.cpp
    ${CMAKE_SOURCE_DIR}/Source/PitchDetector.cpp
    ${CMAKE_SOURCE_DIR}/Source/PlaybackEngine.cpp
    ${CMAKE_SOURCE_DIR}/Source/WaveformDisplay.cpp
    ${CMAKE_SOURCE_DIR}/Source/RegionListPanel.cpp
    ${CMAKE_SOURCE_DIR}/Source/LoopExporter.cpp
)

target_include_directories(LoopFinderSnapshot PRIVATE ${CMAKE_SOURCE_DIR}/Source)
juce_generate_juce_header(LoopFinderSnapshot)

target_compile_definitions(LoopFinderSnapshot PRIVATE
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(LoopFinderSnapshot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
)
