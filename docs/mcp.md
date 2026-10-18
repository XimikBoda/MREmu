# Letting an AI agent drive MREmu

MREmu can run with no window at all and talk to an AI agent over the
[Model Context Protocol](https://modelcontextprotocol.io) — the same format agents use for
other tools. The agent starts the emulator and then asks it questions: *what is running,
what are the CPU registers, what does the screen look like, what broke?*

```
MREmu.exe <app.vxp> --mcp
```

This is what makes it usable from a script, over SSH, in CI, or by an agent. If you want to
watch it while it works, see [Watching what the agent is doing](#watching-what-the-agent-is-doing).

## Setting it up

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
emulator boots the app selector instead — exactly what the graphical emulator does — so
the agent starts with the app list visible and can launch a VXP from it with `emu_load_vxp`
or by driving the selector's own UI.

## Watching what the agent is doing

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

The window is fully interactive: clicking and typing in it reach the app through the same
path a normal click does, so you can take over from the agent at any point.

If you would rather not change the command line — an MCP client is usually configured once
with a fixed list of arguments — set `MREMU_MCP_WINDOW=1` in the server's environment
instead. It has the same effect.

The space below the phone screen in that window is the on-screen keyboard area, not more
screen. The emulated display is a fixed 240×320 that the app can read back.

## What the agent can do

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

## Two things that surprise people

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

## One register to be careful with

`r9` holds the MRE VM's pointer to its symbol table. Overwrite it and the app can no
longer find any system functions, and it will not recover. Read the current value first
and put it back afterwards.

## See also

- [Running without a window](../README.md#running-without-a-window) — `--inspect`, a one-shot
  JSON report for checking whether an app loads at all
- [Known issues](../README.md#known-issues) — including the `--steps` crash limit
