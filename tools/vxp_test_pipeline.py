"""
vxp_test_pipeline.py — Automated VXP Testing, HTML Gallery, and Webpage Screenshot Suite

Automates the entire end-to-end pipeline:
1. Runs target VXP packages in MREmu via MCP interface.
2. Applies paced inputs (softkeys, OK, numpad 5, touch tap) to advance into real game/menu screens.
3. Automatically classifies test outcome:
   - Success: saves screen to <out_dir>/RunSuccess/<name>.png
   - Failure: saves diagnostics (fault status, registers, trace dump, error messages) to <out_dir>/RunFail/<name>.txt
4. Generates clean, simple HTML gallery files (standalone base64 & relative) with Arial font and grayscale antialiasing.
5. Captures a clean headless Chromium screenshot of the webpage with --hide-scrollbars and grayscale antialiasing.
"""

import argparse
import base64
import glob
import io
import json
import math
import os
import subprocess
import sys
import threading
import time
from PIL import Image

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_EXE = os.path.join(REPO_ROOT, "bin", "Release", "MREmu.exe")
DEFAULT_VXP_DIR = os.path.join(REPO_ROOT, "bin", "Release", "fs", "e", "mre")
DEFAULT_OUT_DIR = os.path.join(REPO_ROOT, "docs", "working_games", "Gameloft")

