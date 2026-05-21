cmake_minimum_required(VERSION 3.22)
project(GENISYS VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow an out-of-tree build to supply its own JUCE.
# Default: use the JUCE already present in the JUCE4Pi repo.
set(GENISYS_JUCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../surge/libs/JUCE"
    CACHE PATH "Path to the JUCE source tree")

if(NOT TARGET juce::juce_core)
    add_subdirectory("${GENISYS_JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
endif()

add_subdirectory(Shared)
add_subdirectory(Frontend)
add_subdirectory(Backend)
add_subdirectory(DataBridge)
add_subdirectory(VREngine)
