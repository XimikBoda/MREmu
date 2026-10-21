# VXP and Binary Formats

This document describes the structure and specifications of **VXP** (and **VRE**) executable packages used by the MediaTek MAUI Runtime Environment (MRE) platform, and how **MREmu** parses, unpacks, relocates, and executes them.

---

## 1. What is a VXP File?

A `.vxp` file is an executable container designed for feature phones running MediaTek chipsets (MT6250, MT6260, MT6261, MT2502, etc.). It packages:

1. **Executable Machine Code**: ARM/Thumb binary code compiled either with the **ARM Developer Suite (ADS)** or **GNU GCC**.
2. **Execution Scatter-Loading Metadata**: Specifies the location and sizes of read-only code (`ER_RO`), read-write data (`ER_RW`), and zero-initialized memory (`ER_ZI`).
3. **Application Resources**: Bundled assets including icons, bitmap graphics, localized strings, and configuration tables (stored in a `.vm_res` section).
4. **MRE Tag Header / Trailer**: Metadata describing application properties (name, author, required RAM, permissions, compiler version, and charset).

---

## 2. File Envelope & Layout

The overall binary layout of a `.vxp` file differs between uncompressed and compressed variants, but both place the metadata tags table near the end of the file.

### Uncompressed ELF VXP Layout

```text
+-----------------------------------------------------------+ 0x00000000
| ELF Header & Section Headers                              |
|   .text / ER_RO  (Executable code & constants)            |
|   .data / ER_RW  (Initialized read-write variables)       |
|   .rel.dyn / .rel.plt (Dynamic relocation tables)         |
|   .vm_res        (Optional embedded resource block)       |
+-----------------------------------------------------------+
| MRE Tags Table (MreTags)                                  |
|   Series of (ID, Size, Value) records                     |
|   Terminated with ID = 0                                  |
+-----------------------------------------------------------+
| 12-byte Trailer                                           |
|   [file_size - 12]: uint32_t tags_offset                  |
|   [file_size - 8] : uint32_t reserved / checksum          |
|   [file_size - 4] : uint32_t reserved                     |
+-----------------------------------------------------------+ File End
```

### Compressed ADS VXP Layout

In compressed ADS applications (`VM_CE_INFO_RO_RW_ZIP = 1`), the code and data execution regions are compressed with raw Deflate / zlib:

```text
+-----------------------------------------------------------+ 0x00000000
| Deflate-Compressed ER_RO (Code chunk)                     |
+-----------------------------------------------------------+
| Deflate-Compressed ER_RW (Data chunk)                     |
+-----------------------------------------------------------+
| Resource Block (.vm_res)                                  |
+-----------------------------------------------------------+
| compress_ads_elf_info Struct (36 bytes)                   |
+-----------------------------------------------------------+
| uint32_t elf_info_size (0x00000024 = 36 bytes)           |
+-----------------------------------------------------------+
| MRE Tags Table (MreTags)                                  |
+-----------------------------------------------------------+
| 12-byte Trailer (tags_offset at file_size - 12)           |
+-----------------------------------------------------------+ File End
```

---

## 3. The 12-Byte Trailer & Tag Offset

To locate the metadata tags table without scanning the entire file, the loader reads 12 bytes from the end of the file:

```cpp
size_t file_size = file.size();
if (file_size < 12) return false;

// The tags offset is stored 12 bytes before EOF:
uint32_t tags_offset = *(uint32_t*)&file[file_size - 12];
```

The loader jumps to `tags_offset` to begin parsing the tag stream.

---

## 4. MRE Tags Table (`MreTags`)

The tags table consists of a sequential list of packed Type-Length-Value (TLV) records:

```cpp
struct mre_tag_record {
    uint32_t id;        // Tag identifier (from vmcert.h)
    uint32_t tag_size;  // Byte size of the payload
    uint8_t  payload[]; // Raw data of length tag_size
};
```

Parsing continues sequentially until a record with `id == 0` is reached.

### Key Tag Definitions

| Tag ID | Macro Name | Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | — | None | **End of Tag Stream** marker. |
| `0x01` | `VM_CE_INFO_NAME` | String | Application display name. |
| `0x02` | `VM_CE_INFO_DEV` | String | Developer / company name. |
| `0x03` | `VM_CE_INFO_DESCRIPTION` | String | Application description or subtitle. |
| `0x07` | `VM_CE_INFO_MEM_REQ` | `uint32` | Requested heap/RAM size in **kilobytes** (e.g. 1024 = 1 MB). |
| `0x08` | `VM_CE_INFO_FILE_TYPE` | `uint32` | Binary format type: `0` or `1` = ADS, `5` = Simple ADS. |
| `0x16` | `VM_CE_INFO_RO_RW_ZIP` | `uint32` | Set to `1` on **every** ADS package, including plain uncompressed ELFs. It marks "is ADS", *not* "is compressed" - do not use it to pick a container. Decide from the file magic: `7F 45 4C 46` = uncompressed ELF, anything else = compressed ADS. See [vxp_icons.md](vxp_icons.md). |
| `0x0B` | `VM_CE_INFO_COMPILER` | `uint32` | Toolchain: `1` indicates VRE / ADS compiler. |
| `0x0D` | `VM_CE_INFO_CHARSET` | `uint32` | Text encoding of tag strings: `0` = ASCII, `1` = UCS-2 LE. |
| `0x19` | `VM_CE_INFO_NAME_LIST` | MString | Multi-language name list `(lang_id: u32, len: u32, name)`. |

