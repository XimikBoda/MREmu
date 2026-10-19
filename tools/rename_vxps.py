#!/usr/bin/env python3
"""rename_vxps.py - Uses vxpinfo to rename .vxp packages to nicely formatted names.

Can rename in local folders (e.g. bin/Release/fs/e/mre) and sync to ADB devices.
"""

import argparse
import glob
import os
import subprocess
import sys

# Import vxpinfo from tools directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vxpinfo

# Mapping logic based on metadata extracted by vxpinfo
def get_nicely_formatted_name(file_path):
    info = vxpinfo.inspect(file_path)
    base = os.path.basename(file_path)
    s = info.get("summary", {})
    tags = {t["name"]: t.get("display", t.get("value")) for t in info.get("tags", [])}

    app_name = s.get("app_name") or ""
    dev_name = s.get("dev_name") or ""
    ram_kb = s.get("ram_kb")
    
    # Check for name list if app_name is generic 'App'
    name_list = tags.get("VM_CE_INFO_NAME_LIST")
    if app_name.lower() in ("", "app") and isinstance(name_list, list) and len(name_list) > 0:
        app_name = name_list[0].get("name", app_name)

    # 1. GamesAndApps / App Store
    if "gamesandapps" in app_name.lower() or "app store" in base.lower():
        return "Games and Apps.vxp"

    # 2. Calculator
    if app_name.lower() == "calculator" or "calculator" in base.lower():
        return "Calculator.vxp"

    # 3. Facebook & Messenger
    if "messenger" in app_name.lower() or "messenger" in base.lower():
        return "Facebook Messenger.vxp"
    if "facebook" in app_name.lower() or "facebook" in base.lower():
        if "2.5.1" in base or tags.get("VM_CE_INFO_VER") == 152043776:
            return "Facebook 2.5.1.vxp"
        return "Facebook.vxp"

    # 4. Gameloft Shop / Live
    if "gameloft_live" in base.lower() or (dev_name.lower() == "gameloft" and app_name.lower() == "shop"):
        return "Gameloft Live.vxp"

    # 5. Gravity Defied
    if "gravitydefied" in app_name.lower() or "gravitydefied" in base.lower():
        # Check if x86 Windows DLL
        with open(file_path, "rb") as f:
            magic = f.read(2)
        if magic == b"MZ" or ".vc." in base.lower():
            return "Gravity Defied (x86).vxp"
        return "Gravity Defied.vxp"

    # 6. GPT
    if app_name.upper() == "GPT" or base.lower() == "gpt.vxp":
        return "GPT.vxp"

    # 7. Opera Mini
    if "opera mini" in app_name.lower() or "opera" in base.lower():
        if "6.1" in base:
            return "Opera Mini 6.1.vxp"
        if "leatest" in base.lower() or "latest" in base.lower() or "en" in base.lower():
            return "Opera Mini 4.4 (EN).vxp"
        return "Opera Mini 4.4.vxp"

    # 8. PeanutVXP
    if "peanut" in app_name.lower() or "peanut" in base.lower():
        if ram_kb == 2500 or "2500k" in base.lower():
            return "PeanutVXP (2500 KB).vxp"
        elif ram_kb == 512 or "500k" in base.lower():
            return "PeanutVXP (500 KB).vxp"
        else:
            return "PeanutVXP (1.2 MB).vxp"

    # 9. Spider-Man
    if "spiderman" in app_name.lower() or "spider" in base.lower():
        return "The Amazing Spider-Man (Demo).vxp"

    # 10. Show Picture
    if "show_picture" in app_name.lower() or "show_picture" in base.lower():
        return "Show Picture.vxp"

    # 11. RDZDX tools (Mrecp, Niblp, Textep)
    if app_name.lower() == "mrecp" or base.lower() == "mrecp.vxp":
        return "Mrecp.vxp"
    if app_name.lower() == "niblp" or base.lower() == "niblp.vxp":
        return "Niblp.vxp"
    if app_name.lower() == "textep" or base.lower() == "textep.vxp":
        return "Textep.vxp"

    # Generic fallback
    clean_title = app_name if app_name else os.path.splitext(base)[0]
    return f"{clean_title}.vxp"


def main():
    parser = argparse.ArgumentParser(description="Rename VXPs to nice formatted names using vxpinfo.")
    parser.add_argument("--dir", default=r"bin\Release\fs\e\mre", help="Directory containing .vxp files")
    parser.add_argument("--dry-run", action="store_true", help="Print planned renames without applying")
    parser.add_argument("--sync-adb", action="store_true", help="Also sync/rename files on ADB device")
    parser.add_argument("--adb-dir", default="/sdcard/MREmu/fs/e/mre", help="ADB target directory")
    parser.add_argument("--device", default="", help="Specific ADB device ID")
    args = parser.parse_args()

    vxp_dir = os.path.abspath(args.dir)
    if not os.path.isdir(vxp_dir):
        print(f"Directory not found: {vxp_dir}", file=sys.stderr)
        return 1

    files = sorted(glob.glob(os.path.join(vxp_dir, "*.[vV][xX][pP]")))
    print(f"Found {len(files)} VXP files in {vxp_dir}:\n")

    renames = []
    for file_path in files:
        old_name = os.path.basename(file_path)
        new_name = get_nicely_formatted_name(file_path)
        renames.append((file_path, old_name, new_name))
        print(f"  {old_name:35s} -> {new_name}")

    if args.dry_run:
        print("\nDry run completed. No files were modified.")
        return 0

    print("\nApplying local renames...")
    # Two-pass rename to prevent collision issues on Windows (case sensitivity or circular)
    temp_renames = []
    for file_path, old_name, new_name in renames:
        if old_name != new_name:
            temp_path = file_path + ".tmp_rename"
            os.rename(file_path, temp_path)
            temp_renames.append((temp_path, os.path.join(vxp_dir, new_name), old_name, new_name))

    for temp_path, target_path, old_name, new_name in temp_renames:
        if os.path.exists(target_path):
            os.remove(target_path)
        os.rename(temp_path, target_path)
        print(f"  Renamed: {old_name} -> {new_name}")

    if args.sync_adb:
        print(f"\nSyncing to ADB device ({args.adb_dir})...")
        adb_cmd = ["adb"]
        if args.device:
            adb_cmd += ["-s", args.device]

        # Ensure directory exists on device
        subprocess.run(adb_cmd + ["shell", f"mkdir -p '{args.adb_dir}'"], check=True)

        # Clear existing vxps in adb dir and push cleanly renamed ones
        print("  Cleaning old VXPs on device...")
        subprocess.run(adb_cmd + ["shell", f"rm -f '{args.adb_dir}'/*.vxp '{args.adb_dir}'/*.Vxp"], check=True)

        print("  Pushing renamed VXPs to device...")
        updated_files = sorted(glob.glob(os.path.join(vxp_dir, "*.[vV][xX][pP]")))
        for f in updated_files:
            dest = f"{args.adb_dir}/{os.path.basename(f)}"
            subprocess.run(adb_cmd + ["push", f, dest], check=True)
            print(f"    Pushed {os.path.basename(f)}")

        print("  Done syncing to ADB device.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
