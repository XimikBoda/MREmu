set(BUILD_SHARED_LIBS FALSE) 

set(CMAKE_FOLDER "Deps")


if(BUILTIN_GDB)
    set(SFML_BUILD_NETWORK TRUE)
else()
    set(SFML_BUILD_NETWORK FALSE)
endif()

set(UNICORN_ARCH "arm" CACHE STRING "Supported architectures") 
if(NOT EMSCRIPTEN)
    add_subdirectory(SFML)
    add_subdirectory(unicorn) 
else()
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/Zombieschannel/SFML
        GIT_TAG 52b08b0703ba7266cb1414535bce8b513f7c3490
    )	
    FetchContent_MakeAvailable(SFML)

    target_include_directories(sfml-audio PRIVATE "$ENV{EMSDK}/upstream/emscripten/cache/sysroot/include/AL")
    
    FetchContent_Declare(
        unicorn
        GIT_REPOSITORY https://github.com/XimikBoda/unicorn
        GIT_TAG 854ddf851c6c194707c596bd403bf68dcb2bf51b
    )	
    FetchContent_MakeAvailable(unicorn)
endif()

if(DEBUG_WINDOW)
    add_subdirectory(imgui)
endif()

add_subdirectory(libiconv-cmake) 

if(CAPSTONE)
    set(CAPSTONE_ARCHITECTURE_DEFAULT FALSE) 
    set(CAPSTONE_ARM_SUPPORT TRUE) 
    add_subdirectory(capstone) 
endif()

add_subdirectory(libADLMIDI) 

add_subdirectory(spdlog) 

add_subdirectory(asio.cmake) 

add_subdirectory(miniz) 

add_subdirectory(mresdk) 

unset(CMAKE_FOLDER)