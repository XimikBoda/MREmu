# MRE Runtime API Reference

This document provides a comprehensive guide to the **MediaTek MRE C Runtime APIs** emulated by **MREmu**, detailing how they are structured, mapped through the symbol bridge, and implemented natively.

For the other side of the ledger - the APIs MREmu does *not* implement, and
the ones that resolve but quietly do nothing - see
[**Unimplemented MRE APIs**](unimplemented_apis.md).

---

## 1. Symbol Resolution Architecture

MRE applications query function pointers dynamically at runtime using `vm_get_sym_entry`:

```c
typedef VMINT (*vm_get_sym_entry_t)(const char* symbol);
```

When an application starts, the address of `vm_get_sym_entry` is passed to the entry point (in register `R0` or `R1`). During initialization, the application calls `vm_get_sym_entry("vm_malloc")`, `vm_get_sym_entry("vm_create_timer")`, etc., to populate its internal dispatch table.

In MREmu, [`Bridge.cpp`](../MREmu/Bridge.cpp) implements this symbol resolver, routing every recognized identifier to a trampoline that unpacks guest registers (AAPCS: `R0`–`R3` and stack) and invokes the native C++ handler.

---

## 2. System & Memory Management APIs

Implemented primarily in [`MREngine/System.cpp`](../MREmu/MREngine/System.cpp) and [`Memory.cpp`](../MREmu/Memory.cpp).

### Memory Allocation

| Function | Signature | Description |
| :--- | :--- | :--- |
| `vm_malloc` | `void* vm_malloc(VMINT size)` | Allocates `size` bytes from the guest application memory heap. Returns a virtual 32-bit pointer. |
| `vm_calloc` | `void* vm_calloc(VMINT size)` | Allocates zero-initialized memory for the guest application. |
| `vm_realloc` | `void* vm_realloc(void* p, VMINT size)` | Resizes an allocated memory block, copying existing contents. |
| `vm_free` | `void vm_free(void* ptr)` | Frees memory previously allocated with `vm_malloc` or `vm_calloc`. |
| `vm_get_malloc_stat` | `vm_malloc_stat_t* vm_get_malloc_stat(void)` | Returns memory statistics (total heap, used bytes, and free pool size). |

### System Time & Environment

| Function | Signature | Description |
| :--- | :--- | :--- |
| `vm_get_time` | `VMINT vm_get_time(vm_time_t* time)` | Populates the `vm_time_t` struct with the current wall-clock date and time. |
| `vm_get_curr_utc` | `VMINT vm_get_curr_utc(VMUINT* utc)` | Writes the current UTC timestamp (seconds since Unix epoch). |
| `vm_get_sys_time_zone` | `VMINT vm_get_sys_time_zone(void)` | Returns the time zone offset in minutes relative to UTC. |
| `vm_get_tick_count` | `VMUINT vm_get_tick_count(void)` | Monotonic millisecond counter since system boot (derived from host clock). |
| `vm_get_clock` | `VMUINT vm_get_clock(void)` | High-precision microsecond counter for performance profiling. |
| `vm_get_sys_scene` | `VMINT vm_get_sys_scene(void)` | Returns system scene / power profile state (`VM_SCENE_NORMAL`). |

### Process & Program Management

| Function | Signature | Description |
| :--- | :--- | :--- |
| `vm_pmng_register_msg_proc` | `void vm_pmng_register_msg_proc(vm_pmng_msg_proc proc)` | Registers the application's top-level event/message callback. |
| `vm_pmng_get_current_handle` | `VMINT vm_pmng_get_current_handle(void)` | Returns the active application process handle (`phandle`). |
| `vm_exit` | `void vm_exit(VMINT status)` | Terminates the current application, cleaning up resources. |

---

## 3. Timer & Event Loop APIs

Implemented in [`MREngine/Timer.cpp`](../MREmu/MREngine/Timer.cpp) and [`MREngine/Timer.h`](../MREmu/MREngine/Timer.h).

MREmu maintains two timer queues: **Standard Timers** and **GUI Timers**.

| Function | Signature | Description |
| :--- | :--- | :--- |
| `vm_create_timer` | `VMINT vm_create_timer(VMUINT32 millis, VM_TIMERPROC_T cb)` | Creates a recurring timer firing every `millis` milliseconds. |
| `vm_delete_timer` | `VMINT vm_delete_timer(VMINT timer_id)` | Cancels and destroys an active recurring timer. |
| `vm_create_timer_ex` | `VMINT vm_create_timer_ex(VMUINT32 millis, VM_TIMERPROC_T cb)` | Creates a non-GUI background timer. |
| `vm_delete_timer_ex` | `VMINT vm_delete_timer_ex(VMINT timer_id)` | Destroys a background timer created via `vm_create_timer_ex`. |

