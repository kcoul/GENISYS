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

target_compile_definitions(GenisysBackend
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
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

find_package(HailoRT REQUIRED)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/FetchOnnxRuntime.cmake")

# libonnxruntime.so ships next to the binary; tell the dynamic linker to look there.
set_target_properties(GenisysBackend PROPERTIES
    BUILD_RPATH  "$ORIGIN"
    INSTALL_RPATH "$ORIGIN")

target_link_libraries(GenisysBackend PRIVATE
    HailoRT::libhailort
    onnxruntime::onnxruntime)

# Download Silero VAD ONNX model and embed it via incbin.
set(_silero_model  "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/silero_vad.onnx")
set(_incbin_dir    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/incbin")
set(_gen_dir       "${CMAKE_CURRENT_BINARY_DIR}/generated")

if(NOT EXISTS "${_silero_model}")
    message(STATUS "GenisysBackend: downloading Silero VAD model...")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Resources")
    file(DOWNLOAD
        "https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx"
        "${_silero_model}"
        TIMEOUT 60
        STATUS _dl_status)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        list(GET _dl_status 1 _dl_msg)
        file(REMOVE "${_silero_model}")
        message(FATAL_ERROR "GenisysBackend: Silero VAD model download failed: ${_dl_msg}\n"
            "Download manually to ${_silero_model} then reconfigure.")
    endif()
endif()

file(MAKE_DIRECTORY "${_gen_dir}")
set(_embed_src "${_gen_dir}/silero_onnx_embed.c")
set(_genisys_cmake_dir "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if(WIN32)
    # MSVC/clang-cl do not support GAS inline assembly (.incbin); generate a C array instead.
    add_custom_command(
        OUTPUT  "${_embed_src}"
        COMMAND "${CMAKE_COMMAND}"
                -DINPUT="${_silero_model}"
                -DOUTPUT="${_embed_src}"
                -DSYMBOL=SileroVadOnnx
                -DPREFIX=g
                -P "${_genisys_cmake_dir}/BinaryToCArray.cmake"
        DEPENDS "${_silero_model}"
        COMMENT "Embedding Silero VAD model as C array")
else()
    set(SILERO_ONNX_PATH "${_silero_model}")
    configure_file("${_incbin_dir}/silero_onnx_embed.c.in" "${_embed_src}" @ONLY)
endif()

target_sources(GenisysBackend PRIVATE "${_embed_src}")
target_include_directories(GenisysBackend PRIVATE "${_incbin_dir}")
target_compile_definitions(GenisysBackend PRIVATE GENISYS_HAS_EMBEDDED_SILERO_VAD=1)

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
