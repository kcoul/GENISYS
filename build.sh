#!/usr/bin/env bash
# Cross-compile all three GENISYS binaries for aarch64 Ubuntu/RPi OS (RPi5) from WSL2.
#
# Prerequisites (WSL2/Ubuntu host):
#   # Build tools + aarch64 cross-compiler
#   sudo apt install cmake ninja-build gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#
#   # Enable arm64 multiarch and install target libraries
#   sudo dpkg --add-architecture arm64
#   sudo apt update
#   sudo apt install \
#       libasound2-dev:arm64 \
#       libfreetype-dev:arm64 libfontconfig1-dev:arm64 \
#       libx11-dev:arm64 libxrandr-dev:arm64 libxinerama-dev:arm64 \
#       libxcursor-dev:arm64 libxext-dev:arm64 \
#       libonnxruntime-dev:arm64
#
#   Optional: provide a full Pi sysroot to avoid multiarch setup:
#     export SYSROOT=/path/to/rpi5-sysroot
#
# Usage:
#   ./build.sh             # GENISYS_HAS_HAILO=ON   (default — backend always targets RPi5+Hailo)
#   ./build.sh --no-hailo  # GENISYS_HAS_HAILO=OFF  (OSC + UI only, no HailoRT SDK required)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
NATIVE_BUILD="$BUILD_DIR/native"
CROSS_BUILD="$BUILD_DIR/aarch64-linux"
DIST_DIR="$BUILD_DIR/dist"

HAILO=ON
if [[ "${1:-}" == "--no-hailo" ]]; then
    HAILO=OFF
fi

# ── Step 1: Configure natively to bootstrap juceaide ─────────────────────────
# JUCE builds juceaide via execute_process during CMake *configure* (not during
# the build step). It ends up as an IMPORTED target, so there is no Ninja rule
# for it — attempting --target juceaide will always fail.  We just need the
# configure step to complete; by then the binary exists on disk.
echo "=== Step 1: Native configure (bootstraps juceaide) ==="
cmake -S "$SCRIPT_DIR" -B "$NATIVE_BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGENISYS_HAS_HAILO=OFF \
    -G Ninja

# JUCE writes the bootstrap binary under <native_build>/JUCE/tools/
JUCEAIDE_EXE="$(find "$NATIVE_BUILD/JUCE" -name "juceaide" -type f 2>/dev/null | head -1)"
if [[ -z "$JUCEAIDE_EXE" || ! -x "$JUCEAIDE_EXE" ]]; then
    echo "ERROR: juceaide binary not found under $NATIVE_BUILD/JUCE"
    echo "  Hint: $(find "$NATIVE_BUILD" -name "juceaide" 2>/dev/null | head -5)"
    exit 1
fi
echo "  juceaide: $JUCEAIDE_EXE"

# ── Step 2: Cross-compile all three GENISYS targets ──────────────────────────
# JUCE_JUCEAIDE_PATH: when defined, JUCE imports that binary directly and skips
# the bootstrap entirely — no chicken-and-egg problem for cross builds.
echo ""
echo "=== Step 2: Cross-compile GENISYS for aarch64 (GENISYS_HAS_HAILO=$HAILO) ==="
cmake -S "$SCRIPT_DIR" -B "$CROSS_BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/cmake/toolchain-aarch64-linux.cmake" \
    -DJUCE_JUCEAIDE_PATH="$JUCEAIDE_EXE" \
    -DGENISYS_HAS_HAILO="$HAILO" \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja
cmake --build "$CROSS_BUILD"

# ── Step 3: Collect binaries into a flat dist/ directory ────────────────────
# JUCE names the output binary after PRODUCT_NAME, which may contain spaces.
# We copy each to a no-space name for deploy.py.
mkdir -p "$DIST_DIR"
echo ""
echo "=== Collecting binaries ==="
collect() {
    local dest="$1"; shift   # target filename in dist/
    # remaining args: candidate names to search for (PRODUCT_NAME may differ from target)
    for name in "$@"; do
        local src
        src="$(find "$CROSS_BUILD" -type f -name "$name" ! -path "*/_deps/*" 2>/dev/null | head -1)"
        if [[ -n "$src" ]]; then
            cp "$src" "$DIST_DIR/$dest"
            echo "  → dist/$dest"
            return
        fi
    done
    echo "  WARNING: $dest not found in $CROSS_BUILD"
}

collect GenisysBackend      "GenisysBackend"    "Genisys Backend"
collect GenisysFrontend     "GenisysFrontend"   "Genisys Frontend"
collect GenisysDebugConsole "GenisysDebugConsole" "Genisys Debug Console"

# Collect Hailo HEF models copied next to the backend binary during POST_BUILD.
HEF_SRC="$CROSS_BUILD/Backend/models/hailo10h"
if [[ -d "$HEF_SRC" ]]; then
    mkdir -p "$DIST_DIR/models/hailo10h"
    if cp "$HEF_SRC/"*.hef "$DIST_DIR/models/hailo10h/" 2>/dev/null; then
        echo "  → dist/models/hailo10h/*.hef"
    else
        echo "  WARNING: no .hef files found in $HEF_SRC"
    fi
fi

echo ""
echo "=== Done ==="
echo "  Run:  python deploy.py --target-ip <pi-ip>"
