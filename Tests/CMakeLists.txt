# Standalone console executable that runs all LoopDetector unit tests.
# It links against juce_core (for the UnitTest framework) and includes
# LoopDetector.cpp directly so it has no plugin dependency.

juce_add_console_app(LoopDetectorTests
    PRODUCT_NAME "LoopDetectorTests"
)

target_sources(LoopDetectorTests PRIVATE
    LoopDetectorTests.cpp
    ${CMAKE_SOURCE_DIR}/Source/LoopDetector.cpp
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
