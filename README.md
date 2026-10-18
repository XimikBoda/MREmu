# MREmu

MREmu is an emulator for [MediaTek MRE](https://lpcwiki.miraheze.org/wiki/MediaTek_MRE) (VXP) and VRE platforms.

## How to build?

For platform-specific dependencies and build instructions, see:

- [Building on Linux](docs/build_instructions/linux.md)
- [Building on Windows](docs/build_instructions/windows.md)
- [Building on Android](docs/build_instructions/android.md)

## Running without a window

Normally MREmu opens a window and you click on it. It can also run with nothing on screen,
which is what makes it usable from a script, over SSH, in CI, or by an AI agent.

### `--inspect` — one-shot report

```
MREmu.exe <app.vxp> --json report.json --steps 2000 --screen shot.png --inspect
```

Loads the app, writes what it found (the MRE tags, the memory layout, the entry point) to
a JSON file, optionally runs a set number of instructions, and optionally saves a picture
of the screen. Then it exits. Useful for checking whether an app will even load.

One quirk of the command-line parser: put the `.vxp` path first, and write `--inspect`
on its own rather than `--inspect=1`.

### `--mcp` — let an AI agent drive the emulator

```
MREmu.exe <app.vxp> --mcp [--mcp-window]
```

Instead of opening a window, MREmu starts up, loads the app, and then waits for
instructions over stdin/stdout in the Model Context Protocol format — the same format AI
agents use for other tools. That means an agent can start the emulator and then ask it
questions: *what is running, what are the CPU registers, what does the screen look like,
what broke?*

Nothing is displayed, so it works on a headless machine, over SSH, or in CI. Add
`--mcp-window` to get the window back and watch the agent work.

For the full guide — client setup, every tool, and the behaviours that surprise people —
see [Letting an AI agent drive MREmu](docs/mcp.md).

## Known issues

### `--steps` above ~5,000 instructions still crashes

`--inspect --steps N` bounds a single `uc_emu_start` call to `N` instructions. Above
roughly 5,000 the VRE/ADS boot path faults (`0xC0000409` on Windows) partway through. This
is undiagnosed and unfixed.

Note that the MCP server's `emu_step` does **not** hit this: it deliberately caps each
VM callback at 4096 instructions and carries the remainder of the budget over to the app's
next callback, which runs indefinitely. `--inspect` still spends its whole limit in the
one invocation, because it is running the app's entry point directly and has no next
callback to defer to.

### An app that has spent its timers cannot be resumed

`Opera Mini 6.1.vxp` creates a single 10-second timer while booting and deletes it again.
Once the app has run that timer out and stopped, nothing re-enters it: the emulator only
calls into the app from a timer callback or an incoming event, so it stays parked at its
idle handler indefinitely. `emu_step` reports this honestly (`timers_made_due: 0` with a
non-zero `step_budget`) rather than appearing to work. To drive the app again, inject
input or reload the VXP.

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
