# Building on Android

The Android app is a Gradle project in the [`Android/`](../../Android) folder. It uses the shared
[`CMakeLists.txt`](../../CMakeLists.txt) through the Android Gradle Plugin external native build, so
the same MREmu sources are compiled as a native library.

## Requirements

- [Android Studio](https://developer.android.com/studio) (or just Android SDK + command-line tools)
- [Java 17 or 21](https://adoptium.net/). Do **not** use a JDK 25 toolchain: the Gradle wrapper in this
  project is `gradle-9.0-milestone-1` with Android Gradle Plugin 8.9.1, which do not support JDK 25 and will
  fail with `Unsupported class file major version 69`.
- The Android SDK with these packages installed:
  - Platform: `android-36`
  - Build Tools: `35.0.0`
  - NDK: `27.0.12077973` (see `Android/app/build.gradle` → `ndkVersion`)
  - CMake: `3.22.1`

You can install the required SDK packages with `sdkmanager`:

```bat
sdkmanager "platforms;android-36" "build-tools;35.0.0" "ndk/27.0.12077973" "cmake/3.22.1"
```

> Note: older `sdkmanager` builds accept the slash form for NDK/CMake package ids. If
> `sdkmanager "ndk;27.0.12077973"` reports *Package not found*, use `ndk/27.0.12077973` instead.

## 1. Get the source and MediaTek SDK

Clone with submodules and install the MediaTek MRE SDK headers as for the desktop builds (see
[Windows](windows.md) / [Linux](linux.md)):

```bash
git clone --recursive https://github.com/XimikBoda/MREmu
cd MREmu
git submodule update --init --recursive
```

Set the `MRE_SDK` environment variable to the SDK directory that contains the `include` folder.

## 2. Point Gradle at the SDK and a supported JDK

Create `Android/local.properties` (this file is machine-specific and should not be committed):

```properties
sdk.dir=C\:\\Users\\<you>\\AppData\\Local\\Android\\Sdk
```

Ensure the Java you use is 17 or 21, for example:

```bat
set "JAVA_HOME=C:\Program Files\Android\Android Studio\jre"  REM if that is JDK 17/21, otherwise use a JDK 21 install
set "PATH=%JAVA_HOME%\bin;%PATH%"
java -version
```

## 3. Build

From the `Android` folder:

```bat
cd Android
gradlew.bat assembleDebug
```

or a release build:

```bat
gradlew.bat assembleRelease
```

APK output:

- Debug: `Android/app/build/outputs/apk/debug/app-debug.apk`
- Release: `Android/app/build/outputs/apk/release/app-release.apk`

## 4. Build and install from the launcher script

`launch.bat` includes an Android submenu that performs the same checks and Gradle tasks
(`[1]` Build Debug, `[2]` Build Release, `[3]` Install + Launch Debug,
`[4]` Install + Launch Release, `[5]` Clean):

```bat
launch.bat 7
```

Menu keys take effect as soon as they are pressed - there is no need to confirm with
Enter. The whole flow can also be scripted by passing both choices as arguments:

```bat
launch.bat 7 4
```

The install options build the APK, push it to the connected device and then start
`MainActivity` on it. The script locates `adb` via `sdk.dir` in `Android/local.properties`,
then `ANDROID_SDK_ROOT` / `ANDROID_HOME`, then `%LOCALAPPDATA%\Android\Sdk`, then `PATH`.
If more than one device is connected, set `ANDROID_SERIAL` first:

```bat
set ANDROID_SERIAL=<serial>
launch.bat 7
```

## 5. Install on a device

Enable USB debugging on the phone, connect it, then:

```bat
adb devices
adb install -r Android\app\build\outputs\apk\release\app-release.apk
adb shell am start -n com.ximikboda.mremu/.MainActivity
```

## Troubleshooting

### `Unsupported class file major version 69`

You are running Gradle with JDK 25. Point `JAVA_HOME` to a JDK 17 or 21 install, or delete stale
Gradle-managed JDK caches and re-run the build. `launch.bat` prefers a JDK 21 installation
(under `%USERPROFILE%\.gradle\jdks`) over Android Studio's bundled JBR for exactly this reason.

### `SDK location not found`

Create/update `Android/local.properties` with a valid `sdk.dir`, or set `ANDROID_HOME`.

### Stale `.cxx` build directory pointing at the wrong NDK

If you change the requested NDK version, delete the CMake cache before rebuilding:

```bat
rmdir /s /q Android\app\.cxx
```

Gradle's external native build caches the toolchain/NDK selection in that directory, so a stale
cache can keep using an old NDK.
