# Fixed Case: The Amazing Spider-Man (Demo) Crashes with Unmapped Memory Read

## The short version

`Spiderman4.vxp` (internal name `Spiderman4`) crashed almost immediately on boot at VM call 360 with:
`[Spiderman4] [error] uc_emu_start returned 6 (Invalid memory read (UC_ERR_READ_UNMAPPED)) at PC=0x1580`.

The root cause was that `vm_resource_get_data` in [`MREmu/MREngine/Resources.cpp`](../../MREmu/MREngine/Resources.cpp) treated the caller-provided `offset` as an absolute file offset rather than taking `local_offsets` into account. In compressed ADS packages, `vm_res_offset` is non-zero and resource offsets read from the `mre-2.0` index table are relative to the start of the resource block. As a result, `vm_resource_get_data` copied raw compressed zlib code bytes into the sprite/data buffer instead of genuine resource payloads. The game's sprite parser subsequently computed invalid negative offsets and faulted.

Translating `offset += resources.vm_res_offset` when `resources.local_offsets` is set fixes the issue completely. The game now boots smoothly past the Gameloft splash screen to the interactive language selection menu.

## Symptoms & Trace

- At boot, the game loaded its ADS descriptor (`is_ads=True`, `is_zipped=True`, `vm_res_offset=0x1565A`).
- At VM call 360, Unicorn crashed with:
  ```
  R0=0xFFFF0128  R1=0xFFFF8094   R2=0x00000000   R3=0x00000000
  R4=0x0002DFB0  R5=0x00A1FFBC   R6=0x0002A6EC   R7=0x0002A707
  R8=0x00000000  R9=0x00029CA8  R10=0x0002D204  R11=0x00000000
  R12=0xFFFFFFFB SP=0x00A1FFA8   LR=0x00001C3C   PC=0x00001580
  ```
- Disassembly at `0x1580`:
  ```arm
  0x1580: ldr r3, [r0, #0x20]
  0x1584: ldr r0, [r0, #0x44]
  0x1588: add r1, r3, r1, lsl #1
  0x158C: ldrsh r1, [r1]
  0x1590: add r1, r1, r2
  0x1594: add r0, r0, r1, lsl #1
  0x1598: ldrsh r0, [r0]
  ```
- The function loaded `r0 = [r0 + 0x44]`, then loaded signed 16-bit word `[r1]` which returned `0xFFFF8094` (the first 2 bytes of the binary, because `r1` computed to 0), and then attempted `ldrsh r0, [0xFFFF0128]` which unmapped memory faulted.

## Root Cause

In compressed ADS packages:
1. The app calls `vm_load_resource("mre-2.0", &size)` to acquire the resource catalog.
2. The catalog provides entry offsets (e.g., `0x1C5D`, `0x154D7`, `0x2262B`). These are offsets relative to the start of the resource section (`vm_res_offset = 0x1565A`).
3. The app allocates memory and calls `vm_resource_get_data(buf, offset, size)`.
4. While `Resources::scan()` and `Resources::scan_mre2_0()` correctly applied `if (local_offsets) res_offset += vm_res_offset;`, `vm_resource_get_data` omitted this translation:
   ```cpp
   // BEFORE (buggy):
   memcpy(data, resources.file_context->data() + offset, size);
   ```
   This copied bytes from `file_context->data() + 0x1C5D` (which is inside the compressed ARM code payload) instead of `file_context->data() + 0x1565A + 0x1C5D`.

## The Fix

In [`MREmu/MREngine/Resources.cpp`](../../MREmu/MREngine/Resources.cpp):

```cpp
VMINT32 vm_resource_get_data(VMUINT8* data, VMUINT32 offset, VMUINT32 size) {
	MREngine::Resources& resources = get_current_app_resources();
	if (data == 0)
		return -1;

	if (resources.local_offsets)
		offset += resources.vm_res_offset;

	if (offset + size > resources.file_context->size() || offset + size < offset)
		return -1;

	memcpy(data, resources.file_context->data() + offset, size);
	return 0;
}
```

## Verification

| Check | Result |
|---|---|
| Crash at call 360 | Eliminated |
| Gameloft splash screen | Renders crisply at 3s |
| Language selection screen | Reached at ~7s with full menu options (English, French, German, Italian, Spanish, Russian, Brazilian) |
| VM calls at 7s | Over 9,300 VM calls executed; continuous game loop running (`vm_graphic_blt`, `vm_graphic_fill_rect`, `vm_graphic_flush_layer`, `vm_create_timer_ex`) |
| Fault status | `faulted: false`, `last_error: ""` |
| MCP Test Suite (`test_mcp.py`) | All checks passed (`ALL CHECKS PASSED`) |

## Input Navigation & Gameplay Flow

1. **Language Selection Screen**:
   - Use Up/Down arrows to highlight a language, then press **OK** (`Enter` / `Space` / `VM_KEY_OK`).
2. **Sound Prompt ("DO YOU WANT SOUND? YES / NO")**:
   - This prompt requires **Left Softkey** (`VM_KEY_LEFT_SOFTKEY`, mapped to `Q`, `F1`, `/`, or the `Left S` button in the ImGui virtual keypad) to confirm "SELECT".
   - Pressing **OK** on this dialog does nothing because Gameloft's dialog routine specifically listens for Left Softkey / Right Softkey rather than OK.
   - Touch/mouse clicks on the screen are ignored because Spider-Man's registered pen callback is a no-op stub (`bx lr`).
3. **Title Splash Screen**:
   - Displays "PRESS 5 TO CONTINUE". Press `5` (or NumPad `5`) to advance to the main menu.
4. **Frame Rate / Timing**:
   - Spider-Man drives its main loop via `vm_create_timer_ex` with a 57 ms interval (~17.5 FPS), typical of 2014 MediaTek feature phone titles. This is the intended game tick rate, not a performance regression or bottleneck.

