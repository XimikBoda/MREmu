@echo off
rem MREmu launcher: configure, build, run, and clean using the
rem Visual Studio (Win32 / x86) CMake generator.
rem   Usage: launch.bat [0-8] [android 0-6] [abi 0-3] [web 0-4]   (no argument = interactive menu)
rem     [0] -> exit
rem     [1] -> build Release
rem     [2] -> build Debug
rem     [3] -> run Release
rem     [4] -> run Debug
rem     [5] -> clean build artifacts
rem     [6] -> reconfigure + build Release
rem     [7] -> Android build menu
rem     [8] -> Web (Emscripten) build menu
rem   Menu keys take effect immediately; no Enter key needed.
rem   Pass a second argument to answer the Android menu non-interactively,
rem   e.g. launch.bat 7 4 -> install + launch the Release APK.
rem   Android option [6] opens the per-ABI build menu (0=back, 1=arm64-v8a,
rem   2=armeabi-v7a, 3=x86_64), which also takes a third argument:
rem   e.g. launch.bat 7 6 2 -> build a Release armeabi-v7a APK into Downloads.
rem   Web option [3] launches the web build in the default browser.
setlocal

set "ANDROID_PACKAGE=com.ximikboda.mremu"
set "ANDROID_ACTIVITY=com.ximikboda.mremu.MainActivity"

set "ROOT=%~dp0"
cd /d "%ROOT%"

