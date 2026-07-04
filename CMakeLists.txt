cmake_minimum_required (VERSION 3.8)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("MREmu")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Enable optimizations by default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

add_definitions(-DLIBICONV_PLUG)

# Include sub-projects.
add_subdirectory (imgui)

set(BUILD_SHARED_LIBS FALSE) 

set(UNICORN_ARCH "arm" CACHE STRING "Supported architectures") 
add_subdirectory(unicorn) 

add_subdirectory (deps)

add_subdirectory (MREmu)
