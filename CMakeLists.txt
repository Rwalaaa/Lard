cmake_minimum_required(VERSION 3.22)
project(Lard VERSION 0.3.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LARD_BUILD_TOOLS "Construire l'outil de rendu hors-ligne (tests)" ON)

# Windows : runtime Visual C++ intégré aux plugins (/MT). Sinon ils exigent le
# « Redistribuable Visual C++ » récent sur le PC, et Ableton les ignore ou plante sans lui.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
    Source/LardLookAndFeel.cpp
    Source/DemonFace.cpp)

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

# ---- Plugin Koquille (saturateur à lampes) ----
juce_add_plugin(Koquille
    COMPANY_NAME            "Rwalaaa"
    PLUGIN_MANUFACTURER_CODE Rwla
    PLUGIN_CODE             Koqu
    FORMATS                 VST3
    PRODUCT_NAME            "Koquille"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    VST3_CATEGORIES         Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(KoquilleData
    NAMESPACE   KoqBinary
    HEADER_NAME KoqBinaryData.h
    SOURCES
        Resources/fonts/BigShouldersDisplay-ExtraBold-latin.ttf
        Resources/fonts/IBMPlexMono-SemiBold.ttf
        Resources/fonts/IBMPlexSansCondensed-SemiBold.ttf)

set(KOQUILLE_SOURCES
    Koquille/KoquilleProcessor.cpp
    Koquille/KoquilleEditor.cpp
    Koquille/KoquilleLookAndFeel.cpp
    Koquille/NacreBackground.cpp)

target_sources(Koquille PRIVATE ${KOQUILLE_SOURCES})
target_include_directories(Koquille PRIVATE dsp Koquille)
target_compile_definitions(Koquille PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Koquille
    PRIVATE
        KoquilleData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Plugin MAKAR (saturateur de piste à 5 caractères) ----
juce_add_plugin(MAKAR
    COMPANY_NAME            "Rwalaaa"
    PLUGIN_MANUFACTURER_CODE Rwla
    PLUGIN_CODE             Makr
    FORMATS                 VST3
    PRODUCT_NAME            "MAKAR"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    VST3_CATEGORIES         Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(MakarData
    NAMESPACE   MakarBinary
    HEADER_NAME MakarBinaryData.h
    SOURCES
        Resources/fonts/MakarDisplay-Black.ttf
        Resources/fonts/JetBrainsMono-Bold-latin.ttf)

set(MAKAR_SOURCES
    Makar/MakarProcessor.cpp
    Makar/MakarEditor.cpp
    Makar/MakarLookAndFeel.cpp
    Makar/HudBackground.cpp
    Makar/HudWidgets.cpp)

target_sources(MAKAR PRIVATE ${MAKAR_SOURCES})
target_include_directories(MAKAR PRIVATE dsp Makar)
target_compile_definitions(MAKAR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(MAKAR
    PRIVATE
        MakarData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Outils de rendu hors-ligne (C++ pur, sans JUCE) ----
if(LARD_BUILD_TOOLS)
    foreach(tool IN ITEMS lard_render render_koquille render_makar)
        if(tool STREQUAL "lard_render")
            add_executable(${tool} tools/render.cpp)
        else()
            add_executable(${tool} tools/${tool}.cpp)
        endif()
        target_include_directories(${tool} PRIVATE dsp)
        if(MSVC)
            target_compile_definitions(${tool} PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
        endif()
    endforeach()
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

    juce_add_console_app(koquille_snapshot PRODUCT_NAME "koquille_snapshot")
    target_sources(koquille_snapshot PRIVATE tools/snapshot_koquille.cpp ${KOQUILLE_SOURCES})
    target_include_directories(koquille_snapshot PRIVATE dsp Koquille)
    target_compile_definitions(koquille_snapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Koquille")
    target_link_libraries(koquille_snapshot PRIVATE
        KoquilleData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)

    juce_add_console_app(makar_snapshot PRODUCT_NAME "makar_snapshot")
    target_sources(makar_snapshot PRIVATE tools/snapshot_makar.cpp ${MAKAR_SOURCES})
    target_include_directories(makar_snapshot PRIVATE dsp Makar)
    target_compile_definitions(makar_snapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="MAKAR")
    target_link_libraries(makar_snapshot PRIVATE
        MakarData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
