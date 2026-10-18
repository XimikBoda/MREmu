set(BUILD_SHARED_LIBS FALSE) 

set(CMAKE_FOLDER "Deps")

add_subdirectory(SFML)

add_subdirectory(imgui)

add_subdirectory(libiconv-cmake) 

if(CAPSTONE)
    set(CAPSTONE_ARCHITECTURE_DEFAULT FALSE) 
    set(CAPSTONE_ARM_SUPPORT TRUE) 
    add_subdirectory(capstone) 
endif()

add_subdirectory(libADLMIDI) 

add_subdirectory(spdlog) 

set(UNICORN_ARCH "arm" CACHE STRING "Supported architectures") 
add_subdirectory(unicorn) 

add_subdirectory(asio.cmake) 

unset(CMAKE_FOLDER)