cmake_minimum_required(VERSION 3.22)
project(Euphoria VERSION 1.0.0 LANGUAGES C CXX)
add_compile_definitions(EUP_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(EUP_BUILD_PLUGIN "Build the JUCE plugin (VST3 + Standalone)" ON)
option(EUP_BUILD_CHECKS "Build the headless checks" OFF)
set(EUP_JUCE_PATH "" CACHE PATH "Optional local JUCE checkout (otherwise fetched from GitHub)")

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(EUP_JUCE_PATH)
    add_subdirectory(${EUP_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(EUP_DSP_SOURCES Source/dsp/Engine.cpp)
set(EUP_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1 JUCE_DISPLAY_SPLASH_SCREEN=0)
set(EUP_JUCE_LIBS juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra juce::juce_cryptography)

set(EUP_PLUGIN_SOURCES
    ${EUP_DSP_SOURCES}
    Source/plugin/Licensing.cpp
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    Source/plugin/Ephy.cpp)

if(EUP_BUILD_PLUGIN)
    if(APPLE)
        set(EUP_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(EUP_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Euphoria
        COMPANY_NAME "Ephoric"
        COMPANY_WEBSITE "https://www.ephoric.nl"
        BUNDLE_ID "com.ephoric.euphoria"
        PLUGIN_MANUFACTURER_CODE Ephr
        PLUGIN_CODE Eup1
        FORMATS ${EUP_PLUGIN_FORMATS}
        PRODUCT_NAME "Euphoria"
        PLUGIN_NAME "Euphoria"
        DESCRIPTION "One-knob euphoric lead enhancer by EPH SAMPLES"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES Fx Reverb
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(Euphoria PRIVATE ${EUP_PLUGIN_SOURCES})
    target_compile_definitions(Euphoria PUBLIC ${EUP_DEFS})
    target_link_libraries(Euphoria
        PRIVATE ${EUP_JUCE_LIBS}
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endif()

if(EUP_BUILD_CHECKS)
    # end-to-end check of the real processor and engine (levels, mono bass, bypass, state, editor screenshot)
    juce_add_console_app(eup_plugin_check PRODUCT_NAME "eup_plugin_check")
    target_sources(eup_plugin_check PRIVATE tests/PluginCheck.cpp ${EUP_PLUGIN_SOURCES})
    target_compile_definitions(eup_plugin_check PRIVATE ${EUP_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Euphoria")
    target_link_libraries(eup_plugin_check PRIVATE ${EUP_JUCE_LIBS} juce::juce_recommended_config_flags)
endif()

option(EUP_BUILD_PROMO "Build the promo video renderer (local tool)" OFF)
if(EUP_BUILD_PROMO)
    juce_add_console_app(eup_promo PRODUCT_NAME "eup_promo")
    target_sources(eup_promo PRIVATE tools/PromoRender.cpp ${EUP_PLUGIN_SOURCES})
    target_compile_definitions(eup_promo PRIVATE ${EUP_DEFS} JucePlugin_Name="Euphoria")
    target_link_libraries(eup_promo PRIVATE ${EUP_JUCE_LIBS} juce::juce_recommended_config_flags)
    juce_add_console_app(eup_promo2 PRODUCT_NAME "eup_promo2")
    target_sources(eup_promo2 PRIVATE tools/PromoRender2.cpp ${EUP_PLUGIN_SOURCES})
    target_compile_definitions(eup_promo2 PRIVATE ${EUP_DEFS} JucePlugin_Name="Euphoria")
    target_link_libraries(eup_promo2 PRIVATE ${EUP_JUCE_LIBS} juce::juce_recommended_config_flags)
    juce_add_console_app(eup_promo3 PRODUCT_NAME "eup_promo3")
    target_sources(eup_promo3 PRIVATE tools/PromoRender3.cpp ${EUP_PLUGIN_SOURCES})
    target_compile_definitions(eup_promo3 PRIVATE ${EUP_DEFS} JucePlugin_Name="Euphoria")
    target_link_libraries(eup_promo3 PRIVATE ${EUP_JUCE_LIBS} juce::juce_recommended_config_flags)
    juce_add_console_app(eup_promo4 PRODUCT_NAME "eup_promo4")
    target_sources(eup_promo4 PRIVATE tools/PromoRender4.cpp ${EUP_PLUGIN_SOURCES})
    target_compile_definitions(eup_promo4 PRIVATE ${EUP_DEFS} JucePlugin_Name="Euphoria")
    target_link_libraries(eup_promo4 PRIVATE ${EUP_JUCE_LIBS} juce::juce_recommended_config_flags)
endif()
