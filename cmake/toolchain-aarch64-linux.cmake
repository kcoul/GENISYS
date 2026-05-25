set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

if(DEFINED ENV{SYSROOT})
    # Full Pi sysroot provided — strict mode, find only within it.
    set(CMAKE_SYSROOT           "$ENV{SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH    "$ENV{SYSROOT}")
else()
    # Ubuntu multiarch layout (no separate sysroot).
    # sudo dpkg --add-architecture arm64
    # sudo apt install libfreetype-dev:arm64 libfontconfig1-dev:arm64 \
    #     libx11-dev:arm64 libxrandr-dev:arm64 libxinerama-dev:arm64 \
    #     libxcursor-dev:arm64 libxext-dev:arm64 libasound2-dev:arm64
    #
    # CMAKE_SYSROOT=/ + CMAKE_LIBRARY_ARCHITECTURE causes CMake to prefer
    # /usr/lib/aarch64-linux-gnu/ over /usr/lib/x86_64-linux-gnu/.
    set(CMAKE_SYSROOT              /)
    set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
    set(CMAKE_FIND_ROOT_PATH       /usr/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu)
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