rem Locate CMake (prefer the copy installed with Visual Studio).
set "CMAKE=cmake"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -property installationPath`) do (
    if exist "%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE=%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )
)

if not "%~1"=="" (
  set "choice=%~1"
  set "once=1"
  goto run_choice
)

:menu
echo.
echo   MREmu - what do you want to do?
echo   [0] Exit
echo   [1] Build Release
echo   [2] Build Debug
echo   [3] Run Release
echo   [4] Run Debug
echo   [5] Clean
echo   [6] Reconfigure + build Release
echo   [7] Android build menu
echo   [8] Web (Emscripten) build menu
echo.
echo   Press a listed key. Other keys beep and are ignored.
echo.
choice /c:012345678 /n
if errorlevel 10 goto prompt_menu
set "choice=%errorlevel%"
set /a choice-=1
goto run_choice

rem Fallback if choice is unavailable (e.g. very old Windows).
:prompt_menu
set "choice="
set /p "choice=Choose 0-8: "
goto run_choice

:run_choice
if "%choice%"=="0" goto done
if "%choice%"=="1" goto build_release
if "%choice%"=="2" goto build_debug
if "%choice%"=="3" goto run_release
if "%choice%"=="4" goto run_debug
if "%choice%"=="5" goto clean
if "%choice%"=="6" goto reconfigure
if "%choice%"=="7" goto android_menu
if "%choice%"=="8" goto web_menu
if defined once (
  echo Unknown option "%~1". Use 0, 1, 2, 3, 4, 5, 6, 7 or 8.
  exit /b 1
)
goto menu

:build_release
echo.
echo Building Release...
call :configure
if errorlevel 1 ( if defined once exit /b %errorlevel% & goto menu )
"%CMAKE%" --build build --config Release
if errorlevel 1 ( echo Build failed. & if defined once exit /b %errorlevel% & goto menu )
call :copy_openal Release
if defined once exit /b 0
goto menu

:build_debug
echo.
echo Building Debug...
call :configure
if errorlevel 1 ( if defined once exit /b %errorlevel% & goto menu )
"%CMAKE%" --build build --config Debug
if errorlevel 1 ( echo Build failed. & if defined once exit /b %errorlevel% & goto menu )
call :copy_openal Debug
if defined once exit /b 0
goto menu

:run_release
if not exist "bin\Release\MREmu.exe" (
  echo Release build not found. Build first [1].
  if defined once exit /b 1
  goto menu
)
echo.
echo Copying to MREmu.build_run.exe...
copy /y "bin\Release\MREmu.exe" "bin\Release\MREmu.build_run.exe" >nul
echo Starting MREmu (Release)...
start "" "bin\Release\MREmu.build_run.exe"
if defined once exit /b 0
goto menu

:run_debug
if not exist "bin\Debug\MREmu.exe" (
  echo Debug build not found. Build first [2].
  if defined once exit /b 1
  goto menu
)
echo.
echo Copying to MREmu.build_run.exe...
copy /y "bin\Debug\MREmu.exe" "bin\Debug\MREmu.build_run.exe" >nul
echo Starting MREmu (Debug)...
start "" "bin\Debug\MREmu.build_run.exe"
if defined once exit /b 0
goto menu

:clean
echo.
echo Cleaning build artifacts...
if exist build rmdir /s /q build
if exist bin\Release\MREmu.exe del /q bin\Release\MREmu.exe
if exist bin\Release\MREmu.build_run.exe del /q bin\Release\MREmu.build_run.exe
if exist bin\Debug\MREmu.exe del /q bin\Debug\MREmu.exe
if exist bin\Debug\MREmu.build_run.exe del /q bin\Debug\MREmu.build_run.exe
echo Cleaned.
if defined once exit /b 0
goto menu

:reconfigure
echo.
echo Reconfiguring...
if exist build rmdir /s /q build
call :configure
if errorlevel 1 ( if defined once exit /b %errorlevel% & goto menu )
"%CMAKE%" --build build --config Release
if errorlevel 1 ( echo Build failed. & if defined once exit /b %errorlevel% & goto menu )
call :copy_openal Release
if defined once exit /b 0
goto menu

:configure
"%CMAKE%" -S . -B build -A Win32
exit /b %errorlevel%

:copy_openal
if exist "deps\SFML\extlibs\bin\x86\openal32.dll" (
  if not exist "bin\%~1" mkdir "bin\%~1"
  copy /y "deps\SFML\extlibs\bin\x86\openal32.dll" "bin\%~1\" >nul
)
exit /b 0

:android_menu
call :android_checks
if errorlevel 1 (
  if defined once exit /b 1
  goto menu
)

echo.
echo   Android - what do you want to do?
echo   [0] Back
echo   [1] Build Debug APK
echo   [2] Build Release APK
echo   [3] Install + Launch Debug APK
echo   [4] Install + Launch Release APK
echo   [5] Clean
echo   [6] Build Release APK for a specific ABI -^> Downloads
echo.
echo   Press a listed key. Other keys beep and are ignored.
echo.
if not "%~2"=="" (
  set "achoice=%~2"
  goto run_achoice
)
choice /c:0123456 /n
if errorlevel 8 goto prompt_android
set "achoice=%errorlevel%"
set /a achoice-=1
goto run_achoice

rem Fallback if choice is unavailable.
:prompt_android
set "achoice="
set /p "achoice=Choose 0-6: "

:run_achoice
set "achoice=%achoice: =%"
if "%achoice%"=="0" (
  if defined once exit /b 0
  goto menu
)
if "%achoice%"=="1" goto android_debug
if "%achoice%"=="2" goto android_release
if "%achoice%"=="3" goto android_install
if "%achoice%"=="4" goto android_install_release
if "%achoice%"=="5" goto android_clean
if "%achoice%"=="6" goto android_abi_menu
if defined once (
  echo Unknown Android option "%achoice%".
  exit /b 1
)
goto android_menu

:android_checks
set "ANDROID_DIR=%ROOT%Android"
if not exist "%ANDROID_DIR%\gradlew.bat" goto android_fail
if not exist "%ANDROID_DIR%\local.properties" goto android_fail

set "JAVA_HOME="
for /d %%D in ("%USERPROFILE%\.gradle\jdks\*21*") do if not defined JAVA_HOME if exist "%%D\bin\java.exe" set "JAVA_HOME=%%D"
for /d %%D in ("%USERPROFILE%\.jdks\*21*") do if not defined JAVA_HOME if exist "%%D\bin\java.exe" set "JAVA_HOME=%%D"
for /d %%D in ("C:\Program Files\Eclipse Adoptium\jdk-21*") do if not defined JAVA_HOME if exist "%%D\bin\java.exe" set "JAVA_HOME=%%D"
for /d %%D in ("C:\Program Files\Java\jdk-21*") do if not defined JAVA_HOME if exist "%%D\bin\java.exe" set "JAVA_HOME=%%D"
if not defined JAVA_HOME if exist "C:\Program Files\Android\Android Studio\jbr\bin\java.exe" set "JAVA_HOME=C:\Program Files\Android\Android Studio\jbr"
if not defined JAVA_HOME if defined ANDROID_STUDIO if exist "%ANDROID_STUDIO%\jbr\bin\java.exe" set "JAVA_HOME=%ANDROID_STUDIO%\jbr"
if not defined JAVA_HOME (
  for /f "delims=" %%I in ('where java 2^>nul') do (
    if not defined JAVA_HOME set "JAVA_HOME=%%~dpI.."
  )
)
if not defined JAVA_HOME goto android_fail
if not exist "%JAVA_HOME%\bin\java.exe" goto android_fail

set "PATH=%JAVA_HOME%\bin;%PATH%"
java -version >nul 2>&1
if errorlevel 1 goto android_fail
exit /b 0

:android_fail
echo.
echo Android build prerequisites not met.
echo   - Need Android\gradlew.bat
echo   - Need Android\local.properties with sdk.dir set
echo   - Need Java, such as Android Studio JBR or JAVA_HOME
echo.
exit /b 1

rem Locate adb: local.properties sdk.dir, then ANDROID_SDK_ROOT/ANDROID_HOME, then PATH.
:find_adb
set "ADB="
if defined ANDROID_SERIAL set "ADB_SERIAL=%ANDROID_SERIAL%"
if exist "%ANDROID_DIR%\local.properties" (
  for /f "usebackq tokens=1,* delims==" %%I in (`findstr /b /c:"sdk.dir" "%ANDROID_DIR%\local.properties"`) do (
    if not defined ADB (
      set "SDKDIR=%%J"
      set "SDKDIR=%SDKDIR:\:=%"
      if exist "%SDKDIR%\platform-tools\adb.exe" set "ADB=%SDKDIR%\platform-tools\adb.exe"
    )
  )
)
if not defined ADB if defined ANDROID_SDK_ROOT if exist "%ANDROID_SDK_ROOT%\platform-tools\adb.exe" set "ADB=%ANDROID_SDK_ROOT%\platform-tools\adb.exe"
if not defined ADB if defined ANDROID_HOME if exist "%ANDROID_HOME%\platform-tools\adb.exe" set "ADB=%ANDROID_HOME%\platform-tools\adb.exe"
if not defined ADB if defined LOCALAPPDATA if exist "%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe" set "ADB=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe"
if not defined ADB (
  for /f "delims=" %%I in ('where adb 2^>nul') do if not defined ADB set "ADB=%%I"
)
exit /b 0

rem Pick a device: honour ANDROID_SERIAL, otherwise require exactly one connected.
:android_device
set "ADB_ARGS="
if defined ADB_SERIAL set "ADB_ARGS=-s %ADB_SERIAL%"
set "DEVICES=%ADB% %ADB_ARGS% devices"
set "DEVCOUNT=0"
for /f "usebackq skip=1 tokens=1,2" %%A in (`%DEVICES%`) do (
  if not "%%B"=="unauthorized" if not "%%B"=="offline" (
    set /a DEVCOUNT+=1
    if not defined ADB_SERIAL if not defined ADB_PICKED set "ADB_PICKED=%%A"
  )
)
if "%DEVCOUNT%"=="0" (
  echo.
  echo No Android device connected. Connect the device with USB debugging enabled.
  exit /b 1
)
if "%DEVCOUNT%"=="1" (
  if defined ADB_SERIAL set "ADB_ARGS=-s %ADB_SERIAL%"
) else (
  echo.
  echo %DEVCOUNT% devices connected. Set ANDROID_SERIAL to pick one, for example:
  echo   set ANDROID_SERIAL=%ADB_PICKED%
  exit /b 1
)
exit /b 0

:android_launch
echo.
echo Launching MREmu on device...
call :find_adb
call :android_device
if errorlevel 1 exit /b 1
%ADB% %ADB_ARGS% shell am force-stop %ANDROID_PACKAGE% >nul 2>&1
%ADB% %ADB_ARGS% shell am start -W -n %ANDROID_PACKAGE%/%ANDROID_ACTIVITY% >nul 2>&1
set "LAUNCH_RC=%errorlevel%"
if not "%LAUNCH_RC%"=="0" %ADB% %ADB_ARGS% shell monkey -p %ANDROID_PACKAGE% -c android.intent.category.LAUNCHER 1 >nul 2>&1
if errorlevel 1 (
  echo.
  echo Failed to launch MREmu. Check that the device is unlocked and the app is installed.
  exit /b 1
)
echo MREmu launched.
exit /b 0

:android_abi_menu
echo.
echo   Build a Release APK for which ABI?
echo   [0] Back
echo   [1] arm64-v8a
echo   [2] armeabi-v7a
echo   [3] x86_64
echo.
echo   Press a listed key. Other keys beep and are ignored.
echo.
if not "%~3"=="" (
  set "abichoice=%~3"
  goto run_abichoice
)
choice /c:0123 /n
if errorlevel 5 goto prompt_abi
set "abichoice=%errorlevel%"
set /a abichoice-=1
goto run_abichoice

rem Fallback if choice is unavailable.
:prompt_abi
set "abichoice="
set /p "abichoice=Choose 0-3: "

:run_abichoice
set "abichoice=%abichoice: =%"
if "%abichoice%"=="0" (
  if defined once exit /b 0
  goto android_menu
)
set "ABIPARAM="
if "%abichoice%"=="1" set "ABIPARAM=arm64-v8a"
if "%abichoice%"=="2" set "ABIPARAM=armeabi-v7a"
if "%abichoice%"=="3" set "ABIPARAM=x86_64"
if not defined ABIPARAM (
  rem Without this guard an out-of-range scripted argument re-entered this menu with
  rem the same %~3 still set and spun forever, printing nothing.
  if defined once (
    echo Unknown ABI option "%abichoice%".
    exit /b 1
  )
  goto android_abi_menu
)
call :android_build_abi %ABIPARAM%
if defined once exit /b 0
goto android_abi_menu

rem Builds a Release APK for one ABI and copies it to the user's Downloads as
rem MREmu-<abi>-<version>.apk. Uses -PmremuAbis so the per-ABI native build is
rem isolated from the default single-ABI dev build. Only exit /b here; a goto out
rem of a called subroutine would break the caller's context.
:android_build_abi
set "ABI=%~1"
echo.
echo Building Release APK for %ABI%...
cd /d "%ANDROID_DIR%"
call gradlew.bat assembleRelease -PmremuAbis=%ABI%
set "ANDROID_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%ANDROID_RC%"=="0" (
  echo Release build for %ABI% failed.
  exit /b %ANDROID_RC%
)
set "APK_SRC=%ANDROID_DIR%\app\build\outputs\apk\release\app-release.apk"
if not exist "%APK_SRC%" (
  echo APK not found: "%APK_SRC%".
  exit /b 1
)
set "DL=%USERPROFILE%\Downloads"
if not exist "%DL%" mkdir "%DL%"
set "APK_VERSION=unknown"
for /f "tokens=2 delims==" %%V in ('findstr /c:"android:versionName=" "%ANDROID_DIR%\app\src\main\AndroidManifest.xml"') do set "APK_VERSION=%%V"
rem %%V is like "1.0"> ; drop the surrounding quote and the closing bracket.
set "APK_VERSION=%APK_VERSION:"=%"
set "APK_VERSION=%APK_VERSION:>=%"
set "APK_OUT=%DL%\MREmu-%ABI%-%APK_VERSION%.apk"
copy /y "%APK_SRC%" "%APK_OUT%" >nul
if errorlevel 1 (
  echo Copy to "%APK_OUT%" failed.
  exit /b 1
)
echo.
echo Saved: %APK_OUT%
exit /b 0

:android_debug
cd /d "%ANDROID_DIR%"
call gradlew.bat assembleDebug
set "ANDROID_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%ANDROID_RC%"=="0" (
  echo Android Debug build failed.
  if defined once exit /b %ANDROID_RC%
  goto android_menu
)
echo Android Debug APK build succeeded.
if defined once exit /b 0
goto android_menu

:android_release
cd /d "%ANDROID_DIR%"
call gradlew.bat assembleRelease
set "ANDROID_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%ANDROID_RC%"=="0" (
  echo Android Release build failed.
  if defined once exit /b %ANDROID_RC%
  goto android_menu
)
echo Android Release APK build succeeded.
if defined once exit /b 0
goto android_menu

:android_install
cd /d "%ANDROID_DIR%"
call gradlew.bat installDebug
set "ANDROID_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%ANDROID_RC%"=="0" (
  echo Android install failed.
  if defined once exit /b %ANDROID_RC%
  goto android_menu
)
echo Android Debug APK installed.
call :android_launch
if defined once exit /b 0
goto android_menu

:android_install_release
cd /d "%ANDROID_DIR%"
call gradlew.bat installRelease
set "ANDROID_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%ANDROID_RC%"=="0" (
  echo Android Release install failed.
  if defined once exit /b %ANDROID_RC%
  goto android_menu
)
echo Android Release APK installed.
call :android_launch
if defined once exit /b 0
goto android_menu

:android_clean
cd /d "%ANDROID_DIR%"
call gradlew.bat clean
set "ANDROID_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%ANDROID_RC%"=="0" (
  echo Android clean failed.
  if defined once exit /b %ANDROID_RC%
  goto android_menu
)
echo Android build cleaned.
if defined once exit /b 0
goto android_menu

:web_menu
set "EMSDK_ROOT=%ROOT%\em\emsdk"
set "EM_SDK=%USERPROFILE%\AppData\Local\emscripten\emsdk"
if exist "%EMSDK_ROOT%" set "EM_SDK=%EMSDK_ROOT%"
if exist "C:\Program Files\Emscripten\emsdk" set "EM_SDK=C:\Program Files\Emscripten\emsdk"

set "EM_SDK_EXISTS=0"
for /d %%D in ("%EM_SDK%\*") do if exist "%%D\emsdk_env.bat" set "EM_SDK_EXISTS=1"
if exist "%EM_SDK%\emsdk_env.bat" set "EM_SDK_EXISTS=1"

echo.
echo   Web (Emscripten) - what do you want to do?
echo   [0] Back
echo   [1] Configure + build Release for web
echo   [2] Build Debug for web
echo   [3] Launch web build in browser
echo   [4] Clean web build artifacts
echo.
echo   Press a listed key. Other keys beep and are ignored.
echo.
if not "%~2"=="" (
  set "wchoice=%~2"
  goto run_wchoice
)
choice /c:01234 /n
if errorlevel 6 goto prompt_web
set "wchoice=%errorlevel%"
set /a wchoice-=1
goto run_wchoice

rem Fallback if choice is unavailable.
:prompt_web
set "wchoice="
set /p "wchoice=Choose 0-4: "

:run_wchoice
set "wchoice=%wchoice: =%"
if "%wchoice%"=="0" (
  if defined once exit /b 0
  goto menu
)
if "%wchoice%"=="1" goto web_build_release
if "%wchoice%"=="2" goto web_build_debug
if "%wchoice%"=="3" goto web_launch
if "%wchoice%"=="4" goto web_clean
if defined once (
  echo Unknown Web option "%wchoice%".
  exit /b 1
)
goto web_menu

:web_checks
set "WEB_DIR=%ROOT%em"

set "EM_SDK_FOUND=0"
for /d %%D in ("%EM_SDK%\*") do if exist "%%D\emsdk_env.bat" set "EM_SDK_FOUND=1"
if exist "%EM_SDK%\emsdk_env.bat" set "EM_SDK_FOUND=1"
if "%EM_SDK_FOUND%"=="1" (
  echo   Emscripten SDK found at: %EM_SDK%
  exit /b 0
)

echo.
echo Emscripten SDK not found. Installing it now (one-time, ~1-2 minutes)...
set "EM_INSTALL_DIR=%ROOT%em\emsdk"
if not exist "%EM_INSTALL_DIR%" mkdir "%EM_INSTALL_DIR%"
cd /d "%EM_INSTALL_DIR%"
if not exist "%EM_INSTALL_DIR%\emsdk.bat" (
  git clone https://github.com/emscripten-core/emsdk.git "%EM_INSTALL_DIR%"
  if errorlevel 1 (
    echo git clone of emsdk failed. Check your internet connection.
    cd /d "%ROOT%"
    exit /b 1
  )
)
echo Installing emsdk 3.1.67...
call emsdk install 3.1.67
if errorlevel 1 (
  echo emsdk install failed.
  cd /d "%ROOT%"
  exit /b 1
)
echo Activating emsdk 3.1.67...
call emsdk activate 3.1.67
if errorlevel 1 (
  echo emsdk activate failed.
  cd /d "%ROOT%"
  exit /b 1
)
call emsdk_env.bat >nul 2>&1
if errorlevel 1 (
  echo emsdk_env failed.
  cd /d "%ROOT%"
  exit /b 1
)
set "EM_SDK=%EM_INSTALL_DIR%"
set "EM_SDK_FOUND=1"
cd /d "%ROOT%"
echo Emscripten SDK installed and activated at: %EM_SDK%
exit /b 0

:web_build_release
call :web_checks
if errorlevel 1 ( if defined once exit /b 1 & goto web_menu )
echo.
echo Configuring Emscripten Release build in build\em...
call :web_env
if errorlevel 1 ( if defined once exit /b 1 & goto web_menu )
set "EM_BUILD=%ROOT%build\em"
if not exist "%EM_BUILD%" mkdir "%EM_BUILD%"
cd /d "%EM_BUILD%"
emcmake cmake -S %ROOT% -B . -DCMAKE_BUILD_TYPE=Release -DEMSCRIPTEN=1 -G Ninja
set "EM_CFG_RC=%errorlevel%"
if not "%EM_CFG_RC%"=="0" (
  echo Emscripten configure failed.
  if defined once exit /b %EM_CFG_RC%
  goto web_menu
)
emcmake cmake --build .
set "EM_BUILD_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%EM_BUILD_RC%"=="0" (
  echo Emscripten Release build failed.
  if defined once exit /b %EM_BUILD_RC%
  goto web_menu
)
echo Emscripten Release build succeeded.
call :copy_web_to_bin Release
if defined once exit /b 0
goto web_menu

:web_build_debug
call :web_checks
if errorlevel 1 ( if defined once exit /b 1 & goto web_menu )
echo.
echo Configuring Emscripten Debug build in build\em...
call :web_env
if errorlevel 1 ( if defined once exit /b 1 & goto web_menu )
set "EM_BUILD=%ROOT%build\em"
if not exist "%EM_BUILD%" mkdir "%EM_BUILD%"
cd /d "%EM_BUILD%"
emcmake cmake -S %ROOT% -B . -DCMAKE_BUILD_TYPE=Debug -DEMSCRIPTEN=1 -G Ninja
set "EM_CFG_RC=%errorlevel%"
if not "%EM_CFG_RC%"=="0" (
  echo Emscripten configure failed.
  if defined once exit /b %EM_CFG_RC%
  goto web_menu
)
emcmake cmake --build .
set "EM_BUILD_RC=%errorlevel%"
cd /d "%ROOT%"
if not "%EM_BUILD_RC%"=="0" (
  echo Emscripten Debug build failed.
  if defined once exit /b %EM_BUILD_RC%
  goto web_menu
)
echo Emscripten Debug build succeeded.
call :copy_web_to_bin Debug
if defined once exit /b 0
goto web_menu

:web_env
if exist "%EM_SDK%\emsdk_env.bat" (
  call "%EM_SDK%\emsdk_env.bat" >nul 2>&1
) else (
  for /d %%D in ("%EM_SDK%\*") do if exist "%%D\emsdk_env.bat" call "%%D\emsdk_env.bat" >nul 2>&1
)
if errorlevel 1 (
  echo Failed to activate emsdk environment.
  exit /b 1
)

rem CPM.cmake is downloaded from GitHub at configure time. If SSL inspection
rem blocks the download, pre-place CPM_0.40.2.cmake at
rem   %ROOT%em\emsdk_cache\cpm\CPM_0.40.2.cmake
rem and set CPM_SOURCE_CACHE=%ROOT%em\emsdk_cache to avoid the download.
if not defined CPM_SOURCE_CACHE if exist "%ROOT%em\emsdk_cache\cpm\CPM_0.40.2.cmake" (
  set "CPM_SOURCE_CACHE=%ROOT%em\emsdk_cache"
)
exit /b 0

:copy_web_to_bin
set "WEB_BUILD=%ROOT%build\em"
set "OUT_DIR=%ROOT%bin\%~1"
if exist "%WEB_BUILD%\index.html" (
  if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
  copy /y "%WEB_BUILD%\index.html" "%OUT_DIR%\index.html" >nul
  copy /y "%WEB_BUILD%\index.js" "%OUT_DIR%\index.js" >nul 2>&1
  copy /y "%WEB_BUILD%\index.wasm" "%OUT_DIR%\index.wasm" >nul 2>&1
  copy /y "%WEB_BUILD%\index.data" "%OUT_DIR%\index.data" >nul 2>&1
  echo Web artifacts copied to: %OUT_DIR%
) else (
  echo No index.html found in "%WEB_BUILD%". Build first.
  exit /b 1
)
exit /b 0

:web_launch
set "WEB_BUILD=%ROOT%bin\Release"
if not exist "%WEB_BUILD%\index.html" (
  echo Web build not found. Build first [1].
  if defined once exit /b 1
  goto web_menu
)
echo.
echo Launching web build in default browser...
start "" "%WEB_BUILD%\index.html"
if defined once exit /b 0
goto web_menu

:web_clean
echo.
echo Cleaning web build artifacts...
if exist "build\em" rmdir /s /q "build\em"
if exist "bin\Release\index.html" del /q bin\Release\index.html >nul 2>&1
if exist "bin\Release\index.js" del /q bin\Release\index.js >nul 2>&1
if exist "bin\Release\index.wasm" del /q bin\Release\index.wasm >nul 2>&1
if exist "bin\Release\index.data" del /q bin\Release\index.data >nul 2>&1
echo Web build cleaned.
if defined once exit /b 0
goto web_menu

:done
echo.
echo Goodbye.
endlocal
exit /b 0
