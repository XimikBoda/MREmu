cmake_minimum_required (VERSION 3.8)

add_library (MREngine 
						"Graphic.cpp" "Graphic.h"
						"System.cpp" "System.h" 
						"Resources.cpp" "Resources.h"
						"Timer.cpp" "Timer.h"
						"ProgramManager.cpp" "ProgramManager.h"
						"IO.cpp" "IO.h"
						"SIM.cpp" "SIM.h"
						"Sock.cpp" "Sock.h"
						"Audio.cpp" "Audio.h"
						"CharSet.cpp" "CharSet.h"
						"Textout.cpp" 
						"STDLib.cpp" 
						"ItemsMng.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MREngine PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MREngine sfml-graphics)
target_link_libraries(MREngine imgui)
target_link_libraries(MREngine ADLMIDI_static)
target_link_libraries(MREngine libiconv)

target_include_directories(imgui PUBLIC "$ENV{MRE_SDK}/include")