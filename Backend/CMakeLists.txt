juce_add_console_app(GenisysBackend
    PRODUCT_NAME "Genisys Backend"
    VERSION      "0.1")

target_sources(GenisysBackend
    PRIVATE
        Source/Main.cpp
        Source/VoiceEngine.cpp)

target_include_directories(GenisysBackend
    PRIVATE
        Source)

option(GENISYS_HAS_HAILO "Enable Hailo NPU voice engine in GenisysBackend" OFF)

target_compile_definitions(GenisysBackend
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        GENISYS_HAS_HAILO=$<BOOL:${GENISYS_HAS_HAILO}>
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GenisysBackend,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GenisysBackend,JUCE_VERSION>")

target_link_libraries(GenisysBackend
    PRIVATE
        GenisysShared
        juce::juce_osc
        juce::juce_audio_devices   # VoiceEngine.h inherits AudioIODeviceCallback unconditionally
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(GENISYS_HAS_HAILO)
    find_package(HailoRT REQUIRED)

    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/FetchOnnxRuntime.cmake")

    # libonnxruntime.so ships next to the binary; tell the dynamic linker to look there.
    set_target_properties(GenisysBackend PROPERTIES
        BUILD_RPATH  "$ORIGIN"
        INSTALL_RPATH "$ORIGIN")

    target_link_libraries(GenisysBackend PRIVATE
        HailoRT::libhailort
        onnxruntime::onnxruntime
        juce::juce_audio_devices)

    # Download Silero VAD ONNX model and embed it via incbin.
    set(_silero_model  "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/silero_vad.onnx")
    set(_incbin_dir    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/incbin")
    set(_gen_dir       "${CMAKE_CURRENT_BINARY_DIR}/generated")

    if(NOT EXISTS "${_silero_model}")
        message(STATUS "GenisysBackend: downloading Silero VAD model...")
        file(DOWNLOAD
            "https://github.com/snakers4/silero-vad/raw/master/files/silero_vad.onnx"
            "${_silero_model}"
            TIMEOUT 60
            STATUS _dl_status)
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            list(GET _dl_status 1 _dl_msg)
            message(WARNING "GenisysBackend: Silero VAD model download failed: ${_dl_msg}")
            file(REMOVE "${_silero_model}")
        endif()
    endif()

    if(EXISTS "${_silero_model}")
        file(MAKE_DIRECTORY "${_gen_dir}")
        set(SILERO_ONNX_PATH "${_silero_model}")
        configure_file("${_incbin_dir}/silero_onnx_embed.c.in"
                       "${_gen_dir}/silero_onnx_embed.c" @ONLY)
        target_sources(GenisysBackend PRIVATE "${_gen_dir}/silero_onnx_embed.c")
        target_include_directories(GenisysBackend PRIVATE "${_incbin_dir}")
        target_compile_definitions(GenisysBackend PRIVATE GENISYS_HAS_EMBEDDED_SILERO_VAD=1)
    else()
        target_compile_definitions(GenisysBackend PRIVATE GENISYS_HAS_EMBEDDED_SILERO_VAD=0)
        message(WARNING "GenisysBackend: silero_vad.onnx not found; will search next to binary at runtime")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_compile_definitions(GenisysBackend PRIVATE NOMINMAX=1)
    endif()

    # Copy Whisper HEF models (Tiny / Base / Small) next to the binary.
    set(_hailo_models_dir "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/models/hailo10h")
    foreach(_hef_name Tiny Base Small)
        set(_hef_src "${_hailo_models_dir}/Whisper-${_hef_name}.hef")
        if(EXISTS "${_hef_src}")
            add_custom_command(TARGET GenisysBackend POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                        "$<TARGET_FILE_DIR:GenisysBackend>/models/hailo10h"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${_hef_src}"
                        "$<TARGET_FILE_DIR:GenisysBackend>/models/hailo10h/Whisper-${_hef_name}.hef"
                COMMENT "Copying Whisper-${_hef_name}.hef next to GenisysBackend binary")
        endif()
    endforeach()
endif()
