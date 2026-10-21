cmake_minimum_required (VERSION 3.8)

set(CMAKE_FOLDER "MREmu")

set(SOURCES 
						"MREmu.cpp" "MREmu.h" 
						"Memory.cpp" "Memory.h" 
						"Cpu.cpp" "Cpu.h" 
						"GDB.cpp" "GDB.h" 
						"Bridge.cpp" "Bridge.h" 
						"App.cpp" "App.h" 
						"ArmApp.cpp" "ArmApp.h" 
						"DLLApp.cpp" "DLLApp.h" 
						"NativeApp.cpp" "NativeApp.h" 
						"MreTags.cpp" "MreTags.h"
						"MREngine/VxpIcon.cpp" "MREngine/VxpIcon.h"
						"Disassembler.cpp" "Disassembler.h"
						"ARModule.cpp" "ARModule.h"
						"AppManager.cpp" "AppManager.h"
						"Log.cpp" "Log.h" 
						"miniz.c" "miniz.h" "mutex_wrapper.h" "Queue.h"
						"jni/Vibration.cpp" "jni/Vibration.h"
						"jni/ScreenCorner.cpp" "jni/ScreenCorner.h"
						"DragAndDrop.cpp" "DragAndDrop.h"
)

# Headless agent control. Desktop only: on Android the options sheet already drives
# the app and there is no stdio transport to serve. Emscripten builds exclude MCP
# since there's no stdio server there either.
if (NOT ANDROID AND NOT EMSCRIPTEN)
	list(APPEND SOURCES
					"McpServer.cpp" "McpServer.h"
					"McpHost.cpp")
endif()

if (ANDROID)
	add_library(MREmu SHARED ${SOURCES})
else()
	add_executable(MREmu ${SOURCES})
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MREmu PROPERTY CXX_STANDARD 20)
endif()

if (EMSCRIPTEN)
  # Emit a self-contained index.html/index.js/index.wasm/index.data bundle,
  # which is the entry point launch.bat copies and tools/web_serve.py serves.
  # Emscripten would otherwise name it MREmu.js and emit no page at all.
  # The binary dir on purpose: without an explicit output directory CMake puts
  # the bundle in build\em\MREmu, but the launcher, the copy step and
  # tools/web_serve.py all expect it at the top of build\em.
  set_target_properties(MREmu PROPERTIES
    OUTPUT_NAME index
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_options(MREmu PRIVATE "--shell-file=${PROJECT_SOURCE_DIR}/web/shell.html")

  # The shell is embedded by the link step, so it must force a relink when it
  # changes; otherwise Ninja sees no dependency and leaves the old page behind.
  # The two copied scripts are listed for the same reason: their post-build
  # copy only runs after a link.
  set_property(TARGET MREmu APPEND PROPERTY LINK_DEPENDS
    "${PROJECT_SOURCE_DIR}/web/shell.html"
    "${PROJECT_SOURCE_DIR}/web/mremu-web.js"
    "${PROJECT_SOURCE_DIR}/web/coi-serviceworker.js")

  # The page loads these next to index.js; emcc only embeds the shell itself.
  # coi-serviceworker.js is what makes the bundle work on a static host such as
  # GitHub Pages, which cannot send COOP/COEP headers itself.
  add_custom_command(TARGET MREmu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${PROJECT_SOURCE_DIR}/web/mremu-web.js"
      "${PROJECT_SOURCE_DIR}/web/coi-serviceworker.js"
      "$<TARGET_FILE_DIR:MREmu>"
    COMMAND ${CMAKE_COMMAND} -E touch "$<TARGET_FILE_DIR:MREmu>/.nojekyll"
    COMMENT "Copying web host assets next to index.html")
endif()

target_link_libraries(MREmu sfml-graphics)
target_link_libraries(MREmu sfml-network)
target_link_libraries(MREmu sfml-audio)
target_link_libraries(MREmu unicorn)
target_link_libraries(MREmu ADLMIDI_static)
target_link_libraries(MREmu libiconv)
target_link_libraries(MREmu spdlog)

target_link_libraries(MREmu MREngine)
target_link_libraries(MREmu NativeApps)
target_link_libraries(MREmu Device)

if(DEBUG_WINDOW)
    target_link_libraries(MREmu imgui)
    target_compile_definitions(MREmu PRIVATE DEBUG_WINDOW)
endif()

if(CAPSTONE)
	target_link_libraries(MREmu capstone)
    target_compile_definitions(MREmu PRIVATE CAPSTONE)
endif()

if(WIN32)
	target_link_libraries(MREmu shell32 dwmapi)
endif()

if (ANDROID)
	target_link_libraries(MREmu Android -Wl,--whole-archive sfml-main -Wl,--no-whole-archive)

	add_library(AL SHARED IMPORTED) 
	set_target_properties(AL PROPERTIES IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/deps/SFML/extlibs/libs-android/${CMAKE_ANDROID_ARCH_ABI}/libopenal.so")
	target_link_libraries(MREmu AL)
endif()

add_subdirectory (MREngine)
add_subdirectory (NativeApps)
add_subdirectory (Device)

unset(CMAKE_FOLDER)