cmake_minimum_required (VERSION 3.8)

set(SOURCES 
						"MREmu.cpp" "MREmu.h" 
						"Memory.cpp" "Memory.h" 
						"Cpu.cpp" "Cpu.h" 
						"GDB.cpp" "GDB.h" 
						"Bridge.cpp" "Bridge.h" 
						"App.cpp" "App.h" 
						"ArmApp.cpp" "ArmApp.h" 
						"DLLApp.cpp" "DLLApp.h" 
						"MreTags.cpp" "MreTags.h" 
						"Disassembler.cpp" "Disassembler.h"
						"ARModule.cpp" "ARModule.h"
						"AppManager.cpp" "AppManager.h"
						"Keyboard.cpp" "Keyboard.h"
						"miniz.c" "miniz.h" "mutex_wrapper.h" )

if (ANDROID)
	add_library(MREmu SHARED ${SOURCES})
else()
	add_executable(MREmu ${SOURCES})
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MREmu PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MREmu sfml-graphics)
target_link_libraries(MREmu sfml-network)
target_link_libraries(MREmu sfml-audio)
target_link_libraries(MREmu imgui)
target_link_libraries(MREmu unicorn)
target_link_libraries(MREmu capstone)
target_link_libraries(MREmu ADLMIDI_static)
target_link_libraries(MREmu MREngine)
target_link_libraries(MREmu libiconv)

if (ANDROID)
	target_link_libraries(MREmu Android -Wl,--whole-archive sfml-main -Wl,--no-whole-archive)

	add_library(AL SHARED IMPORTED) 
	set_target_properties(AL PROPERTIES IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/SFML/extlibs/libs-android/${CMAKE_ANDROID_ARCH_ABI}/libopenal.so") 
	target_link_libraries(MREmu AL)
endif()

target_include_directories(MREmu PUBLIC "../include")

add_subdirectory (MREngine)