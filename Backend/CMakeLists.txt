juce_add_gui_app(GenisysBackend
    PRODUCT_NAME "Genisys Backend"
    VERSION      "0.1")

target_sources(GenisysBackend
    PRIVATE
        Source/Main.cpp)

target_include_directories(GenisysBackend
    PRIVATE
        Source)

target_compile_definitions(GenisysBackend
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GenisysBackend,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GenisysBackend,JUCE_VERSION>")

target_link_libraries(GenisysBackend
    PRIVATE
        GenisysShared
        juce::juce_gui_basics
        juce::juce_osc
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
