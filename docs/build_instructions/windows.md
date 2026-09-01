# Building on Windows

## Dependencies

The project relies on several third-party libraries. Most dependencies are included directly as Git submodules (SFML, Unicorn, Capstone, libADLMIDI, libiconv-cmake), while others must be installed on your host system.

**System requirements:**
- CMake 3.8+
- A C++20 compatible compiler (MSVC)
- System libraries required by SFML (OpenGL, OpenAL, etc.)
- [Git](https://git-scm.com/) for cloning the repo

### Prerequisites

- [Visual Studio](https://visualstudio.microsoft.com/downloads/) 2019 or newer with the "Desktop development with C++" workload
- [CMake](https://cmake.org/download/)
- [OpenAL 1.1 Windows Installer](https://www.openal.org/downloads/)
- [MediaTek MRE SDK 3.0](https://github.com/raspiduino/mre-sdk/releases/download/1.0.0/MRE_SDK_3.0.00.20_Normal_Eng.zip)

## Build Instructions

**1. Clone the repository along with submodules**

If you haven't cloned the repository yet, make sure to clone recursively:
```bash
git clone --recursive https://github.com/XimikBoda/MREmu
cd MREmu
```

If you have already cloned the repository without submodules, initialize them now:
```bash
git submodule update --init --recursive
```

**2. Download and setup MediaTek SDK**

The emulator engine requires the MediaTek MRE SDK headers (`vm*.h`) to compile. You must download the SDK and set the `MRE_SDK` environment variable.

1. Download [MRE SDK 3.0](https://github.com/raspiduino/mre-sdk/releases/download/1.0.0/MRE_SDK_3.0.00.20_Normal_Eng.zip) and install it to `C:\MRE_SDK`.
2. Then just set the `MRE_SDK` environment variable pointing to the SDK directory and make sure the specified directory contains an `include` folder. 
<br>
<br>
(Win + R opens Run dialog > `sysdm.cpl` > Advanced > Enviornment Variables > Then under System Variables click New... > Set variable name to `MRE_SDK` and value to `C:\MRE_SDK` )

**3. Generate Visual Studio Solution**

A. Using CMake GUI:
1. Open CMake GUI.
2. Set the source code path to the repository directory.
3. Set the build binaries path to the `build` directory inside the repository.
4. Click **Configure**, select your Visual Studio version, and choose `Win32` as the platform.
5. Click **Generate**, then click **Open Project** to open the solution in Visual Studio.
6. Build the solution in Visual Studio (Release or Debug).

B. Using command line:
```cmd
mkdir build
cd build
cmake .. -A Win32
cmake --build . --config Release
```

**4. Run the emulator**

If the build succeeds, you'll see the executable inside `bin` directory located at the project root.
```cmd
bin\MREmu.exe
```
