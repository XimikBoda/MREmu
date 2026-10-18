# Fixed Case: Android Silently Swallowed Emulator Errors

## The short version

The emulator could fail — a bad VXP file, a file it could not read, a corrupt ELF —
and on Android nothing happened. The user just sat there looking at a blank screen with
no idea anything had gone wrong.

The reason was simple: the engine wrote the error message into a variable and raised a
flag, but only the desktop build ever read them back. Desktop showed the message in an
ImGui popup. Android had no popup, so the message was written and then thrown away.

The fix adds an Android path that hands the message to the app, which shows it in a
Compose dialog with a **Copy** button.

## What happened

- `MREmu/MREmu/AppManager.cpp` sets `error_message` and `show_error` on three failures:
  unrecognised VXP format, failed load, failed preparation.
- `MREmu/MREmu.cpp` sets the same pair when the VXP path does not exist.
- `trigger_hard_reset_with_error()` sets them on a fault after the app is running.
- On desktop, `MREmu.cpp` had an ImGui popup reading that state and rendering it.
- On Android, nothing read it. The only Android-side hint was a 4-second flashing
  "Invalid VXP file!" banner, which is drawn by a completely different mechanism
  (`show_warning`, set by drag-and-drop) and does not carry the message.

## Why it happened

The error state was global and flag-based, but the code that consumed it lived inside a
desktop-only block:

```cpp
#ifndef ANDROID
if (show_error) {
    ImGui::OpenPopup("VXP Error");
    show_error = false;
}
```

Android compiled that block out, so `show_error` was set, never read, and the fault
reason — including the full path of the file that failed — was unrecoverable.

## The fix

### 1. Hand the message to the app from the render thread

`MREmu/MREmu.cpp` gains `request_error_dialog()`, which attaches to the JVM if needed and
calls `showErrorDialogUi(String)` on the activity. It mirrors the existing
`request_options_dialog()` and carries the same JNI exception hygiene: a pending
exception must be cleared before detaching the thread, or the process aborts.

```cpp
static void request_error_dialog(const std::string& message) {
	// ... GetEnv / AttachCurrentThread ...
	jmethodID mid = env->GetMethodID(cls, "showErrorDialogUi", "(Ljava/lang/String;)V");
	if (mid) {
		jstring j_msg = env->NewStringUTF(message.c_str());
		if (j_msg) {
			env->CallVoidMethod(g_activity, mid, j_msg);
			env->DeleteLocalRef(j_msg);
		}
		else {
			env->ExceptionClear();
		}
		if (env->ExceptionCheck())
			env->ExceptionClear();
	}
	// ... DetachCurrentThread ...
}
```

### 2. Drain the message in the main loop, Android only

```cpp
#ifdef ANDROID
	if (show_error) {
		show_error = false;
		std::string err = take_error_message();
		if (!err.empty())
			request_error_dialog(err);
	}
#endif
```

`take_error_message()` returns the text **and clears it**, which is what stops the same
fault being re-shown on every frame.

The `#ifdef ANDROID` is load-bearing. The desktop ImGui popup further down the same loop
reads `error_message` directly instead of taking it. If the drain ran on both platforms,
the popup would get an empty string. The two consumers must not both claim the message.

### 3. Show it as a Compose dialog

`MainActivity.kt` gains:

- `showErrorDialogUi(message)` — the JNI entry point, marshals to the UI thread.
- `presentErrorDialog(message)` — guards against a finishing activity and a second
  stacked dialog, builds a `ComposeView`, and shows a translucent platform `Dialog`.
- `copyToClipboard(text)` — `ClipboardManager`, with a toast suppressed on Android 13+
  because the system shows its own confirmation there.
- `ErrorDialog` / `ErrorDialogAction` — the card: red error icon and title, the message in
  a scrollable bounded box, then **Copy** and **OK** stacked in one segmented group.

The dialog is hosted in a platform `Dialog` rather than a Compose `Dialog` on purpose.
Wrapping a Compose `Dialog` inside a platform dialog opens a second window on top of the
first, which stacks two scrims and makes the back button dismiss the wrong one.

The message box is capped at 220dp and scrolls, because the text carries a full filesystem
path and will not reliably fit on one screen.

## Verification

| Check | Result |
|---|---|
| Desktop Release build | `bin\Release\MREmu.exe` builds clean |
| Android `assembleDebug` (arm64-v8a) | BUILD SUCCESSFUL |
| `test_mcp.py` | ALL CHECKS PASSED |
| `test_mcp_window.py` | ALL CHECKS PASSED |
| Desktop ImGui popup | untouched — the drain is `#ifdef ANDROID` |

## Notes for the next agent

- Both `request_options_dialog()` and `request_error_dialog()` cache a single global ref to
  the activity that is never refreshed. Every callback therefore re-checks
  `isFinishing || isDestroyed` on the Kotlin side before touching a window, and wraps
  `show()` in a `BadTokenException` catch.
- `error_message` is written under `g_error_mutex` in `trigger_hard_reset_with_error()`,
  but `AppManager.cpp` writes the global directly without the lock. That is pre-existing
  and unchanged here, but it means the lock does not actually protect all writers.
- The desktop popup is still the only desktop consumer. If a third front end is ever
  added, it needs the same `#ifdef` discipline, or one consumer will drain the message
  out from under another.