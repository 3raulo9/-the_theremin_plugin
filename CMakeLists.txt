cmake_minimum_required(VERSION 3.22)

project(AetherTheremin VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the MSVC runtime statically so the plugin runs on machines without the
# Visual C++ redistributable installed (important for FL Studio users).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(THEREMIN_BUILD_TESTS "Build the offline test host and snapshot tools" ON)
set(THEREMIN_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (leave empty to download JUCE automatically)")

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
if(THEREMIN_JUCE_PATH)
    add_subdirectory("${THEREMIN_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# The plugin
# ---------------------------------------------------------------------------
set(THEREMIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND THEREMIN_FORMATS AU)
endif()

juce_add_plugin(AetherTheremin
    PRODUCT_NAME                "Aether Theremin"
    COMPANY_NAME                "Aether Instruments"
    BUNDLE_ID                   com.aetherinstruments.aethertheremin
    DESCRIPTION                 "Heterodyne theremin synthesizer"
    PLUGIN_MANUFACTURER_CODE    Aeth
    PLUGIN_CODE                 Thrm
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    FORMATS                     ${THEREMIN_FORMATS})

target_sources(AetherTheremin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/dsp/ThereminEngine.cpp
        Source/gui/ThereminLookAndFeel.cpp
        Source/gui/ThereminView.cpp)

target_include_directories(AetherTheremin PRIVATE Source)

target_compile_definitions(AetherTheremin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(AetherTheremin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests (offline VST3 host + editor snapshot)
# ---------------------------------------------------------------------------
if(THEREMIN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
