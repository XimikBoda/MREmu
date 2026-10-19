# Fixed Case: Facebook Messenger Boot Crash & Dynamic Array Incompatibility

## The short version

`Messenger.vxp` (internal name `Messenger`) crashed on boot with:
1. Initially at VM call 147: `[Messenger] [error] uc_emu_start returned 7 (Invalid memory write (UC_ERR_WRITE_UNMAPPED)) at PC=0x00000DD4`.
2. Once the first crash was mitigated, at VM call 259: `[Messenger] [error] uc_emu_start returned 6 (Invalid memory read (UC_ERR_READ_UNMAPPED)) at PC=0x000025F4`.

The primary cause was twofold:
- **Font & Image Metrics**: `vm_graphic_get_highest_char_height_of_all_language` was not exported in `Bridge.cpp`, and `vm_graphic_get_img_property_FIX` ignored `frame_index` and canvas signatures. The app's layout engine received 0 for font height and computed an out-of-bounds component offset `y = 640` on a 240x320 display. The layout allocator `0x0C18` returned error `-8`, which was blindly dereferenced at `0x0DD4`.
- **Dynamic Array ABI Mismatch**: In [`MREmu/MREngine/STDLib.cpp`](../../MREmu/MREngine/STDLib.cpp), `vm_dyn_array_t` was compiled with the host's 64-bit ABI instead of 32-bit ARM layout. ARM guest code directly reads `array->ptr` at offset `+0x14`, which under 64-bit alignment landed in padding bytes and stored 64-bit host pointers instead of 32-bit emulated addresses (`ADDRESS_TO_EMU`).

Implementing the missing font metrics API, fixing `vm_graphic_get_img_property_FIX`, and adopting a packed 32-bit `vm_dyn_array_t_emu` struct completely resolves the crash. Facebook Messenger now boots cleanly to the interactive login screen.

---

## Symptoms & Trace

### Crash 1: Fatal UI Bounds Rejection (VM Call 147)
- Crash state:
  ```
  R0=0xFFFFFFF8  R1=0xE5982008  R2=0x00000000  R3=0x00000000
  R4=0x00077744  R5=0x00078580  R6=0x00000280  R7=0x0006EF18
  R8=0x0006EF74  R9=0x0006C75C R10=0x000143C0 R11=0x0000006C
  SP=0x00A1FF60  LR=0x00014900  PC=0x00000DD4
  ```
- Disassembly leading to crash:
  ```arm
  0x148ec: mov r3, r6       ; r6 = 0x280 (640)
  0x148f0: bl #0xc18        ; bounds-checked UI element allocator
  0x148f4: str r0, [r5, #0x10]
  0x148f8: str r1, [r5, #0x14]
  0x148fc: bl #0xdd4
  ...
  0x0dd4: ldr r1, [r0, #0x74]  ; r0 = -8, reads [-8 + 0x74] = 0x6C (unmapped!)
  0x0ddc: mov r2, #0
  0x0de0: str r2, [r1, #0x18]  ; FAULT: UC_ERR_WRITE_UNMAPPED
  ```
- `0x0C18` compares `y <= screen_height` (320). Because `y = 640 > 320`, `0x0C18` returned error codes `r0 = -8` (`0xFFFFFFF8`) and `r1 = -1`. The caller blindly dereferenced `r0 = -8`.

### Crash 2: Unmapped Dynamic Array Pointer (VM Call 259)
- Crash state:
  ```
  R0=0x040785A0  R1=0x00000007  R2=0x00000000  R3=0x000000F0
  R4=0x0006C7D0  R5=0x0007927C  R6=0x00000000  R7=0x00000000
  SP=0x00A1FFA0  LR=0x000636A4  PC=0x000025F4
  ```
- Disassembly:
  ```arm
  0x2700: ldr r0, [r5, #0x50] ; r0 = array pointer (vm_dyn_array_t*)
  0x2704: ldr r1, [r0, #0xc]  ; r1 = array->count (offset 0x0C)
  0x2708: cmp r1, r6          ; loop over count
  0x270c: bgt #0x25f4
  ...
  0x25f4: ldr r0, [r0, #0x14] ; r0 = array->ptr (offset 0x14)
  0x25f8: ldr r4, [r0, r6, lsl #2] ; FAULT: UC_ERR_READ_UNMAPPED
  ```

---

## Root Causes

1. **Missing `vm_graphic_get_highest_char_height_of_all_language`**:
   The app queried this symbol 9 times during UI font metric setup via `vm_get_sym_entry`. The symbol was missing in [`MREmu/Bridge.cpp`](../../MREmu/Bridge.cpp), causing font heights to default to 0. Subsequent centering and margin computations in `0x14770` produced `y = 640`.
