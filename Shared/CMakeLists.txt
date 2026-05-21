add_library(GenisysShared INTERFACE)

target_include_directories(GenisysShared INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_link_libraries(GenisysShared
    INTERFACE
        juce::juce_core
        juce::juce_osc)
