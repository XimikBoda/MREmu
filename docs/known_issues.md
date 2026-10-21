# Known Issues

This document tracks known limitations, architectural edge cases, and open issues in MREmu.

---

## `--steps` Above ~5,000 Instructions Still Crashes

`--inspect --steps N` bounds a single `uc_emu_start` call to `N` instructions. Above roughly 5,000 the VRE/ADS boot path faults (`0xC0000409` on Windows) partway through. This is undiagnosed and unfixed.

Note that the MCP server's `emu_step` does **not** hit this: it deliberately caps each VM callback at 4096 instructions and carries the remainder of the budget over to the app's next callback, which runs indefinitely. `--inspect` still spends its whole limit in the one invocation, because it is running the app's entry point directly and has no next callback to defer to.

---

## An App That Has Spent Its Timers Cannot Be Resumed

`Opera Mini 6.1.vxp` creates a single 10-second timer while booting and deletes it again. Once the app has run that timer out and stopped, nothing re-enters it: the emulator only calls into the app from a timer callback or an incoming event, so it stays parked at its idle handler indefinitely. `emu_step` reports this honestly (`timers_made_due: 0` with a non-zero `step_budget`) rather than appearing to work. To drive the app again, inject input or reload the VXP.

---

## A Second Concurrent Directory Enumeration Stops After One Entry

`vm_find_next_ext` ([`MREmu/MREngine/IO.cpp`](../MREmu/MREngine/IO.cpp)) walks a `find_el` snapshot. A second enumeration opened while another find handle is still active (the old `AppSelector::scan()` leaked its handle, and VXP apps can legitimately keep one open) stops after the first entry. The app selector's old folder-details count had this and was removed rather than shipped wrong; `scan()` itself now closes its handle, so back-to-back enumerations through the selector work. Whether an app's own second concurrent enumeration is affected is unverified.

---

## VXP Offsets Are Not Bounds-Checked

[`MREmu/ArmApp.cpp`](../MREmu/ArmApp.cpp) and [`MREmu/MreTags.cpp`](../MREmu/MreTags.cpp) use offsets and sizes read straight out of the `.vxp` file without validating them against the file's actual size. A malformed or deliberately hostile VXP can therefore cause out-of-bounds reads and writes when it is loaded. On Android this is reachable without any special setup: `MainActivity` registers an `ACTION_VIEW` filter for `content://` URIs matching `.*\.vxp`, so opening such a file from any file manager loads it.

This is a known, unfixed defect. Treat VXP files from untrusted sources as unsafe, and do not expect the emulator to survive a malformed one. The fix is to validate every `offset + size` against `file_context.size()` before each dereference and `memcpy`.

---

## Release APKs Are Signed with the Debug Key

`Android/app/build.gradle` sets `signingConfig signingConfigs.debug` for the release build type, and `~/.android/debug.keystore` is auto-generated per machine with the publicly documented password `android`. Two consequences:

- The APKs are for **sideloading only**. They cannot be published to the Play Store, and installing one over an existing install signed by a different machine's debug key fails with `INSTALL_FAILED_UPDATE_INCOMPATIBLE`.
- Because that key is not secret, anyone can build an APK with the same `applicationId` and a signature that `adb install -r` will accept as an upgrade of the genuine app. Use the build for personal testing only.
