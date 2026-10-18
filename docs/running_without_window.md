# Running Without a Window

Normally MREmu opens a window and you click on it. It can also run with nothing on screen, which is what makes it usable from a script, over SSH, in CI, or by an AI agent.

---

## `--inspect` — One-Shot Report

```bash
MREmu.exe <app.vxp> --json report.json --steps 2000 --screen shot.png --inspect
```

Loads the app, writes what it found (the MRE tags, the memory layout, the entry point) to a JSON file, optionally runs a set number of instructions, and optionally saves a picture of the screen. Then it exits. Useful for checking whether an app will even load.

One quirk of the command-line parser: put the `.vxp` path first, and write `--inspect` on its own rather than `--inspect=1`.

For more details on CLI options and output format, see [Desktop & Headless Tooling](desktop.md).

---

## `--mcp` — Let an AI Agent Drive the Emulator

```bash
MREmu.exe <app.vxp> --mcp [--mcp-window]
```

Instead of opening a window, MREmu starts up, loads the app, and then waits for instructions over stdin/stdout in the Model Context Protocol format — the same format AI agents use for other tools. That means an agent can start the emulator and then ask it questions: *what is running, what are the CPU registers, what does the screen look like, what broke?*

Nothing is displayed, so it works on a headless machine, over SSH, or in CI. Add `--mcp-window` to get the window back and watch the agent work.

For the full guide — client setup, every tool, and the behaviours that surprise people — see [Letting an AI Agent Drive MREmu](mcp.md).
