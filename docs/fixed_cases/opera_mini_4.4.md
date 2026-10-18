# Fixed Case: Opera Mini 4.4 Boots Then Quits

## The short version

Opera Mini 4.4 started up, then killed itself with `vm_exit_app`. The cause was
one wrong number: the emulator answered `8` to `vm_get_res_header()` when the
right answer is `0`. The app used that number to skip 8 bytes at the start of
every resource it read, which put it 8 bytes off. Its PNG check then failed, so
it quit. Changing the answer to `0` fixes the boot.

## What happened

- Both `Opera mini 4.4.vxp` and `Opera Mini 4.4 leatest en.vxp` failed the same way.
- `vm_exit_app` was call number 901 of 902 (the second VXP: 908).
- The app never reached the UI.

## Why it happened

`vm_get_res_header()` is a function the emulator gives apps. It answers:
"how many bytes of MRE bookkeeping sit at the start of a resource buffer, so
the app can skip them?" This emulator hands resources out exactly as stored in
the file (nothing is prefixed), so the answer should be `0`. The code had
`return 8;` (a guess).

Opera Mini 4.4 reads a resource like this:

    buf = vm_malloc(size);                // size = 284 bytes
    vm_resource_get_data(buf, offset, size);  // copies 284 file bytes into buf
    return buf + vm_get_res_header();      // <-- adds 8

So the app pointed 8 bytes into the middle of its own data. Its PNG signature
check then ran on `00 00 00 0D 49 48 44 52` (a valid PNG with its 8-byte
signature stripped), the format detector returned "unknown", the
`VM_MSG_CREATE` handler returned `-1`, the SDK tore the app down, and the app
called `vm_exit_app`.

## How it was found

- Traced the failure to gate 9 at `0xA76A` (after `bl 0x4B3E4`), inside the
  `VM_MSG_CREATE` handler, which returns `-1`.
- Walked the helper chain `0x24824 -> 0x248A0 -> 0x40010 -> 0x4ADA8`:
  `0x4ADE4` is the image-format detector on `buf[0]`
  (`'R'`/`'C'`/`0x89`/`0xFF`/`'P'`/else), `0x4ADA8` dispatches by format and
  returns 3 = "unknown".
- The parse buffer was `00 00 00 0D 49 48 44 52 ...` = a valid PNG missing its
  8-byte signature. `vm_resource_get_data` served the right bytes
  (`png_sig_at = 0`), so the skew was on the app side.
- The app read 284 bytes into `buf` and used `buf + 8` (the 282-byte payload):
  284 = the directory span, 282 = the PNG length.
- Traced the app's resource getter (`0x62F78`): it returns
  `buf + vm_get_res_header()`.
- Setting `vm_get_res_header()` to `0` makes 4.4 boot.

## The fix

```cpp
// MREmu/MREngine/Resources.cpp
VMINT vm_get_res_header() {
    // Bytes of MRE bookkeeping an app has to skip at the start of a resource
    // buffer. The engine hands resources out verbatim - vm_resource_get_data()
    // copies the member to data[0] and vm_load_resource() returns the data
    // pointer - so the offset is 0. Opera Mini 4.4 reads through
    // `ptr + vm_get_res_header()`; answering 8 shifted every resource it loaded
    // by 8 bytes, so its PNG signature check ran on the middle of the file,
    // failed, and the app quit itself during startup.
    return 0;
}
```

Two related, pre-existing hardening edits shipped alongside it:

- `MREmu/AppManager.cpp` `get_tags_by_mem_adr()`: match apps by a non-empty tag
  table rather than by `offset_mem == 0`, so a lookup for address 0 returns the
  running VXP app's tags instead of the native menu's empty table.
- `MREmu/MREngine/IO.cpp` `vm_get_vm_tag()`: initialise `adr` to `0` before
  `sscanf` so a malformed `@:` path can't become a wild host pointer.

## Did it work?

| Check | Result |
|---|---|
| `Opera mini 4.4.vxp` | boots to the EULA welcome page; `vm_exit_app` calls **0** (was 1 at call 901); ~9.5-10.3k VM calls, 36+ `vm_graphic_flush_layer` |
| `Opera Mini 4.4 leatest en.vxp` | same; screenshot byte-identical |
| `Opera Mini 6.1.vxp` | unchanged - renders Top Sites (it never calls this API) |
| `test_mcp.py` / `test_mcp_window.py` | ALL CHECKS PASSED |
| 19-app library sweep | no new failures; the two faults (`Messenger`, `Spider Man`) are pre-existing |
| `Spider Man` (the only other caller of `vm_get_res_header`) | faulted at the same PC with both `0` and `8` - a pre-existing fault, not a regression |

## Related files

- `MREmu/MREngine/Resources.cpp` - `vm_get_res_header()`, `vm_load_resource()`, `vm_resource_get_data()`, `vm_res_get_image_and_size()`
- `MREmu/MREngine/IO.cpp` - `vm_get_vm_tag()`
- `MREmu/AppManager.cpp` - `get_tags_by_mem_adr()`
- `bin/Release/unpack.bin` - guest image used for disassembly
- `docs/mcp.md` - MCP server reference
