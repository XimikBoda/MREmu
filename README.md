# MREmu
MediaTek MRE emulator

## Setting up
### Linux
Clone the repository and open it in a terminal. Run these commands:
```
git submodule init
git submodule update
sudo apt install libvorbis-dev libflac-dev
mkdir build && cd build
```
Download the MRE SDK and extract it somewhere. Set the MRE_SDK environment variable to the folder that contains the include folder, like shown below. Run these commands:
```
export MRE_SDK=/path/to/mresdk
cmake ..
make
```