# ---------------------------------------------------------------------------
# MCP Client
# ---------------------------------------------------------------------------
class MCPClient:
    def __init__(self, exe_path=DEFAULT_EXE, cwd=REPO_ROOT):
        self.proc = subprocess.Popen(
            [exe_path, "--mcp"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            cwd=cwd
        )
        self._id = 0
        self._responses = {}
        self._stderr_lines = []
        self._lock = threading.Lock()

        self._thread_out = threading.Thread(target=self._reader, daemon=True)
        self._thread_out.start()
        self._thread_err = threading.Thread(target=self._err_reader, daemon=True)
        self._thread_err.start()

        self.rpc("initialize", {"protocolVersion": "2024-11-05", "capabilities": {}})
        self.rpc("notifications/initialized", {}, notify=True)

    def _reader(self):
        for line in self.proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
                if "id" in data:
                    with self._lock:
                        self._responses[data["id"]] = data
            except Exception:
                pass

    def _err_reader(self):
        for line in self.proc.stderr:
            self._stderr_lines.append(line)

    def rpc(self, method, params, notify=False, timeout=5.0):
        with self._lock:
            self._id += 1
            req_id = self._id
        req = {"jsonrpc": "2.0", "method": method, "params": params}
        if not notify:
            req["id"] = req_id
        msg = json.dumps(req) + "\n"
        try:
            self.proc.stdin.write(msg)
            self.proc.stdin.flush()
        except OSError as e:
            raise RuntimeError(f"Process stdin error: {e}")

        if notify:
            return None

        start = time.time()
        while time.time() - start < timeout:
            if self.proc.poll() is not None:
                raise RuntimeError(f"Process terminated with exit code {self.proc.returncode}")
            with self._lock:
                if req_id in self._responses:
                    resp = self._responses.pop(req_id)
                    if "error" in resp:
                        raise RuntimeError(f"RPC Error: {resp['error']}")
                    return resp.get("result", {})
            time.sleep(0.02)
        raise TimeoutError(f"RPC timeout for {method}")

    def call_tool(self, name, args=None, timeout=5.0):
        res = self.rpc("tools/call", {"name": name, "arguments": args or {}}, timeout=timeout)
        return res.get("content", [])

    def close(self):
        try:
            self.proc.terminate()
            self.proc.wait(timeout=1)
        except Exception:
            try:
                self.proc.kill()
            except Exception:
                pass


# ---------------------------------------------------------------------------
# Test Runner Engine
# ---------------------------------------------------------------------------
def run_vxp_test(vxp_path, success_dir, fail_dir, exe_path=DEFAULT_EXE, duration_sec=14):
    base_name = os.path.splitext(os.path.basename(vxp_path))[0]
    print(f"\n[TESTING] {base_name} ({vxp_path})")

    client = None
    try:
        client = MCPClient(exe_path=exe_path)
    except Exception as e:
        record_fail(fail_dir, success_dir, base_name, f"Failed to start MREmu: {e}")
        return False, "Failed to start"

    try:
        try:
            client.call_tool("emu_trace_control", {"action": "enable"}, timeout=2.0)
        except Exception:
            pass

        client.call_tool("emu_load_vxp", {"path": vxp_path}, timeout=5.0)

        def get_screen():
            try:
                res = client.call_tool("emu_screen", timeout=2.0)
                for c in res:
                    if c.get("type") == "image":
                        return base64.b64decode(c["data"])
            except Exception:
                pass
            return None

        def get_status():
            try:
                res = client.call_tool("emu_status", timeout=2.0)
                return json.loads(res[0].get("text", "{}"))
            except Exception:
                return {}

        crashed = False
        crash_msg = ""
        last_screen = None

        for sec in range(1, duration_sec + 1):
            time.sleep(1.0)

            if client.proc.poll() is not None:
                crashed = True
                crash_msg = f"Emulator process terminated with code {client.proc.returncode}"
                print(f"  t={sec:2d}s: CRASHED: {crash_msg}")
                break

            st = get_status()
            if st.get("faulted"):
                crashed = True
                crash_msg = f"CPU Fault: {st.get('last_error')}"
                print(f"  t={sec:2d}s: FAULTED: {crash_msg}")
                break

            # Well-spaced inputs to advance through language prompt, sound prompt, and publisher screens
            if sec in [3, 5, 7, 9, 11]:
                try:
                    client.call_tool("emu_key", {"key": -5, "action": "click"}, timeout=1.0)
                    client.call_tool("emu_key", {"key": 53, "action": "click"}, timeout=1.0)
                    client.call_tool("emu_key", {"key": -6, "action": "click"}, timeout=1.0)
                    client.call_tool("emu_touch", {"action": "tap", "x": 120, "y": 160}, timeout=1.0)
                except Exception:
                    pass

            cur_scr = get_screen()
            if cur_scr:
                last_screen = cur_scr

            if sec in [3, 8, duration_sec]:
                color_count = 0
                if cur_scr:
                    try:
                        im = Image.open(io.BytesIO(cur_scr))
                        color_count = len(im.getcolors(100000) or [])
                    except Exception:
                        pass
                print(f"  t={sec:2d}s: vm_calls={st.get('vm_calls', 0):<7} unique_colors={color_count}")

        if crashed:
            record_fail(fail_dir, success_dir, base_name, crash_msg, client=client)
            return False, crash_msg

        final_scr = get_screen() or last_screen
        final_st = get_status()

        if not final_scr:
            record_fail(fail_dir, success_dir, base_name, "No screen buffer produced", client=client, status=final_st)
            return False, "No screen"

        im_final = Image.open(io.BytesIO(final_scr))
        extrema = im_final.getextrema()
        is_all_black = (extrema[0][1] == 0 and extrema[1][1] == 0 and extrema[2][1] == 0)

        if is_all_black:
            print("  [FAIL] Screen remained completely black.")
            record_fail(fail_dir, success_dir, base_name, "Stuck on black screen (0x000000)", client=client, status=final_st)
            return False, "Black screen"

        # Success!
        print(f"  [PASS] Screen captured successfully.")
        succ_path = os.path.join(success_dir, f"{base_name}.png")
        with open(succ_path, "wb") as f:
            f.write(final_scr)

        # Clear any prior failure log
        fail_path = os.path.join(fail_dir, f"{base_name}.txt")
        if os.path.exists(fail_path):
            os.remove(fail_path)

        return True, "Success"

    except Exception as e:
        record_fail(fail_dir, success_dir, base_name, f"Exception: {e}", client=client)
        return False, str(e)
    finally:
        if client:
            client.close()


def record_fail(fail_dir, success_dir, base_name, reason, client=None, status=None):
    os.makedirs(fail_dir, exist_ok=True)
    fail_path = os.path.join(fail_dir, f"{base_name}.txt")
    succ_path = os.path.join(success_dir, f"{base_name}.png")
    if os.path.exists(succ_path):
        os.remove(succ_path)

    with open(fail_path, "w", encoding="utf-8") as f:
        f.write(f"=== FAILURE REPORT: {base_name} ===\n\n")
        f.write(f"Reason: {reason}\n\n")
        if status:
            f.write(f"Status:\n{json.dumps(status, indent=2)}\n\n")
        if client:
            try:
                err_res = client.call_tool("emu_errors", timeout=2.0)
                f.write(f"Emulator Errors:\n{err_res[0].get('text', '')}\n\n")
            except Exception:
                pass
            try:
                reg_res = client.call_tool("emu_regs", timeout=2.0)
                f.write(f"CPU Registers:\n{reg_res[0].get('text', '')}\n\n")
            except Exception:
                pass
            try:
                trace_res = client.call_tool("emu_trace_dump", {"limit": 50, "tail": True}, timeout=3.0)
                f.write(f"Recent Trace Calls (Tail 50):\n{trace_res[0].get('text', '')}\n\n")
            except Exception:
                pass
            if client._stderr_lines:
                f.write("Emulator Stderr:\n" + "".join(client._stderr_lines[-50:]) + "\n")


# ---------------------------------------------------------------------------
# HTML Gallery Generator
# ---------------------------------------------------------------------------
def generate_html_gallery(success_dir, out_html_path, standalone_path=None):
    images = sorted([f for f in os.listdir(success_dir) if f.lower().endswith((".png", ".jpg", ".jpeg"))])
    print(f"Building HTML gallery from {len(images)} images in {success_dir}")

    items = []
    for fname in images:
        fpath = os.path.join(success_dir, fname)
        title = os.path.splitext(fname)[0]
        with open(fpath, "rb") as img_f:
            b64 = base64.b64encode(img_f.read()).decode("ascii")
            data_uri = f"data:image/png;base64,{b64}"

        items.append({
            "title": title,
            "rel_path": f"RunSuccess/{fname}",
            "data_uri": data_uri
        })

    def render(use_data_uri=False):
        cells = []
        for it in items:
            src = it["data_uri"] if use_data_uri else it["rel_path"]
            cells.append(
                f'  <div style="display:inline-block; margin:4px; vertical-align:top; text-align:center; background:#000; border:1px solid #333;">\n'
                f'    <img src="{src}" width="240" height="320" alt="{it["title"]}" style="display:block;" />\n'
                f'    <div style="color:#fff; padding:4px 2px; font-size:12px; max-width:240px; overflow:hidden; white-space:nowrap; text-overflow:ellipsis;">{it["title"]}</div>\n'
                f'  </div>'
            )
        body = "\n".join(cells)
        return f'''<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>MRE Screenshots</title>
  <style>
    html, body {{
      margin: 0;
      padding: 4px;
      background: #111;
      font-family: Arial, sans-serif;
      -webkit-font-smoothing: antialiased;
      -moz-osx-font-smoothing: grayscale;
      overflow: hidden;
    }}
    ::-webkit-scrollbar {{
      display: none;
      width: 0;
      height: 0;
    }}
  </style>
</head>
<body>
{body}
</body>
</html>'''

    os.makedirs(os.path.dirname(os.path.abspath(out_html_path)), exist_ok=True)
    with open(out_html_path, "w", encoding="utf-8") as f:
        f.write(render(use_data_uri=False))

    if standalone_path:
        os.makedirs(os.path.dirname(os.path.abspath(standalone_path)), exist_ok=True)
        with open(standalone_path, "w", encoding="utf-8") as f:
            f.write(render(use_data_uri=True))

    return items


# ---------------------------------------------------------------------------
# Screenshot Capture (Headless Chromium)
# ---------------------------------------------------------------------------
def capture_webpage_screenshot(html_file, output_image_path, num_items=30):
    html_url = "file:///" + os.path.abspath(html_file).replace("\\", "/")
    
    # Calculate required dimensions
    cols = 6
    card_w = 240 + 8 + 2
    card_h = 320 + 26 + 10
    total_w = cols * card_w + 20
    rows = math.ceil(num_items / cols) if num_items > 0 else 5
    total_h = rows * card_h + 30

    chrome_candidates = [
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
    ]
    browser_exe = None
    for c in chrome_candidates:
        if os.path.exists(c):
            browser_exe = c
            break

    if not browser_exe:
        raise RuntimeError("Neither Chrome nor Edge was found for headless capture.")

    user_data = os.path.join(os.environ.get("TEMP", "C:/Temp"), "chrome_vxp_snap")
    cmd = (
        f'cmd.exe /c "start /wait `"`" `"{browser_exe}`" '
        f'--headless=new --disable-gpu --user-data-dir=`"{user_data}`" '
        f'--hide-scrollbars --window-size={total_w + 100},{total_h + 50} '
        f'--screenshot=`"{output_image_path}`" `"{html_url}`""'
    )
    print(f"Capturing webpage screenshot via {os.path.basename(browser_exe)} ({total_w}x{total_h})...")
    subprocess.run(cmd, shell=True, check=True)

    if os.path.exists(output_image_path):
        im = Image.open(output_image_path)
        # Crop exactly to grid area
        cropped = im.crop((0, 0, min(total_w, im.size[0]), min(total_h, im.size[1])))
        cropped.save(output_image_path)
        print(f"Captured screenshot saved to {output_image_path} ({cropped.size[0]}x{cropped.size[1]})")
        return True
    return False


# ---------------------------------------------------------------------------
# CLI Pipeline Interface
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Automated VXP Test & Webpage Screenshot Suite")
    parser.add_argument("--vxps", nargs="*", help="List of VXP filenames or paths to test (default: all in dir)")
    parser.add_argument("--vxp-dir", default=DEFAULT_VXP_DIR, help="Directory containing VXPs")
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR, help="Base output directory")
    parser.add_argument("--skip-tests", action="store_true", help="Skip running tests, just update HTML & screenshot")
    parser.add_argument("--duration", type=int, default=14, help="Test duration in seconds per game")
    args = parser.parse_args()

    success_dir = os.path.join(args.out_dir, "RunSuccess")
    fail_dir = os.path.join(args.out_dir, "RunFail")
    os.makedirs(success_dir, exist_ok=True)
    os.makedirs(fail_dir, exist_ok=True)

    if not args.skip_tests:
        targets = []
        if args.vxps:
            for v in args.vxps:
                p = v if os.path.isabs(v) else os.path.join(args.vxp_dir, v)
                if os.path.exists(p):
                    targets.append(p)
                else:
                    print(f"Warning: VXP not found: {p}")
        else:
            targets = glob.glob(os.path.join(args.vxp_dir, "*.vxp"))

        print(f"Starting test execution for {len(targets)} packages...")
        for p in targets:
            run_vxp_test(p, success_dir, fail_dir, duration_sec=args.duration)

    # Step 2: Build HTML Galleries
    html_rel = os.path.join(args.out_dir, "gameloft_games_gallery.html")
    html_standalone = os.path.join(args.out_dir, "gameloft_games_gallery_standalone.html")
    items = generate_html_gallery(success_dir, html_rel, html_standalone)

    # Step 3: Capture Clean Screenshot of Webpage
    screenshot_path = os.path.join(args.out_dir, "gallery_screenshot_full.png")
    capture_webpage_screenshot(html_standalone, screenshot_path, num_items=len(items))

    print("\n[COMPLETE] Automation pipeline finished successfully!")
    print(f"  - RunSuccess dir: {success_dir}")
    print(f"  - RunFail dir:    {fail_dir}")
    print(f"  - HTML Gallery:   {html_rel}")
    print(f"  - Standalone:     {html_standalone}")
    print(f"  - Screenshot:     {screenshot_path}")


if __name__ == "__main__":
    main()
