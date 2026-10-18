# Native Applications & UI Subsystem

This document describes the native applications built directly into **MREmu**, focusing on the default launcher (**AppSelector**) and its reusable UI components (**BottomSheet**).

---

## 1. What are Native Apps in MREmu?

In addition to executing emulated ARM binaries, MREmu can run **Native Apps** written in standard C++ ([`NativeApp.h`](../MREmu/NativeApp.h)):

- **MRE API Integration**: Native apps consume the same `vm_*` C functions (such as `vm_graphic_create_layer`, `vm_find_first_ext`, and `vm_create_timer`) that guest VXP applications use.
- **Execution Speed**: Native apps run directly on host CPU hardware without passing through the Unicorn emulator.
- **System Integration**: They are used for core platform menus, built-in file selectors, and system dialogs.

---

## 2. The Launcher Menu (`AppSelector`)

Implemented in [`MREmu/NativeApps/Menu/AppSelector.cpp`](../MREmu/NativeApps/Menu/AppSelector.cpp), `AppSelector` is the default application loaded when MREmu boots without a designated `.vxp` argument.

```text
+-----------------------------------+
|              MREmu                |
|  [Select an Application]          |
| +-------------------------------+ |
| | [Icon] Opera Mini 6.1         | |
| |        Opera Software (342 KB)| |
| +-------------------------------+ |
| | [Icon] Bounce Tales           | |
| |        Nokia / Rovio (410 KB) | |
| +-------------------------------+ |
| | [Icon] City Bloxx             | |
| |        Digital Chocolate      | |
| +-------------------------------+ |
| |                               | |
| +-------------------------------+ |
+-----------------------------------+
```

### Application Discovery

1. `AppSelector::scan()` scans the default virtual directory `e:\mre\` using `vm_find_first_ext` and `vm_find_next_ext`.
2. For each `.vxp` encountered, it reads the application icon, human-readable title, developer name, and file size.
3. Applications are sorted alphabetically and displayed in a scrollable list.

### Touch Scrolling & Rubber-Banding Physics

`AppSelector` implements modern fluid touch interaction:

- **1:1 Finger Tracking**: The list follows the touch point synchronously and stops immediately when released.
- **No Involuntary Fling**: Inertial fling animations on timers were intentionally omitted so that physical keyboard navigation (arrow keys / D-pad) is never fighting a running timer animation.
- **Apple WWDC18 Rubber-Band Curve**: When pulled past the top or bottom boundaries, an elastic resistance curve is applied:
  $$b(x, d, c) = \left(1 - \frac{1}{\frac{x \cdot c}{d} + 1}\right) \cdot d$$
  where $c = 0.55$ (`kRubberC`), $x$ is the pull distance, and $d$ is the screen dimension.

### Marquee Text Animation

When a selected application name exceeds the available screen width:

- A smooth horizontal text marquee scrolls the title back and forth.
- Controlled by a dedicated timer (`marquee_timer_id`) with pause intervals at the endpoints (`marquee_pause_ticks = 28`).

---

## 3. The `BottomSheet` Component

Implemented in [`MREmu/NativeApps/Menu/BottomSheet.cpp`](../MREmu/NativeApps/Menu/BottomSheet.cpp), `BottomSheet` provides a reusable modal dialog:

### Features

- **Smooth Easing**: Animates vertically from the bottom of the screen using cubic ease-out interpolation.
- **Dimmed Background**: Automatically dims the underlying layer to focus user attention on the dialog.
- **Touch & Key Dismissal**: Dismissible via tapping the dimmed scrim, pressing the Back softkey, or tapping explicit confirmation buttons.
- **Use Cases**:
  - Viewing full VXP file metadata (version, author, size, exact virtual path).
  - Launch confirmation dialogs.
