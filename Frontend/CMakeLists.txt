juce_add_gui_app(GenisysFrontend
    PRODUCT_NAME "Genisys Frontend"
    VERSION      "0.1")

target_sources(GenisysFrontend
    PRIVATE
        Source/Main.cpp)

target_include_directories(GenisysFrontend
    PRIVATE
        Source)

target_compile_definitions(GenisysFrontend
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GenisysFrontend,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GenisysFrontend,JUCE_VERSION>")

target_link_libraries(GenisysFrontend
    PRIVATE
        GenisysShared
        juce::juce_gui_basics
        juce::juce_osc
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
