cmake_minimum_required(VERSION 3.22)

# macOS deployment target must be set before project()
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(MAVKA VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-------------------------------------------------------------------------------
# JUCE: a local checkout can be used with  -DJUCE_PATH=/path/to/JUCE
# otherwise a pinned release is downloaded automatically.
set(JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")

if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.6
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

#-------------------------------------------------------------------------------
if(APPLE)
    set(MAVKA_DEFAULT_FORMATS VST3 AU Standalone)
else()
    set(MAVKA_DEFAULT_FORMATS VST3 Standalone)
endif()
set(MAVKA_FORMATS "${MAVKA_DEFAULT_FORMATS}" CACHE STRING "Plugin formats to build")

juce_add_plugin(MAVKA
    COMPANY_NAME                "MAVKA Audio"
    BUNDLE_ID                   com.mavkaaudio.mavka
    PLUGIN_MANUFACTURER_CODE    Mvka
    PLUGIN_CODE                 Mvk1
    FORMATS                     ${MAVKA_FORMATS}
    PRODUCT_NAME                "MAVKA"
    DESCRIPTION                 "Vocal handwriting distortion"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect)

#-------------------------------------------------------------------------------
# Sources may sit in Source/ dsp/ tests/ (as in the archive) or all together in the
# project root (e.g. after a flat upload to GitHub) - both layouts build.
function(mavka_locate out name)
    foreach(dir Source dsp tests .)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${name}")
            set(${out} "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${name}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "MAVKA: cannot find ${name} (looked in Source/, dsp/, tests/ and the project folder)")
endfunction()

set(MAVKA_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(dir Source dsp)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
        list(APPEND MAVKA_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
    endif()
endforeach()

mavka_locate(MAVKA_PROCESSOR_CPP PluginProcessor.cpp)
mavka_locate(MAVKA_EDITOR_CPP    PluginEditor.cpp)
mavka_locate(MAVKA_ENGINE_H      MavkaEngine.h)

target_sources(MAVKA PRIVATE
    ${MAVKA_PROCESSOR_CPP}
    ${MAVKA_EDITOR_CPP})

target_include_directories(MAVKA PRIVATE ${MAVKA_INCLUDE_DIRS})

target_compile_definitions(MAVKA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(MAVKA PRIVATE /utf-8)
endif()

if("Standalone" IN_LIST MAVKA_FORMATS)
    set(MAVKA_JUCE_MODULES juce::juce_audio_utils)
else()
    set(MAVKA_JUCE_MODULES juce::juce_audio_processors)
endif()

target_link_libraries(MAVKA
    PRIVATE
        ${MAVKA_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#-------------------------------------------------------------------------------
# Offline DSP test bench (no JUCE needed):  -DMAVKA_BUILD_TESTS=ON
option(MAVKA_BUILD_TESTS "Build the offline DSP test bench" OFF)
if(MAVKA_BUILD_TESTS)
    mavka_locate(MAVKA_TEST_CPP mavka_test.cpp)
    add_executable(mavka_test ${MAVKA_TEST_CPP})
    target_include_directories(mavka_test PRIVATE ${MAVKA_INCLUDE_DIRS})
endif()

# Headless check of the JUCE wrapper (instantiates the processor, renders the UI to PNG):
#   -DMAVKA_BUILD_PLUGIN_TEST=ON
option(MAVKA_BUILD_PLUGIN_TEST "Build the headless JUCE wrapper test" OFF)
if(MAVKA_BUILD_PLUGIN_TEST)
    mavka_locate(MAVKA_PLUGIN_TEST_CPP plugin_test.cpp)
    juce_add_console_app(mavka_plugin_test PRODUCT_NAME "mavka_plugin_test")
    target_sources(mavka_plugin_test PRIVATE ${MAVKA_PLUGIN_TEST_CPP} ${MAVKA_PROCESSOR_CPP} ${MAVKA_EDITOR_CPP})
    target_include_directories(mavka_plugin_test PRIVATE ${MAVKA_INCLUDE_DIRS})
    target_compile_definitions(mavka_plugin_test PRIVATE
        JucePlugin_Name="MAVKA" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(mavka_plugin_test PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()
