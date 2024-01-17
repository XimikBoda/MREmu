cmake_minimum_required (VERSION 3.8)

add_executable (MREmu 
						"MREmu.cpp" "MREmu.h" 
						"Memory.cpp" "Memory.h" 
						"Cpu.cpp" "Cpu.h" 
						"Bridge.cpp" "Bridge.h" 
						"App.cpp" "App.h" 
						"MreTags.cpp" "MreTags.h" 
						"Disassembler.cpp" "Disassembler.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MREmu PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MREmu sfml-graphics)
target_link_libraries(MREmu imgui)
target_link_libraries(MREmu unicorn)
target_link_libraries(MREmu capstone)
target_link_libraries(MREmu MREngine)

target_include_directories(MREmu PUBLIC "../include")
target_include_directories(MREmu PUBLIC "MREngine")

add_subdirectory (MREngine)