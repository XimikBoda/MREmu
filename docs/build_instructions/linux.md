# Building on Linux

## Dependencies

The project relies on several third-party libraries. Most dependencies are included directly as Git submodules (SFML, Unicorn, Capstone, libADLMIDI, libiconv-cmake), while others must be installed on your host system.

**System requirements:**
- CMake 3.8+
- A C++20 compatible compiler (GCC, Clang)
- System libraries required by SFML (X11, OpenGL, FreeType, OpenAL, etc.)
- [Git](https://git-scm.com/) for cloning the repo

### Prerequisites

Before building on Linux, you must install the necessary development packages for your Linux distribution.

#### Fedora
```bash
sudo dnf install cmake gcc-c++ make openal-soft-devel freetype-devel libX11-devel libXrandr-devel libXcursor-devel libXi-devel libudev-devel mesa-libGL-devel flac-devel libvorbis-devel libogg-devel
```

#### Debian / Ubuntu
```bash
sudo apt update
sudo apt install build-essential cmake libopenal-dev libfreetype-dev libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev libgl1-mesa-dev libflac-dev libvorbis-dev libogg-dev
```

#### Arch Linux
```bash
sudo pacman -S base-devel cmake openal freetype2 libx11 libxrandr libxcursor libxi systemd libgl flac libvorbis libogg
```

> If you find any of the above installation commands not working, i.e caused by mismatch of package names, please do suggest a fix by opening a PR!

## Build Instructions

**1. Clone the repository and initialize submodules**

If you haven't cloned the repository yet, make sure to clone recursively:
```bash
git clone --recursive https://github.com/XimikBoda/MREmu
cd MREmu
```

If you have already cloned the repository without submodules, initialize them now:
```bash
git submodule update --init --recursive
```

**2. Download the MediaTek SDK**

The emulator engine requires the MediaTek MRE SDK headers (`vm*.h`) to compile. You must download the SDK and **set the `MRE_SDK` environment variable.**

To download the official SDK installer and extract it using `innoextract`:
```bash
# Install innoextract (e.g. on Fedora use dnf, on Ubuntu use apt)
sudo dnf install innoextract 

mkdir -p ~/mre-sdk && cd ~/mre-sdk
wget https://github.com/raspiduino/mre-sdk/releases/download/1.0.0/MRE_SDK_3.0.00.20_Normal_Eng.zip
unzip MRE_SDK_3.0.00.20_Normal_Eng.zip
innoextract MRE_SDK_3.0.00.20_Normal_Eng.exe

# The headers will be extracted into the 'app' directory
# I suggest you to add this to your shell rc file 
# (e.g ~/.bashrc or ~/.zshrc)
export MRE_SDK=~/mre-sdk/app
cd -
```

**3. Configure and build the project**

Make sure the `MRE_SDK` environment variable is set in your current terminal session. Then, use CMake to configure the build environment and compile the project:
```bash
mkdir build
cd build
cmake ..
cmake --build . -j$(nproc)
```

**4. Run the emulator**

After a successful build, the executable will be placed in the `bin` directory at the project root.
```bash
cd ../bin
./MREmu
```
