cmake_minimum_required(VERSION 3.22)
project(Lard VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LARD_BUILD_TOOLS "Construire l'outil de rendu hors-ligne (tests)" ON)

# ---- JUCE, récupéré automatiquement à la configuration ----
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---- Plugin ----
juce_add_plugin(Lard
    COMPANY_NAME            "Rwalaaa"
    PLUGIN_MANUFACTURER_CODE Rwla
    PLUGIN_CODE             Lard
    FORMATS                 VST3
    PRODUCT_NAME            "Lard"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    VST3_CATEGORIES         Fx Distortion Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(LardData SOURCES
    Resources/fonts/DelaGothicOne-Regular-latin.ttf
    Resources/fonts/ZenKakuGothicNew-Bold-latin.ttf)

set(LARD_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LardLookAndFeel.cpp)

target_sources(Lard PRIVATE ${LARD_SOURCES})

target_include_directories(Lard PRIVATE dsp Source)

target_compile_definitions(Lard PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Lard
    PRIVATE
        LardData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Outil de rendu hors-ligne (C++ pur, sans JUCE) ----
if(LARD_BUILD_TOOLS)
    add_executable(lard_render tools/render.cpp)
    target_include_directories(lard_render PRIVATE dsp)
    if(MSVC)
        target_compile_definitions(lard_render PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# ---- Capture de l'interface en PNG (vérification visuelle, hors DAW) ----
option(LARD_BUILD_SNAPSHOT "Construire l'outil de capture de l'interface" OFF)
if(LARD_BUILD_SNAPSHOT)
    juce_add_console_app(lard_snapshot PRODUCT_NAME "lard_snapshot")
    target_sources(lard_snapshot PRIVATE tools/snapshot.cpp ${LARD_SOURCES})
    target_include_directories(lard_snapshot PRIVATE dsp Source)
    target_compile_definitions(lard_snapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Lard")
    target_link_libraries(lard_snapshot PRIVATE
        LardData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