2. **`vm_graphic_get_img_property_FIX` Flaws**:
   - Hardcoded `(cs + 1)` (frame 0) instead of calling `get_canvas_frame_by_ind(cs, frame_index)`.
   - Ignored canvas signature resolution (`find_canvas_signature`).
   - Doubled delay time by multiplying an already millisecond-converted delay by 10.
3. **`vm_dyn_array_t` Host/Guest Struct Layout Discrepancy**:
   Under 32-bit ARM, `vm_dyn_array_t` layout is:
   - `0x00`: `block_size` (4 bytes)
   - `0x04`: `init_size` (4 bytes)
   - `0x08`: `inc_size` (4 bytes)
   - `0x0C`: `count` (4 bytes)
   - `0x10`: `capacity` (4 bytes)
   - `0x14`: `ptr` (4 bytes)
   
   On a 64-bit host, `void* ptr` requires 8-byte alignment, inserting 4 bytes of padding at `0x14` and placing `ptr` at `0x18`. When ARM code loaded `[r0, #0x14]`, it loaded the padding bytes. Furthermore, `array->ptr` stored host pointers directly rather than emulated guest pointers (`ADDRESS_TO_EMU`). Additionally, `vm_dyn_array_add` reallocated `new_size` bytes instead of `new_size * block_size`, and `vm_dyn_array_del` reversed `memmove` parameters without decrementing `count`.

---

## The Fix

### 1. Font Metrics in [`MREmu/MREngine/Textout.cpp`](../../MREmu/MREngine/Textout.cpp) & [`MREmu/Bridge.cpp`](../../MREmu/Bridge.cpp)
```cpp
VMINT vm_graphic_get_highest_char_height_of_all_language(void) {
	return vm_graphic_get_character_height();
}

VMINT vm_graphic_get_char_height_alllang(VMINT size) {
	return vm_graphic_get_character_height();
}

VMINT vm_graphic_get_character_height_ex(VMUWCHAR c) {
	return vm_graphic_get_character_height();
}
```
Registered all three in `Bridge.cpp`.

### 2. Canvas Frame Property Lookup in [`MREmu/MREngine/Image.cpp`](../../MREmu/MREngine/Image.cpp)
```cpp
struct frame_prop* vm_graphic_get_img_property_FIX(VMINT_CANVAS hcanvas, VMUINT8 frame_index) {
	if (hcanvas == 0)
		return 0;

	static struct frame_prop* info = (frame_prop*)Memory::shared_malloc(sizeof(frame_prop));

	MREngine::canvas_signature* cs = find_canvas_signature((VMUINT8*)hcanvas);
	if (!cs)
		return 0;
	MREngine::canvas_frame_property* cfp_dst = get_canvas_frame_by_ind(cs, frame_index);
	if (!cfp_dst)
		return 0;

	info->flag = cfp_dst->flag;
	info->left = cfp_dst->left;
	info->top = cfp_dst->top;
	info->width = cfp_dst->width;
	info->height = cfp_dst->height;
	info->delay_time = cfp_dst->delay;
	info->trans_color_index = cfp_dst->trans_color_index;
	info->trans_color = (VMUINT16)cfp_dst->trans_color;
	info->offset = cfp_dst->offset;

	return info;
}
```

### 3. Guest-Aligned 32-Bit DynArray in [`MREmu/MREngine/STDLib.cpp`](../../MREmu/MREngine/STDLib.cpp)
```cpp
#pragma pack(push, 1)
struct vm_dyn_array_t_emu {
	VMINT block_size;
	VMINT init_size;
	VMINT inc_size;
	VMINT count;
	VMINT capacity;
	uint32_t ptr; // Emulated 32-bit guest pointer
};
#pragma pack(pop)
```
Updated `vm_create_dyn_array`, `vm_dyn_array_add`, `vm_dyn_array_del`, and `vm_free_dyn_array` to translate addresses using `ADDRESS_TO_EMU` / `ADDRESS_FROM_EMU` and enforce correct bounds and byte size scaling (`capacity * block_size`).

---

## Verification

1. **Boot Health**:
   - `Messenger.vxp` boots cleanly without faults (`faulted: false`, `last_error: ""`).
   - Reaches steady state in the event loop at 449+ VM calls (`run_state: "running"`, `pc: "0x00003978"`).
2. **Visual Presentation**:
   - `emu_screen` captured the Facebook Messenger login interface:
     - Blue messenger logo with lightning bolt.
     - "E-mail or phone number" and "Password" input boxes.
     - "Login" and "Exit" bottom softkeys.
3. **Regression Testing**:
   - Running `test_mcp.py` passes all 23 end-to-end MCP tests.
