# MREmu

MREmu is an emulator for [MediaTek MRE](https://lpcwiki.miraheze.org/wiki/MediaTek_MRE) (VXP) and VRE platforms.

## How to build?

For platform-specific dependencies and build instructions, see:

- [Building on Linux](docs/build_instructions/linux.md)
- [Building on Windows](docs/build_instructions/windows.md)
- [Building on Android](docs/build_instructions/android.md)

## Headless operation

The desktop build can run without a display, which is what makes it scriptable by an
AI agent or usable over SSH/CI.

### `--inspect` — one-shot report

```
MREmu.exe <app.vxp> --json report.json --steps 2000 --screen shot.png --inspect
```

Loads the app, reports its parsed MRE tags, layout and entry point, optionally runs a
bounded number of instructions, and optionally writes the emulated screen to a PNG.

Note: cmdparser keys `get()` on the **short** name, and a bare boolean flag must be last
or followed by another flag. So `--inspect` (not `--inspect=1`), and the positional VXP
path must come before the flags.

### `--mcp` — MCP server over stdio

```
MREmu.exe <app.vxp> --mcp
```

Speaks the Model Context Protocol over stdin/stdout, so an agent can launch the emulator
as an MCP server and then observe and drive it. No window is opened, so it works on a
machine with no display. To register it with an MCP client (OpenCode, Claude Code, etc.),
point the server command at `MREmu.exe` with the arguments `["<app.vxp>", "--mcp"]`.

Tools exposed:

| Tool | What it does |
| --- | --- |
| `emu_status` | Running app, `run_state`, VM call count, stepped instructions, PC, screen size, last error, `faulted` |
| `emu_list_apps` | Every loaded app with index, name and state |
| `emu_load_vxp` | Load a VXP by absolute path |
| `emu_close_app` | Close a loaded app by index |
| `emu_regs` | All 16 registers plus CPSR and decoded flags |
| `emu_set_reg` | Write one register by name (`r0`-`r12`, `sp`, `lr`, `pc`, `cpsr`) |
| `emu_step` | Run at most N instructions, then yield |
| `emu_pause` | Pause or resume |
| `emu_screen` | The emulated screen as a PNG |
| `emu_errors` | Errors recorded since the last clear |
| `emu_trace_control` | Enable / disable / clear / query the VM API call tracer |
| `emu_trace_dump` | Recorded VM API calls: name, PC, arguments, return value |

Two things about the model worth knowing before driving it:

- **The app is event-driven, not free-running.** It only executes inside a slice entered
  by a timer callback or an incoming event, so it spends most of its time parked at its
  idle handler. `emu_step` handles this by first making the app's pending timers due, and
  reports `timers_made_due` so you can tell "stepped and there was nothing scheduled" from
  "the step did not take effect". Once the app has spent its last timer, nothing can drive
  it and a step's budget stays pending; inject input or reload the app.
- **Instruction counts are not reported in total.** An unbounded slice runs to the app's
  idle handler and its length is not knowable from outside, so only `stepped_instructions`
  (instructions spent by bounded slices) is reported. `vm_calls` is the always-on activity
  metric.

`r9` is the MRE VM's DAT base pointer. Writing to it breaks the app's lazy `vm_*` symbol
resolution permanently, so read the original value before writing and restore it after.

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
