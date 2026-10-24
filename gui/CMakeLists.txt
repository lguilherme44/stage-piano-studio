cmake_minimum_required(VERSION 3.22)
project(StagePianoStudio VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE repository if not already added by parent project
if(NOT TARGET juce::juce_core)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.4
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

juce_add_gui_app(StagePianoStudio
    PRODUCT_NAME "Stage Piano Studio"
    COMPANY_NAME "StagePiano"
    BUNDLE_ID "com.stagepiano.studio"
)

target_sources(StagePianoStudio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/StagePianoLookAndFeel.cpp
    
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Sampler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Equalizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Compressor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Chorus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Delay.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Reverb.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/TimbrePreset.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/StagePianoHost.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/SampleImporter.cpp"
)

target_include_directories(StagePianoStudio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/include"
)

target_compile_definitions(StagePianoStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(StagePianoStudio PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
)
