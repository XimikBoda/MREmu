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
MREmu.exe <app.vxp> --mcp
```

Instead of opening a window, MREmu starts up, loads the app, and then waits for
instructions over stdin/stdout in the Model Context Protocol format — the same format AI
agents use for other tools. That means an agent can start the emulator and then ask it
questions: *what is running, what are the CPU registers, what does the screen look like,
what broke?*

Nothing is displayed, so it works on a headless machine, over SSH, or in CI. If you want to
watch it as well, add `--mcp-window` — see below.

#### Setting it up

Point your agent at the executable and give it two arguments — the app to load, and
`--mcp`. In OpenCode that is a few lines in `opencode.jsonc`:

```jsonc
"mremu": {
  "type": "local",
  "command": [
    "C:\\path\\to\\MREmu.exe",
    "C:\\path\\to\\Opera Mini 6.1.vxp",
    "--mcp"
  ],
  "enabled": true
}
```

The same works for Claude Code and other MCP clients. If you leave the app path out, the
emulator starts empty and the agent can load apps itself.

#### Watching what the agent is doing

Because `--mcp` shows nothing, it can be hard to tell an agent's careful stepping apart
from a hang. Add `--mcp-window` and you get the emulator window back:

```
MREmu.exe <app.vxp> --mcp --mcp-window
```

Nothing about the protocol changes — the agent still connects over stdin/stdout and uses
exactly the same tools. You simply also get a window showing the emulated screen, with a
line of text underneath it saying whether the app is running or paused, how many VM calls
it has made, whether the call tracer is on, and whether anything faulted. The window title
shows the same thing, so the state is readable even when the window is behind something
else. Close the window and the whole thing shuts down.

If you would rather not change the command line — an MCP client is usually configured once
with a fixed list of arguments — set `MREMU_MCP_WINDOW=1` in the server's environment
instead. It has the same effect.

#### What the agent can do

| Tool | In plain terms |
| --- | --- |
| `emu_status` | "What am I running, is it healthy, where is it stuck?" |
| `emu_list_apps` | "What apps are loaded?" |
| `emu_load_vxp` | "Run this .vxp file." |
| `emu_close_app` | "Shut that app down." |
| `emu_regs` | "What are all the CPU registers right now?" |
| `emu_set_reg` | "Put this value in that register." |
| `emu_step` | "Run the app a bit and tell me what happened." |
| `emu_pause` | "Hold still" / "carry on." |
| `emu_screen` | "Show me what the phone screen looks like." |
| `emu_errors` | "Did anything crash, and if so where?" |
| `emu_trace_control` | "Start or stop recording every system call the app makes." |
| `emu_trace_dump` | "Show me the calls you recorded." |

The tracer is the interesting one. It records every call the app makes into the MRE
system — `vm_create_timer`, `vm_get_sym_entry`, `vm_malloc` and so on — with the call's
address, its arguments and what it returned. Turning it on before an app boots captures
the app's entire startup sequence, which is exactly what you want when working out why an
app will not start.

#### Two things that surprise people

**The app sits still most of the time, and that is normal.** These apps are event-driven.
They only wake up when something happens to them: a timer expires, or input arrives.
Between events they do nothing at all, so `emu_step` can report that it ran but nothing
happened. That is not a bug. To check, look at `timers_made_due` in `emu_status`: `0`
means the app had nothing scheduled, so there was genuinely nothing to run.

One consequence is worth knowing: if an app uses up its last timer and then stops, nothing
will ever wake it again, and it will sit there forever. Send it some input, or load it
again.

**There is no total instruction count.** Normal runs end when the app goes idle, and there
is no way to know from outside how many instructions that took. So the agent reports
`stepped_instructions`, which only counts instructions it asked to run in bounded chunks.
For "is this app actually doing anything?", `vm_calls` is the number to watch — it counts
every system call the app makes and is always accurate.

#### One register to be careful with

`r9` holds the MRE VM's pointer to its symbol table. Overwrite it and the app can no
longer find any system functions, and it will not recover. Read the current value first
and put it back afterwards.

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
