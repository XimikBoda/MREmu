# MREmu Client-Side File Loading Pipeline Documentation

This document describes the browser client-side file loading system for **MREmu**, explaining how `.vxp`, `.vre`, and `.mre` application binaries are loaded directly from the user's browser memory into the WebAssembly virtual filesystem without server uploads.

---

## 1. Zero-Server Client-Side Data Flow

```
+--------------------------+
|  User File Selection     |
| <input type="file"> / DnD |
+------------+-------------+
             |
             v
+--------------------------+
| Browser FileReader API   |  (Entirely client-side in JS)
|  readAsArrayBuffer()     |
+------------+-------------+
             |
             v
+--------------------------+
| File Format Validation   |  (Check .vxp extension & Zlib 0x78 / ELF magic bytes)
+------------+-------------+
             |
             v
+--------------------------+
| Emscripten WASM Heap     |  (Module._malloc -> Module.HEAPU8.set)
+------------+-------------+
             |
             v
+--------------------------+
| WASM MEMFS Virtual Drive |  (Write to std::ofstream e:/<filename>)
+------------+-------------+
             |
             v
+--------------------------+
| MREmu AppManager         |  (g_webAppManager->add_app_for_launch)
+--------------------------+
```

---

## 2. File Selection & Drag and Drop Interface

In `web/index.html`, users can select files using standard HTML `<input type="file" accept=".vxp,.vre,.mre">` or by dragging files onto the display container:

```javascript
// HTML5 FileReader reads array buffer into browser memory
var reader = new FileReader();
reader.onload = function(e) {
  var uint8Array = new Uint8Array(e.target.result);

  // Allocate buffer in WebAssembly linear heap
  var ptr = Module._malloc(uint8Array.length);
  Module.HEAPU8.set(uint8Array, ptr);

  // Call C export function
  loadFileFunc(file.name, ptr, uint8Array.length);
  Module._free(ptr);
};
reader.readAsArrayBuffer(file);
```

---

## 3. Format Validation & Error Handling

Before launching, JavaScript checks file extensions and validates header magic bytes:
- **Zlib Compressed VXP:** First byte is `0x78`.
- **Uncompressed ELF VXP:** First 4 bytes match `\x7F ELF` (`0x7F`, `0x45`, `0x4C`, `0x46`).

If invalid files or corrupt buffers are passed, helpful error messages are rendered in the status bar without crashing the WebAssembly runtime.

---

## 4. WASM Virtual Filesystem Bridge (`MEMFS`)

In `web/src/main_wasm.cpp`, the exported function `mremu_wasm_load_file()` receives the filename and memory pointer, creating the file in Emscripten's virtual drive (`e:/` mapped to MEMFS `/fs/e/`):

```cpp
extern "C" EMSCRIPTEN_KEEPALIVE
int mremu_wasm_load_file(const char* filename, const uint8_t* data, size_t length) {
    std::string dest_path = std::string("e:/") + filename;
    fs::path target_path = path_from_emu(dest_path);

    fs::create_directories(target_path.parent_path());
    std::ofstream out(target_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data), length);
    out.close();

    g_webAppManager->add_app_for_launch(target_path.string(), false);
    return 1;
}
```

---

## 5. Preservation of Native File Loading

Native file loading logic (`ArmApp.cpp`, `App.cpp`, stdio file streams) remains completely untouched. The WASM target simply populates Emscripten's virtual filesystem paths before invoking standard native file loading routines.
