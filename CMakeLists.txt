# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("MREmu")

# Include sub-projects.
add_subdirectory (imgui)

add_definitions( -DLIBICONV_PLUG)
set(BUILD_SHARED_LIBS FALSE) 
set(UNICORN_ARCH "arm" CACHE STRING "Supported architectures") 
add_subdirectory(SFML) 
add_subdirectory(unicorn) 
add_subdirectory(libiconv-cmake) 

set(CAPSTONE_ARCHITECTURE_DEFAULT FALSE) 
set(CAPSTONE_ARM_SUPPORT TRUE) 
add_subdirectory(capstone) 

add_subdirectory(libADLMIDI) 

add_subdirectory (MREmu)