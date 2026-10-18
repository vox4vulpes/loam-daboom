cmake_minimum_required(VERSION 3.22)
project(LoamDaBoom VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG master # ponytail: pin an exact release tag once one is chosen, for reproducible builds
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# The plugin target pulls in JUCE's GUI/windowing modules, which on Linux
# need dev packages (freetype, X11, ALSA, ...) that a plain DSP-only build
# doesn't require. Flip this off to configure just the EngineTest target.
option(LOAM_BUILD_PLUGIN "Build the VST3/Standalone plugin (needs GUI dev libs on Linux)" ON)

if(LOAM_BUILD_PLUGIN)
    juce_add_plugin(LoamDaBoom
        COMPANY_NAME "Loam"
        PRODUCT_NAME "Loam DaBoom"
        PLUGIN_MANUFACTURER_CODE Loam
        PLUGIN_CODE Ldbm
        FORMATS VST3 Standalone
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        VST3_CATEGORIES Instrument Drum
        # SVG so JUCE renders every icon size crisply (16-256 px on Windows).
        ICON_BIG Resources/icon.svg
        ICON_SMALL Resources/icon-small.svg
    )

    target_sources(LoamDaBoom PRIVATE
        Source/DrumEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )

    target_compile_definitions(LoamDaBoom PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(LoamDaBoom PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce_recommended_config_flags
        juce_recommended_lto_flags
        juce_recommended_warning_flags
    )
endif()

# DSP self-test: links only juce_core/juce_audio_basics/juce_dsp, so it builds
# without the GUI/windowing dev packages the full plugin needs.
add_executable(EngineTest
    Tests/EngineTest.cpp
    Source/DrumEngine.cpp
)
target_include_directories(EngineTest PRIVATE Source)
target_compile_definitions(EngineTest PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(EngineTest PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
)
