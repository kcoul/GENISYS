# Install cross-toolchain and JUCE's Linux deps once:

  sudo apt install cmake ninja-build \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
      libfreetype-dev libfontconfig-dev \
      libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
      libasound2-dev

  ./build.sh

# Install cross-toolchain support libraries

  sudo dpkg --add-architecture arm64
  sudo apt update
  sudo apt install libasound2-dev:arm64 libfreetype-dev:arm64 libfontconfig1-dev:arm64 \
      libx11-dev:arm64 libxrandr-dev:arm64 libxinerama-dev:arm64 \
      libxcursor-dev:arm64 libxext-dev:arm64
      
# Configure apt sources-list

## Legacy approach (for machines on Ubuntu versions earlier than 24.04, or that were upgraded from earlier versions)



## Modern approach (for machines that use DEB-822)