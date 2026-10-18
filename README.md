# MREmu

MREmu is an emulator for [MediaTek MRE](https://lpcwiki.miraheze.org/wiki/MediaTek_MRE) (VXP) and VRE platforms.

## Documentation

Comprehensive technical documentation is available in the [**Documentation Index**](docs/index.md):

- [Architecture](docs/architecture.md) — System design, Unicorn CPU emulation (ARMv5TEJ), memory virtualization, and the C++ symbol bridge.
- [VXP & Binary Formats](docs/vxp_format.md) — VXP envelope structure, MRE metadata tags (`MreTags`), ADS/ELF loaders, and dynamic relocations.
- [MRE Runtime APIs](docs/mre_api.md) — Reference for emulated system, timer, memory, SIM, and standard library APIs.
- [Graphics Subsystem](docs/graphics.md) — 240×320 screen compositing, layer management, canvas memory buffers, and GNU Unifont font rendering.
- [Audio Subsystem](docs/audio.md) — OPL3 FM synthesis via `libADLMIDI` and PCM bitstream streaming.
- [File System & I/O](docs/io_and_filesystem.md) — Virtual drive mapping (`C:`, `D:`, `E:`), UCS-2 path translation, and directory iterators.
- [Networking & Sockets](docs/networking.md) — Asynchronous TCP/UDP sockets powered by Asio and DNS resolution.
- [Desktop & Headless Tooling](docs/desktop.md) — SFML/ImGui interface, CLI options, and GDB debugging.
- [Running Without a Window](docs/running_without_window.md) — Headless operation, script integration, one-shot reporting, and AI agent control.
- [AI Agent Control (MCP)](docs/mcp.md) — Driving MREmu headlessly via the Model Context Protocol.
- [Android Frontend](docs/android.md) — Hybrid NativeActivity architecture, Jetpack Compose overlay, and JNI bridge.
- [Native Applications](docs/native_apps.md) — Built-in `AppSelector` launcher menu, WWDC18 rubber-band scrolling, and `BottomSheet` dialogs.
- [Known Issues](docs/known_issues.md) — Known limitations, stepping crash boundaries, timer exhaustion, and bounds-checking caveats.

## How to build?

For platform-specific dependencies and build instructions, see:

- [Building on Linux](docs/build_instructions/linux.md)
- [Building on Windows](docs/build_instructions/windows.md)
- [Building on Android](docs/build_instructions/android.md)

## Credits for Dependencies

- [QEMU](https://github.com/qemu/qemu) - CPU emulation core reference
- [Unicorn](https://github.com/unicorn-engine/unicorn) - ARM CPU emulation engine
- [SFML](https://github.com/SFML/SFML) - Windowing, graphics, audio, networking
- [Dear ImGui](https://github.com/ocornut/imgui) - Debug UI overlays
- [ImGui-SFML](https://github.com/SFML/imgui-sfml) - ImGui SFML rendering backend
- [Capstone](https://github.com/capstone-engine/capstone) - Instruction disassembly engine
- [asio](https://github.com/chriskohlhoff/asio) - Asynchronous networking I/O
- [asio.cmake](https://github.com/OlivierLDff/asio.cmake) - CMake build integration for asio
- [ELFIO](https://github.com/serge1/ELFIO) - ELF file parsing
- [CmdParser](https://github.com/FlorianRappl/CmdParser) - Command-line argument parsing
- [libADLMIDI](https://github.com/Wohlstand/libADLMIDI) - OPL3 MIDI audio synthesis
- [libiconv-cmake](https://github.com/winlibs/libiconv-cmake) - Character set conversion
- [TurboStitchGIF](https://github.com/Ferki-git-creator/TurboStitchGIF-HeaderOnly-Fast-ZeroAllocation-PlatformIndependent-Embedded-C-GIF-Decoder) - GIF image decoding
- [miniz](https://github.com/richgel999/miniz) - ZIP compression and decompression

## Documentation and References

> These links might be relevant and handy for MRE development. (Also please don't forget to archive them on [Wayback Machine](https://web.archive.org/) if you're visiting them since we dont wan't these resources to be lost)

- [MediaTek MRE SDK (raspiduino/mre-sdk)](https://github.com/raspiduino/mre-sdk)
- [MediaTek MRE on Mobile Universe Wiki](https://mobileuniverse.miraheze.org/wiki/MediaTek_MRE)
- [MAUI Runtime Environment on Emulation General Wiki](https://emulation.gametechwiki.com/index.php/Cellphone_emulators#MAUI_Runtime_Environment)
- [MediaTek MRE on Low Power Computing Wiki](https://lpcwiki.miraheze.org/wiki/MediaTek_MRE)

... if you have more links, please help us expand this list by opening a PR!

## Support

Any questions? Join the Discord server (MRE Development): <https://discord.gg/vMEtkpnMU4>
