cmake_minimum_required(VERSION 3.22)

project(LoopFinder
    VERSION 0.1.0
    DESCRIPTION "Automatically detect seamless loop points in audio"
    LANGUAGES C CXX)

# ----------------------------------------------------------------------------
# Global compile settings
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment target")
endif()

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------
option(LOOPFINDER_BUILD_TESTS "Build LoopDetector unit tests"        ON)
option(ENABLE_CODESIGN        "Codesign macOS plugins after build"   OFF)
option(LOOPFINDER_AUTO_COPY   "Copy plugin bundles to system dirs on build" OFF)

# Path to the legacy VST2 SDK. If set and the directory exists, the VST2
# format is added to the built plugin formats.
set(VST2_SDK_PATH "" CACHE PATH "Path to the legacy VST2 SDK (optional)")

set(CODESIGN_IDENTITY "" CACHE STRING "Apple Developer ID for codesigning (e.g. 'Developer ID Application: Your Name (TEAMID)')")
set(CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/entitlements.plist" CACHE FILEPATH "Path to entitlements plist")

# ----------------------------------------------------------------------------
# JUCE
# ----------------------------------------------------------------------------
# Expect JUCE as a git submodule under ./JUCE
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${CMAKE_CURRENT_SOURCE_DIR}/JUCE.\n"
        "Add JUCE as a submodule:\n"
        "  git submodule add https://github.com/juce-framework/JUCE.git JUCE\n"
        "  git submodule update --init --recursive")
endif()

add_subdirectory(JUCE)

# ----------------------------------------------------------------------------
# VST2 detection
# ----------------------------------------------------------------------------
set(LOOPFINDER_FORMATS VST3 AU Standalone)

if(VST2_SDK_PATH AND EXISTS "${VST2_SDK_PATH}")
    message(STATUS "VST2 SDK found at ${VST2_SDK_PATH} — enabling VST2 build")
    juce_set_vst2_sdk_path("${VST2_SDK_PATH}")
    list(APPEND LOOPFINDER_FORMATS VST)
else()
    message(STATUS "VST2_SDK_PATH not set or invalid — VST2 build disabled")
endif()

if(NOT APPLE)
    list(REMOVE_ITEM LOOPFINDER_FORMATS AU)
endif()

# ----------------------------------------------------------------------------
# Plugin target
# ----------------------------------------------------------------------------
juce_add_plugin(LoopFinder
    COMPANY_NAME              "LoopFinder"
    COMPANY_WEBSITE           "https://example.com"
    COMPANY_EMAIL             "support@example.com"
    BUNDLE_ID                 com.loopfinder.loopfinder
    PLUGIN_MANUFACTURER_CODE  Ynme
    PLUGIN_CODE               Lpfd
    FORMATS                   ${LOOPFINDER_FORMATS}
    PRODUCT_NAME              "LoopFinder"
    DESCRIPTION               "Detect seamless loop points and play them via MIDI"
    PLUGIN_NAME               "LoopFinder"

    # LoopFinder is a sampler-style instrument: it accepts MIDI input and
    # generates audio output (no audio input).
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   ${LOOPFINDER_AUTO_COPY}

    VST3_CATEGORIES           "Instrument" "Sampler"
    AU_MAIN_TYPE              "kAudioUnitType_MusicDevice"

    HARDENED_RUNTIME_ENABLED  TRUE
    HARDENED_RUNTIME_OPTIONS  "com.apple.security.device.audio-input"
)

# Allow VST3 hosts to load this plugin in place of an existing VST2.
target_compile_definitions(LoopFinder PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_MaxNumInputChannels=0
    JucePlugin_MaxNumOutputChannels=2
)

# ----------------------------------------------------------------------------
# Sources
# ----------------------------------------------------------------------------
target_sources(LoopFinder PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/LoopDetector.cpp
    Source/LoopDetector.h
    Source/LoopRegion.h
    Source/AudioFileManager.cpp
    Source/AudioFileManager.h
    Source/WaveformDisplay.cpp
    Source/WaveformDisplay.h
    Source/RegionListPanel.cpp
    Source/RegionListPanel.h
    Source/PlaybackEngine.cpp
    Source/PlaybackEngine.h
    Source/LoopExporter.cpp
    Source/LoopExporter.h
    Source/Theme.h
)

target_include_directories(LoopFinder PRIVATE Source)

juce_generate_juce_header(LoopFinder)

target_link_libraries(LoopFinder
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
# `juce_add_plugin` already creates a `LoopFinder_All` meta-target that
# depends on every requested format, so no extra wiring is needed here.

# ----------------------------------------------------------------------------
# macOS code signing (post-build)
# ----------------------------------------------------------------------------
if(APPLE AND ENABLE_CODESIGN)
    if(NOT CODESIGN_IDENTITY)
        message(WARNING "ENABLE_CODESIGN is ON but CODESIGN_IDENTITY is empty — using ad-hoc signing")
        set(_sign_identity "-")
    else()
        set(_sign_identity "${CODESIGN_IDENTITY}")
    endif()

    foreach(_fmt IN ITEMS VST3 AU VST Standalone)
        if(TARGET LoopFinder_${_fmt})
            add_custom_command(TARGET LoopFinder_${_fmt} POST_BUILD
                COMMAND codesign --force --options runtime --timestamp
                        --entitlements "${CODESIGN_ENTITLEMENTS}"
                        --sign "${_sign_identity}"
                        "$<TARGET_BUNDLE_DIR:LoopFinder_${_fmt}>"
                COMMENT "Codesigning LoopFinder_${_fmt}")
        endif()
    endforeach()
endif()

# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------
if(LOOPFINDER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
