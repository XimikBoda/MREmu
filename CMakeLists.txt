cmake_minimum_required (VERSION 3.8)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project ("MREmu")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if(NOT EMSCRIPTEN AND NOT ANDROID)
    option(DEBUG_WINDOW "Turn on additional debugging windows" ON)
else()
    set(DEBUG_WINDOW FALSE)
endif()

option(CAPSTONE "Endble capstone" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Enable optimizations by default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

if(EMSCRIPTEN)
    add_compile_options(-pthread)
    
    add_link_options(
        "-pthread"
        "-sPTHREAD_POOL_SIZE=4"
        #"-sPROXY_TO_PTHREAD=1"
        "-sINITIAL_MEMORY=67108864"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sUSE_OGG=1"
        "-sUSE_VORBIS=1"
        "-sEMULATE_FUNCTION_POINTER_CASTS=1"
        "-sSUPPORT_LONGJMP=emscripten"
        "-sSTACK_SIZE=4mb"
        "-sASYNCIFY=1"
        "-sFULL_ES2=1"
        "-sMIN_WEBGL_VERSION=1"
        "-sMAX_WEBGL_VERSION=1"
        "-sGL_EXPLICIT_UNIFORM_LOCATION=1"
        "-sGL_EXPLICIT_UNIFORM_BINDING=1"
        "-sFORCE_FILESYSTEM=1"
        "--preload-file" "${PROJECT_SOURCE_DIR}/bin/Release/fs@/fs"
        "-sWASM=1"
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g3) 
        add_link_options(
            "-g3"
            "-sASSERTIONS=2"
            "-sALLOW_TABLE_GROWTH=1" 
            "-sEXPORT_ALL=1"
        )
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -flto)
        add_link_options(
            "-O3"
            "-flto"
            "-sASSERTIONS=0"
            "-sASYNCIFY_IGNORE_INDIRECT=1"
        )
    endif()
endif()

add_definitions(-DLIBICONV_PLUG)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if (MSVC)
  add_compile_options(/wd4828)
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:LIBCMTD")
endif()

set(BUILD_SHARED_LIBS FALSE) 

# Use ccache when available to speed up C++ rebuilds (Android + desktop).
if(NOT MSVC)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
  endif()
endif()

include_directories("include")

add_subdirectory (deps)

add_subdirectory (MREmu)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MREmu)
