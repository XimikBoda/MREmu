# Desktop & Headless Tooling

This document describes the desktop interface, graphical debugger, command-line interface (CLI), and headless automation tools of **MREmu**.

---

## 1. Desktop Interface Architecture

On desktop platforms (Windows, Linux, and macOS), MREmu provides a dual-window interface built with **SFML** (Simple and Fast Multimedia Library) and **Dear ImGui**:

```text
+-----------------------------------+     +---------------------------------------------------+
|          MREmu Device             |     |                    MREmu Debug                    |
| +-------------------------------+ |     | +-----------------------------------------------+ |
| |                               | |     | | [CPU Registers]  R0: 0x00010000  SP: 0x000F8000 | |
| |      Phone Display Screen     | |     | |                  PC: 0x00008024  LR: 0x00008010 | |
| |         (240 x 320)           | |     | |                  CPSR: 0x00000010 (ARM User)    | |
| |                               | |     | +-----------------------------------------------+ |
| +-------------------------------+ |     | | [Layers] 0: (0,0) 240x320 | 1: (0,280) 240x40 | |
| | [1] [2] [3]       [^]         | |     | +-----------------------------------------------+ |
| | [4] [5] [6]    [<] [OK] [>]   | |     | | [Canvases] 0: 240x320 MTKCANVAS               | |
| | [7] [8] [9]       [v]         | |     | |            1: 320x16  MTKCANVAS (Scratch)     | |
| | [*] [0] [#]    [SND] [PWR]    | |     | +-----------------------------------------------+ |
| +-------------------------------+ |     | | [GDB Server] Port: 1234   State: Stopped      | |
+-----------------------------------+     +---------------------------------------------------+
```

### 1. Device Window

- **Emulated Screen**: Scaled presentation of the 240×320 LCD screen with aspect-ratio preserving scaling.
- **On-Screen Keypad**: Virtual keypad rendered beneath the screen (numeric pad on the left, directional D-pad and softkeys on the right).
- **Touch & Pen Translation**: Direct mouse clicks and drags on the screen area are forwarded to the emulated pen handler (`vm_reg_pen_callback`).
- **Drag-and-Drop**: Dropping a `.vxp` file directly onto the device window immediately loads and launches the application.

### 2. Debug Window (Dear ImGui)

- **CPU Registers Inspector** (`Cpu::imgui_REG`): Displays real-time values of `R0`–`R12`, `SP`, `LR`, `PC`, and `CPSR` conditional flags (Negative, Zero, Carry, Overflow).
- **Layers Inspector** (`imgui_layers`): Lists all active MRE layers, coordinates `(x, y)`, dimensions, transparent color keys, and clip rects.
- **Canvases Inspector** (`imgui_canvases`): Visualizes all memory-backed canvases allocated by the guest application.
- **Keypad Debugger** (`imgui_keyboard`): Real-time monitor of keypress states and input sources.

---

## 2. Command-Line Options (CLI)

MREmu features a versatile command-line interface parsed via `cmdparser`.

### CLI Syntax

```bash
MREmu.exe <app.vxp> [options]
```

### Complete Argument Reference

| Option | Shorthand | Type | Description |
| :--- | :--- | :--- | :--- |
| `<app.vxp>` | — | `string` | **Path to the VXP application**. Should always be provided as the first positional argument. |
| `--inspect` | `-i` | `flag` | Enables headless one-shot inspection mode (does not create an SFML window). |
| `--json` | `-j` | `string` | Specifies output path for the JSON metadata report in `--inspect` mode. |
| `--screen` | `-s` | `string` | Specifies output path for a PNG screenshot capture in `--inspect` mode. |
| `--steps` | `-n` | `int` | Number of instructions to execute during `--inspect` mode before exiting. |
| `--mcp` | `-m` | `flag` | Starts the Model Context Protocol (MCP) server over stdio for AI agent automation. |
| `--mcp-window` | `-w` | `flag` | Accompanies `--mcp` to open a visible device window so humans can observe the agent. |
| `--gdb` | `-g` | `flag` | Enables the built-in GDB remote debugging stub. |
| `--port` | `-p` | `int` | Specifies the TCP port for the GDB remote stub (default `1234`). |

---

## 3. Headless One-Shot Inspection (`--inspect`)

For CI/CD pipelines, automated testing, or quick verification without a display server:

```bash
MREmu.exe "Opera Mini 6.1.vxp" --inspect --json report.json --screen shot.png --steps 2000
```

### What It Does

1. Boots the emulator core without opening an X11, Wayland, or Win32 window.
2. Parses the `.vxp` envelope and extracts all `MreTags` (application name, author, required RAM, file type).
3. Allocates memory and unpacks code/data sections.
4. Executes the application entry point for the specified number of steps (e.g. 2,000 instructions).
5. Writes comprehensive JSON metadata to the designated file.
6. Saves an unscaled 240×320 PNG capture of the final screen buffer.
7. Exits cleanly with code `0`.

> [!NOTE]
> **Stepping Bound Caution**: As documented in [Known Issues](known_issues.md#--steps-above-5000-instructions-still-crashes), executing `--inspect --steps` with limits above ~5,000 instructions can fault during the ADS C-runtime initialization. Keep inspection steps modest or use `--mcp` for long-running stepping.

---

## 4. Parser Gotchas & Conventions

When invoking MREmu from scripts or terminals:

1. **Positional Argument First**: Always place the `.vxp` file path before any `--flags`.
2. **Exact Flag Names**: `cmdparser` matches flag names exactly. Declare `--mcp-window` without equals signs.
3. **Boolean Flags**: Boolean flags must be written on their own (e.g. `--inspect`, not `--inspect=1`), and should generally be placed last on the command line.

---

## 5. GDB Remote Debugging

MREmu includes an embedded GDB remote stub ([`GDB.cpp`](../MREmu/GDB.cpp)):

1. Launch MREmu with GDB enabled:

   ```bash
   MREmu.exe app.vxp -g -p 1234
   ```

2. Connect from an ARM cross-debugger:

   ```bash
   arm-none-eabi-gdb app.elf
   (gdb) target remote localhost:1234
   (gdb) info registers
   (gdb) stepi
   (gdb) continue
   ```

3. Breakpoints, memory inspection, and single-step execution are supported via the standard GDB remote serial protocol.
