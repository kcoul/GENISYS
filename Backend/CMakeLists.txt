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

    # Find ONNX Runtime (honours ONNXRUNTIME_DIR cache variable for manual installs).
    if(NOT TARGET onnxruntime::onnxruntime)
        find_path(ONNXRUNTIME_INCLUDE_DIR
            NAMES onnxruntime_cxx_api.h
            HINTS "${ONNXRUNTIME_DIR}/include"
            PATH_SUFFIXES onnxruntime/core/session
            REQUIRED)
        find_library(ONNXRUNTIME_LIBRARY
            NAMES onnxruntime
            HINTS "${ONNXRUNTIME_DIR}/lib"
            REQUIRED)
        add_library(onnxruntime::onnxruntime UNKNOWN IMPORTED)
        set_target_properties(onnxruntime::onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}")
    endif()

    target_link_libraries(GenisysBackend PRIVATE
        HailoRT::libhailort
        onnxruntime::onnxruntime
        juce::juce_audio_devices)

    # Download Silero VAD ONNX model if not already present.
    set(_silero_model "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/silero_vad.onnx")
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
        add_custom_command(TARGET GenisysBackend POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_silero_model}"
                    "$<TARGET_FILE_DIR:GenisysBackend>/silero_vad.onnx"
            COMMENT "Copying silero_vad.onnx next to GenisysBackend binary")
    else()
        message(WARNING "GenisysBackend: silero_vad.onnx not found; VAD will fail at runtime")
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
