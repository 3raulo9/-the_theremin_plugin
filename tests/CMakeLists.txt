# Unit tests, editor snapshot and demo renders (compiled straight from the plugin sources)
juce_add_console_app(ThereminTests PRODUCT_NAME "ThereminTests")

target_sources(ThereminTests
    PRIVATE
        ThereminTests.cpp
        ../Source/PluginProcessor.cpp
        ../Source/PluginEditor.cpp
        ../Source/dsp/ThereminEngine.cpp
        ../Source/gui/ThereminLookAndFeel.cpp
        ../Source/gui/ThereminView.cpp)

target_include_directories(ThereminTests PRIVATE ../Source .)

target_compile_definitions(ThereminTests
    PRIVATE
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(ThereminTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Hosts the built VST3 binary through the real VST3 interfaces
juce_add_console_app(ThereminHostTest PRODUCT_NAME "ThereminHostTest")

target_sources(ThereminHostTest PRIVATE HostTest.cpp)
target_include_directories(ThereminHostTest PRIVATE .)

target_compile_definitions(ThereminHostTest
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(ThereminHostTest
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_dependencies(ThereminHostTest AetherTheremin_VST3)
