@echo off
rem MREmu launcher: configure, build, run, and clean using the
rem Visual Studio (Win32 / x86) CMake generator.
rem   Usage: launch.bat [0-6]   (no argument = interactive menu)
rem     [0] -> exit
rem     [1] -> build Release
rem     [2] -> build Debug
rem     [3] -> run Release
rem     [4] -> run Debug
rem     [5] -> clean build artifacts
rem     [6] -> reconfigure + build Release
rem     [7] -> Android build menu
setlocal

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
echo.
set "choice="
set /p "choice=Choose 0-6: "

:run_choice
if "%choice%"=="0" goto done
if "%choice%"=="1" goto build_release
if "%choice%"=="2" goto build_debug
if "%choice%"=="3" goto run_release
if "%choice%"=="4" goto run_debug
if "%choice%"=="5" goto clean
if "%choice%"=="6" goto reconfigure
if "%choice%"=="7" goto android_menu
if defined once (
  echo Unknown option "%~1". Use 0, 1, 2, 3, 4, 5, 6 or 7.
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
echo Starting MREmu (Release)...
start "" "bin\Release\MREmu.exe"
if defined once exit /b 0
goto menu

:run_debug
if not exist "bin\Debug\MREmu.exe" (
  echo Debug build not found. Build first [2].
  if defined once exit /b 1
  goto menu
)
echo.
echo Starting MREmu (Debug)...
start "" "bin\Debug\MREmu.exe"
if defined once exit /b 0
goto menu

:clean
echo.
echo Cleaning build artifacts...
if exist build rmdir /s /q build
if exist bin\Release\MREmu.exe del /q bin\Release\MREmu.exe
if exist bin\Debug\MREmu.exe del /q bin\Debug\MREmu.exe
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
echo   [3] Install Debug APK
echo   [4] Clean
echo.
set "achoice="
set /p "achoice=Choose 0-4: "
set "achoice=%achoice: =%"
if "%achoice%"=="0" (
  if defined once exit /b 0
  goto menu
)
if "%achoice%"=="1" goto android_debug
if "%achoice%"=="2" goto android_release
if "%achoice%"=="3" goto android_install
if "%achoice%"=="4" goto android_clean
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

:done
echo.
echo Goodbye.
endlocal
exit /b 0