### Execution Slicing & Timer Stepping

- Applications are event-driven and sit idle when no timer callbacks are due.
- [`Timer::update(App* app)`](../MREmu/MREngine/Timer.cpp) checks all timers against `global_clock` and triggers guest callbacks when due.
- In headless debugging or MCP control, [`Timer::make_all_due()`](../MREmu/MREngine/Timer.cpp) forces all active timers to expire immediately so stepping tools can advance the application state without real-time delay.

---

## 4. Character Set & String APIs

Implemented in [`MREngine/CharSet.cpp`](../MREmu/MREngine/CharSet.cpp) and [`MREngine/STDLib.cpp`](../MREmu/MREngine/STDLib.cpp).

MediaTek MRE predominantly utilizes **UCS-2 Little-Endian** (16-bit wide characters) for UI strings and filenames.

### Character Conversion

| Function | Signature | Description |
| :--- | :--- | :--- |
| `vm_ascii_to_ucs2` | `VMINT vm_ascii_to_ucs2(VMWSTR dst, VMINT size, VMSTR src)` | Converts a null-terminated 8-bit ASCII string to 16-bit UCS-2 LE. |
| `vm_ucs2_to_ascii` | `VMINT vm_ucs2_to_ascii(VMSTR dst, VMINT size, VMWSTR src)` | Converts a 16-bit UCS-2 LE string to an 8-bit ASCII string. |
| `vm_chset_convert` | `VMINT vm_chset_convert(int from, int to, char* in, char* out, int len)` | Codepage conversion powered by the bundled `libiconv`. |
| `vm_ucs2_string` | `VMWSTR vm_ucs2_string(VMSTR src)` | Helper returning an emulated UCS-2 pointer allocated in guest RAM. |

### Wide String Utility Functions (`STDLib.cpp`)

- `vm_wstrlen`: Returns character count of a UCS-2 string (excluding terminator).
- `vm_wstrcpy`, `vm_wstrncpy`: Copies UCS-2 character buffers.
- `vm_wstrcmp`, `vm_wstrncmp`: Lexicographical comparison of UCS-2 strings.
- `vm_wstrcat`: Appends a source UCS-2 string to a destination buffer.

---

## 5. SIM & Telephony APIs

Implemented in [`MREngine/SIM.cpp`](../MREmu/MREngine/SIM.cpp).

| Function | Signature | Description |
| :--- | :--- | :--- |
| `vm_sim_get_card_count` | `VMINT vm_sim_get_card_count(void)` | Returns simulated SIM slot count (default `1`). |
| `vm_sim_card_is_present` | `VMINT vm_sim_card_is_present(VMINT slot)` | Returns `1` indicating the virtual SIM card is inserted. |
| `vm_sim_get_card_status` | `VMINT vm_sim_get_card_status(VMINT slot)` | Returns `VM_SIM_CARD_STATUS_READY`. |
| `vm_sim_get_imei` | `VMINT vm_sim_get_imei(VMINT slot, VMSTR imei)` | Copies a standard 15-digit simulated IMEI string into the buffer. |
| `vm_sim_get_imsi` | `VMINT vm_sim_get_imsi(VMINT slot, VMSTR imsi)` | Copies a simulated IMSI subscriber identifier. |

---

## 6. Sensor & Hardware Stubs

Implemented in [`MREngine/Sensor.cpp`](../MREmu/MREngine/Sensor.cpp).

- `vm_sensor_open`: Opens a sensor handle (e.g. accelerometer).
- `vm_sensor_close`: Closes a sensor handle.
- Sensors return clean stub values to ensure games and apps requiring orientation or motion sensors do not crash on launch.

---

## 7. Additional Subsystem APIs

Detailed coverage for specialized subsystems is provided in dedicated documents:

- **Graphics & Canvases**: See [Graphics Subsystem](graphics.md) for `vm_graphic_*`.
- **Audio & MIDI**: See [Audio Subsystem](audio.md) for `vm_bitstream_*` and audio playback.
- **File System & IO**: See [File System & I/O](io_and_filesystem.md) for `vm_file_*` and `vm_find_*`.
- **Networking & Sockets**: See [Networking & Sockets](networking.md) for `vm_tcp_*` and `vm_udp_*`.
