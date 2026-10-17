# MREmu

MREmu is an emulator for [MediaTek MRE](https://lpcwiki.miraheze.org/wiki/MediaTek_MRE) (VXP) and VRE platforms.

## How to build?

For platform-specific dependencies and build instructions, see:

- [Building on Linux](docs/build_instructions/linux.md)
- [Building on Windows](docs/build_instructions/windows.md)
- [Building on Android](docs/build_instructions/android.md)

## Known issues

### VXP offsets are not bounds-checked

`MREmu/ArmApp.cpp` and `MREmu/MreTags.cpp` use offsets and sizes read straight out of
the `.vxp` file without validating them against the file's actual size. A malformed or
deliberately hostile VXP can therefore cause out-of-bounds reads and writes when it is
loaded. On Android this is reachable without any special setup: `MainActivity` registers
an `ACTION_VIEW` filter for `content://` URIs matching `.*\.vxp`, so opening such a file
from any file manager loads it.

This is a known, unfixed defect. Treat VXP files from untrusted sources as unsafe, and do
not expect the emulator to survive a malformed one. The fix is to validate every
`offset + size` against `file_context.size()` before each dereference and `memcpy`.

### Release APKs are signed with the debug key

`Android/app/build.gradle` sets `signingConfig signingConfigs.debug` for the release
build type, and `~/.android/debug.keystore` is auto-generated per machine with the
publicly documented password `android`. Two consequences:

- The APKs are for **sideloading only**. They cannot be published to the Play Store, and
  installing one over an existing install signed by a different machine's debug key fails
  with `INSTALL_FAILED_UPDATE_INCOMPATIBLE`.
- Because that key is not secret, anyone can build an APK with the same `applicationId`
  and a signature that `adb install -r` will accept as an upgrade of the genuine app. Use
  the build for personal testing only.

## Credits for Dependencies
- [QEMU](https://github.com/qemu/qemu)
- [Unicorn](https://github.com/unicorn-engine/unicorn)
- [SFML](https://github.com/SFML/SFML)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [ImGui-SFML](https://github.com/SFML/imgui-sfml)
- [Capstone](https://github.com/capstone-engine/capstone)
- [asio](https://github.com/chriskohlhoff/asio)
- [asio.cmake](https://github.com/OlivierLDff/asio.cmake)
- [ELFIO](https://github.com/serge1/ELFIO)
- [CmdParser](https://github.com/FlorianRappl/CmdParser)
- [libADLMIDI](https://github.com/Wohlstand/libADLMIDI)
- [libiconv-cmake](https://github.com/winlibs/libiconv-cmake)
- [TurboStitchGIF](https://github.com/Ferki-git-creator/TurboStitchGIF-HeaderOnly-Fast-ZeroAllocation-PlatformIndependent-Embedded-C-GIF-Decoder)
- [miniz](https://github.com/richgel999/miniz)

## Documentation and References

> These links might be relevant and handy for MRE development. (Also please don't forget to archive them on [Wayback Machine](https://web.archive.org/) if you're visiting them since we dont wan't these resources to be lost)

- https://github.com/raspiduino/mre-sdk
- https://mobileuniverse.miraheze.org/wiki/MediaTek_MRE
- https://emulation.gametechwiki.com/index.php/Cellphone_emulators#MAUI_Runtime_Environment
- https://lpcwiki.miraheze.org/wiki/MediaTek_MRE


... if you have more links, please help us expand this list by opening a PR!

## Support
Any questions? Join the Discord server (MRE Development): https://discord.gg/vMEtkpnMU4
