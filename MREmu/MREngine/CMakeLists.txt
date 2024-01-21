cmake_minimum_required (VERSION 3.8)

add_library (MREngine 
						"Graphic.cpp" "Graphic.h"
						"System.cpp" "System.h" 
						"Resources.cpp" "Resources.h"
						"Timer.cpp" "Timer.h"
						"ProgramManager.cpp" "ProgramManager.h")

target_link_libraries(MREngine sfml-graphics)
target_link_libraries(MREngine imgui)

target_include_directories(imgui PUBLIC "$ENV{MRE_SDK}/include")