### Character Set Handling in Tags

Tag strings can be either ASCII or UCS-2 Little-Endian depending on `VM_CE_INFO_CHARSET`. [`MreTags::raw_to_u8`](../MREmu/MreTags.cpp) automatically converts UCS-2 strings into standard UTF-8 for display across desktop and Android UIs.

---

## 5. Compressed ADS Descriptor (`compress_ads_elf_info`)

When an ADS VXP is compressed (`is_zipped = true`), a 36-byte header precedes the tags offset describing the offsets and uncompressed sizes:

```cpp
typedef struct {
    uint32_t ro_offset;     // Offset of compressed ER_RO chunk in file
    uint32_t ro_size;       // Compressed byte size of ER_RO
    uint32_t org_ro_size;   // Original (uncompressed) byte size of ER_RO
    uint32_t rw_offset;     // Offset of compressed ER_RW chunk in file
    uint32_t rw_size;       // Compressed byte size of ER_RW
    uint32_t org_rw_size;   // Original (uncompressed) byte size of ER_RW
    uint32_t zi_size;       // Zero-Initialized (BSS) memory size
    uint32_t res_offset;    // Offset of .vm_res resources in file
    uint32_t res_size;      // Byte size of .vm_res resources
} compress_ads_elf_info;
```

MREmu decompresses both regions using `miniz` (`uncompress`):

1. `ER_RO` is inflated directly into the beginning of the guest memory buffer.
2. `ER_RW` is inflated immediately following `ER_RO`.
3. Memory of size `zi_size` is zero-initialized directly following `ER_RW`.
4. The application entry point is set to the base of the decompressed `ER_RO`.

---

## 6. ELF Relocations & Dynamic Loading

For uncompressed ELF binaries, MREmu parses the ELF header using the bundled `ELFIO` library.

### Relocation Processing (`.rel.dyn` and `.rel.plt`)

Because MRE binaries are relocated dynamically to the allocated guest address `offset_mem`, all relative addresses must be patched:

```cpp
switch (sym[i].r_info & 0xFF) {
case 0x17: // R_ARM_RELATIVE
    *(uint32_t*)((unsigned char*)mem_location + sym[i].r_offset) += offset_mem;
    break;
case 0x02: // R_ARM_ABS32
case 0x16: // R_ARM_GLOB_DAT
    *(uint32_t*)((unsigned char*)mem_location + sym[i].r_offset) = 0;
    break;
}
```

### Unaligned Pointer Detection

On real hardware ARMv5TE processors, unaligned memory accesses cause hardware alignment faults or unpredictable data rotations. MREmu detects and warns if any `r_offset` in relocation tables has bits 0 or 1 set (`r_offset & 3 != 0`), helping developers detect broken link scripts.

---

## 7. Resources Section (`.vm_res`)

Assets such as menu icons and localized UI strings are packaged into a dedicated resource block identified either by an ELF section named `.vm_res` or via the `res_offset` field in compressed ADS binaries.

App icons specifically live in `AppLogo.img` inside that block, wrapped in a `VREAPPLOGO` container. The exact header varies between packages and its variant tag does not reliably describe the payload, so MREmu finds the image by signature scan rather than by parsing the tag. See [vxp_icons.md](vxp_icons.md) for the layouts, why the naive parse failed, and how both front ends extract icons.

- **Resource Headers**: Scanned via [`MREngine::Resources::scan()`](../MREmu/MREngine/Resources.cpp).
- **String Resources**: Loaded with `vm_load_resource` / `vm_get_string`.
- **Image Resources**: Loaded into canvas memory via `vm_graphic_load_image`.

---

## 8. Security & Safety Considerations

> [!WARNING]
> **VXP Bounds Checking**: As highlighted in [Known Issues](known_issues.md#vxp-offsets-are-not-bounds-checked), offsets and sizes read from `.vxp` tags and section headers are not comprehensively validated against the physical file length. A corrupted or malicious `.vxp` can trigger out-of-bounds reads or writes during loading. Always exercise caution when loading untrusted VXP files.
