#!/usr/bin/env bash
# Cross-compile all three GENISYS binaries for aarch64 Ubuntu/RPi OS (RPi5) from WSL2.
#
# Prerequisites (WSL2/Ubuntu host):
#   sudo apt install cmake ninja-build \
#       gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
#       libfreetype-dev libfontconfig-dev \
#       libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
#       libasound2-dev
#
#   If host and target libraries conflict (e.g. arm64 packages shadow x86_64),
#   provide a sysroot with the target's headers/libs instead:
#     export SYSROOT=/path/to/rpi5-sysroot
#
# Usage:
#   ./build.sh            # GENISYS_HAS_HAILO=OFF  (OSC + UI, no voice — for early testing)
#   ./build.sh --hailo    # GENISYS_HAS_HAILO=ON   (requires HailoRT SDK on the build host)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
NATIVE_BUILD="$BUILD_DIR/native"
CROSS_BUILD="$BUILD_DIR/aarch64-linux"
DIST_DIR="$BUILD_DIR/dist"

HAILO=OFF
if [[ "${1:-}" == "--hailo" ]]; then
    HAILO=ON
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
mkdir -p "$DIST_DIR"
echo ""
echo "=== Collecting binaries ==="
for BIN in GenisysBackend GenisysFrontend GenisysDebugConsole; do
    # JUCE places build artefacts under <target>_artefacts/<config>/
    SRC="$(find "$CROSS_BUILD" -type f -name "$BIN" ! -path "*/_deps/*" | head -1)"
    if [[ -z "$SRC" ]]; then
        echo "  WARNING: $BIN not found in $CROSS_BUILD"
    else
        cp "$SRC" "$DIST_DIR/$BIN"
        echo "  → dist/$BIN"
    fi
done

echo ""
echo "=== Done ==="
echo "  Run:  python deploy.py --target-ip <user>@<pi-ip>"
