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
#   If host and target libraries conflict (e.g. x86_64 vs aarch64 packages),
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

# ── Step 1: Native host build — produces juceaide only ───────────────────────
# JUCE needs juceaide (a code-generation tool) to run on the *build* machine.
# We configure the full project natively so CMake wires it up, then build only
# that one target before switching to the cross toolchain.
echo "=== Step 1: Build native juceaide ==="
cmake -S "$SCRIPT_DIR" -B "$NATIVE_BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGENISYS_HAS_HAILO=OFF \
    -G Ninja
cmake --build "$NATIVE_BUILD" --target juceaide

JUCEAIDE_DIR="$NATIVE_BUILD/JUCE/extras/Build/juceaide"
if [[ ! -x "$JUCEAIDE_DIR/juceaide" ]]; then
    echo "ERROR: juceaide not found at $JUCEAIDE_DIR/juceaide"
    echo "  The path changed — try: find $NATIVE_BUILD -name juceaide -type f"
    exit 1
fi
echo "  juceaide: $JUCEAIDE_DIR/juceaide"

# ── Step 2: Cross-compile all three GENISYS targets ──────────────────────────
echo ""
echo "=== Step 2: Cross-compile GENISYS for aarch64 (GENISYS_HAS_HAILO=$HAILO) ==="
cmake -S "$SCRIPT_DIR" -B "$CROSS_BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/cmake/toolchain-aarch64-linux.cmake" \
    -DJUCE_TOOL_INSTALL_DIR="$JUCEAIDE_DIR" \
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
