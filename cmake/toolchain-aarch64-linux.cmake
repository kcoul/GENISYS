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
    # One-time setup:
    #   sudo dpkg --add-architecture arm64
    #   sudo apt install \
    #       libasound2-dev:arm64 \
    #       libfreetype-dev:arm64 libfontconfig1-dev:arm64 \
    #       libx11-dev:arm64 libxrandr-dev:arm64 libxinerama-dev:arm64 \
    #       libxcursor-dev:arm64 libxext-dev:arm64
    #
    # CMAKE_SYSROOT=/ + CMAKE_LIBRARY_ARCHITECTURE directs find_library to
    # /usr/lib/aarch64-linux-gnu/ instead of /usr/lib/x86_64-linux-gnu/.
    set(CMAKE_SYSROOT              /)
    set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
    set(CMAKE_FIND_ROOT_PATH       /usr/lib/aarch64-linux-gnu /usr/aarch64-linux-gnu)

    # Redirect pkg-config to aarch64 .pc files.
    # Without this, find_package(PkgConfig) + pkg_check_modules() returns x86_64 library paths.
    set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
    set(ENV{PKG_CONFIG_PATH}   "")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
