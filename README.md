# Install cross-toolchain and JUCE's Linux deps once:

  sudo apt install cmake ninja-build \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
      libfreetype-dev libfontconfig-dev \
      libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
      libasound2-dev

  ./build.sh
