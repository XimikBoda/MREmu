set(BUILD_SHARED_LIBS FALSE) 

add_subdirectory(SFML)

add_subdirectory(imgui)

add_subdirectory(libiconv-cmake) 

if(CAPSTONE)
    set(CAPSTONE_ARCHITECTURE_DEFAULT FALSE) 
    set(CAPSTONE_ARM_SUPPORT TRUE) 
    add_subdirectory(capstone) 
endif()

add_subdirectory(libADLMIDI) 

set(UNICORN_ARCH "arm" CACHE STRING "Supported architectures") 
add_subdirectory(unicorn) 