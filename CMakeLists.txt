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

option(CAPSTONE "Endble capstone" OFF)

if(CAPSTONE)
    add_definitions(-DCAPSTONE)
endif()

add_definitions(-DLIBICONV_PLUG)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if (MSVC)
  add_compile_options(/wd4828)
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:LIBCMTD")
endif()

set(BUILD_SHARED_LIBS FALSE) 

include_directories("include")

add_subdirectory (deps)

add_subdirectory (MREmu)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MREmu)
