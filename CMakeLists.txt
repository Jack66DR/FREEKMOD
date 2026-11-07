cmake_minimum_required(VERSION 3.22)

project(FREEKMOD VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

option(FREEKMOD_BUILD_TESTS "Build the offline engine test, demo render and UI snapshot" OFF)

set(FREEKMOD_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    set(FREEKMOD_FORMATS VST3 Standalone)
endif()

juce_add_plugin(FREEKMOD
    COMPANY_NAME                "manners! DSP"
    BUNDLE_ID                   com.mannersdsp.freekmod
    PLUGIN_MANUFACTURER_CODE    Mnrs
    PLUGIN_CODE                 Frkm
    PRODUCT_NAME                "FREEKMOD"
    DESCRIPTION                 "Four-operator FM after the TX81Z, with big friendly knobs and a SPANNER to throw in the works"
    FORMATS                     ${FREEKMOD_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             Instrument Synth)

# The manners! face: the worn card, its wear maps and the house font. Every plugin in the family ships these.
juce_add_binary_data(FREEKMOD_Assets SOURCES
    Resources/plate.jpg
    Resources/maps.png
    Resources/BarlowSemiCondensed-Medium.ttf)

set(MANNERS_SOURCES
    Source/manners/MannersFace.cpp
    Source/manners/MannersMenu.cpp
    Source/manners/MannersPresets.cpp)

target_sources(FREEKMOD PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${MANNERS_SOURCES})

target_compile_definitions(FREEKMOD PUBLIC
    FREEKMOD_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FREEKMOD
    PRIVATE
        FREEKMOD_Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(FREEKMOD_BUILD_TESTS)
    # Engine-only checks: no JUCE, no GUI, runs on the CI machine.
    add_executable(FREEKMOD_Test Tests/OfflineTest.cpp)
    target_include_directories(FREEKMOD_Test PRIVATE Source)
    set_target_properties(FREEKMOD_Test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/FREEKMOD_Test_artefacts/$<CONFIG>")

    # Full plugin: renders a demo WAV, checks presets and snapshots the editor.
    juce_add_console_app(FREEKMOD_Tool PRODUCT_NAME "FREEKMOD_Tool")
    target_sources(FREEKMOD_Tool PRIVATE
        Tests/Tool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${MANNERS_SOURCES})
    target_include_directories(FREEKMOD_Tool PRIVATE Source)
    target_compile_definitions(FREEKMOD_Tool PRIVATE FREEKMOD_VERSION="${PROJECT_VERSION}" JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FREEKMOD_Tool PRIVATE
        FREEKMOD_Assets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
