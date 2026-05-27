cmake_minimum_required(VERSION 3.22)
project(GENISYS VERSION 0.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GENISYS_JUCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE"
    CACHE PATH "Path to the JUCE source tree")

if(NOT EXISTS "${GENISYS_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${GENISYS_JUCE_DIR}\n"
        "Run: git submodule add -b qnx-ports https://github.com/kcoul/JUCE.git libs/JUCE\n"
        "  or pass -DGENISYS_JUCE_DIR=/path/to/JUCE")
endif()

if(NOT TARGET juce::juce_core)
    add_subdirectory("${GENISYS_JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
endif()

add_subdirectory(Shared)
add_subdirectory(Frontend)
add_subdirectory(Backend)
add_subdirectory(DebugConsole)
