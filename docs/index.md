# MREmu Documentation

Welcome to the technical documentation for **MREmu**, a portable emulator for the **MediaTek MRE** (MAUI Runtime Environment / VXP) and **VRE** feature phone application platforms.

MREmu allows ARM-based feature phone applications compiled with ARM Developer Suite (ADS) or GNU GCC toolchains to execute on modern desktop operating systems (Windows, Linux, macOS) and mobile devices (Android) via instruction emulation, memory virtualization, and a re-implementation of the MediaTek MRE C runtime APIs.

---

## Documentation Overview

The documentation is organized into modular guides following a simple, topic-focused naming convention:

| Document | Topic & Scope |
| :--- | :--- |
| [**Architecture**](architecture.md) | High-level system design, Unicorn CPU emulation (ARMv5TEJ), virtual memory management, ADS/ELF binary loaders, and the host-to-guest C++ symbol bridge. |
| [**VXP & Binary Formats**](vxp_format.md) | Binary format specifications of `.vxp` packages, MRE metadata tags (`MreTags`), miniz/zlib decompression, section layouts (`ER_RO`, `ER_RW`, `ER_ZI`), and dynamic relocations. |
| [**MRE Runtime APIs**](mre_api.md) | Detailed reference of emulated MediaTek MRE C APIs: System, Memory management, Timers, Character set conversions, SIM, and Sensors. |
| [**Graphics Subsystem**](graphics.md) | 240×320 screen compositing, layer management, canvas memory buffers & signatures (`MTKCANVAS`), 2D drawing primitives, alpha blending, and GNU Unifont font rendering. |
| [**Audio Subsystem**](audio.md) | Audio architecture, OPL3 MIDI synthesis via `libADLMIDI`, bitstream PCM streaming (`Bitstream`), and audio recording emulation (`BitstreamRecord`). |
| [**File System & I/O**](io_and_filesystem.md) | Virtual drive mapping (`C:\`, `D:\`, `E:\mre`), host path sandboxing, UCS-2 string handling, file descriptors, and directory search iterators. |
| [**Networking & Sockets**](networking.md) | Asynchronous network stack powered by Asio: TCP/UDP socket lifecycle, event-driven callbacks, DNS resolution (`vm_soc_get_host_by_name`), and connectivity checks. |
| [**Desktop & Headless Tooling**](desktop.md) | Desktop application features, dual-window interface (SFML + Dear ImGui), headless inspection flags (`--inspect`, `--json`, `--screen`, `--steps`), and GDB debugging. |
| [**Running Without a Window**](running_without_window.md) | Headless operation, script integration, one-shot reporting, and AI agent control overview. |
| [**AI Agent Control (MCP)**](mcp.md) | Driving MREmu headlessly via the Model Context Protocol (MCP) over stdio with tools for execution control, memory inspection, and VM call tracing. |
| [**Android Frontend**](android.md) | Architecture of the Android port: `NativeActivity` integration, JNI bridge, Jetpack Compose UI overlay, virtual keypad, and Storage Access Framework (SAF). |
| [**Native Applications**](native_apps.md) | Embedded native C++ applications: the `AppSelector` launcher menu, WWDC18 rubber-band scrolling physics, and animated `BottomSheet` dialogs. |
| [**Known Issues**](known_issues.md) | Known limitations, stepping crash boundaries, timer exhaustion, concurrent enumeration issues, and bounds-checking caveats. |

---

## Build Guides

For step-by-step compilation guides on supported host platforms, refer to the build instructions:

- [Building on Windows](build_instructions/windows.md) (Visual Studio 2019+, MSVC, CMake)
- [Building on Linux](build_instructions/linux.md) (GCC/Clang, CMake, system libraries)
- [Building on Android](build_instructions/android.md) (Android Studio, NDK 27, Gradle, CMake)

---

## Quick Navigation by Role

### For Developers & Contributors

- Read [Architecture](architecture.md) to understand the interaction between Unicorn, memory layouts, and the API bridge.
- Check [MRE Runtime APIs](mre_api.md) and [Graphics](graphics.md) when implementing or fixing emulated system calls.
- Inspect the [Repository Guide](../AGENTS.md) for build rules, conventions, and ABI settings.

### For Reverse Engineers & VXP Analyzers

- Read [VXP & Binary Formats](vxp_format.md) to examine how VXP headers, tags, sections, and relocations are parsed.
- Use the headless inspection mode described in [Desktop & Headless Tooling](desktop.md) (`--inspect --json out.json`) to quickly extract app tags and memory footprints.
- Leverage the [MCP Server](mcp.md) to automate reverse engineering pipelines and capture dynamic API traces.
- Use the Python helper tools documented in [docs/tools/](tools/vxpinfo.md): `vxpinfo`, `vxpicon`, `vxpstrings`, `vxpsections`, `vxpresources`, and `vxpdiff`.

### For Android & Frontend Developers

- Consult [Android Frontend](android.md) for details on the hybrid Kotlin Compose and C++ `NativeActivity` architecture, JNI bindings, and virtual keypad mapping.
- Review [Native Applications](native_apps.md) for the internal launcher menu and UI widgets.
