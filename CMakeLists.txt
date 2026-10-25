cmake_minimum_required(VERSION 3.22)
project(StagePianoStudio VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE framework resolution:
# 1. Use local folder/submodule if present
# 2. Otherwise fetch automatically from GitHub
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(STATUS "Using local JUCE repository at ${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/JUCE" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
else()
    message(STATUS "Fetching JUCE 8.0.4 from official repository...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

add_subdirectory(gui)

# Unit / Integration Tests
enable_testing()
add_executable(test_importer
    tools/test_importer.cpp
    engine/src/SampleImporter.cpp
)
target_include_directories(test_importer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/include"
)
add_test(NAME TestImporter COMMAND test_importer)

add_executable(test_engine
    tools/test_engine.cpp
    engine/src/StagePianoHost.cpp
    engine/src/Sampler.cpp
    engine/src/Chorus.cpp
    engine/src/Compressor.cpp
    engine/src/Delay.cpp
    engine/src/Equalizer.cpp
    engine/src/Reverb.cpp
    engine/src/SampleImporter.cpp
    engine/src/TimbrePreset.cpp
)
target_include_directories(test_engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/include"
)
add_test(NAME TestEngine COMMAND test_engine)

