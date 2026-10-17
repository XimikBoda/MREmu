# MREmu WebAssembly Static Deployment Documentation (GitHub Pages)

This document describes the static deployment architecture for **MREmu**, explaining how the WebAssembly emulator target is packaged and published to GitHub Pages without backend servers or remote dependencies.

---

## 1. Zero-Backend Static Architecture

The WebAssembly port of MREmu is 100% client-side:
- **Zero Server-Side Processing:** All emulation logic (`mremu.wasm`), C++ engine functions, and user `.vxp` file processing run in the client's web browser memory.
- **Zero Remote APIs:** No external HTTP API endpoints, server uploads, or database dependencies are required.
- **Client-Side Storage:** Application files (`.vxp`, `.vre`, `.mre`) are written to Emscripten's virtual filesystem (`MEMFS`) using JavaScript `FileReader`.

---

## 2. GitHub Actions Automated CI/CD Workflow

The `.github/workflows/web.yml` workflow automatically builds and deploys the static site to GitHub Pages on every push to `main` / `master`:

### Build & Package Steps:
1. **Checkout Submodules:** Clones MREmu repository and submodules (`SFML`, `unicorn`, `libADLMIDI`, `spdlog`, `libiconv-cmake`).
2. **Setup Emscripten Toolchain:** Installs Emscripten SDK via `mymindstorm/setup-emsdk`.
3. **WASM Compilation:** Runs `./web/build_web.sh` to compile `mremu.wasm` and `mremu.js`.
4. **Bundle Static Artifacts:** Copies public static assets into `dist/`:
   ```
   dist/
   ├── index.html        # Responsive dark UI
   ├── style.css         # Dark theme styles
   ├── src/              # MREmu.js API & app.js controller
   ├── mremu.js          # Emscripten JS loader
   └── mremu.wasm        # Compiled WebAssembly core
   ```
5. **Publish to GitHub Pages:** Uploads `dist/` bundle using `actions/upload-pages-artifact` and publishes via `actions/deploy-pages`.

---

## 3. Web Server & MIME Type Configuration

GitHub Pages automatically serves static files with the appropriate Content-Type headers:
- `.wasm` files: `application/wasm`
- `.js` files: `text/javascript` or `application/javascript`
- `.html` files: `text/html`
- `.css` files: `text/css`

For custom Web server hosts (e.g. Nginx, Apache), ensure `application/wasm` MIME type is configured for `.wasm` files.

---

## 4. Preservation of Native Desktop / Mobile Build

The GitHub Pages deployment pipeline packages only compiled public web assets from `dist/` and `web/`. Private C++ source files, build object files, and native build scripts outside `dist/` remain excluded from public deployment. Desktop and Android native build systems are fully preserved.
