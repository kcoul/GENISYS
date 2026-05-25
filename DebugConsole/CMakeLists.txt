juce_add_gui_app(GenisysDebugConsole
    PRODUCT_NAME "Genisys Debug Console"
    VERSION      "0.1")

target_sources(GenisysDebugConsole
    PRIVATE
        Source/Main.cpp)

target_include_directories(GenisysDebugConsole
    PRIVATE
        Source)

target_compile_definitions(GenisysDebugConsole
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GenisysDebugConsole,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GenisysDebugConsole,JUCE_VERSION>")

target_link_libraries(GenisysDebugConsole
    PRIVATE
        GenisysShared
        juce::juce_gui_basics
        juce::juce_osc
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
