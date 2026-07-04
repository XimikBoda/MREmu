set(BUILD_SHARED_LIBS FALSE) 

add_subdirectory(SFML)

add_subdirectory(libiconv-cmake) 

set(CAPSTONE_ARCHITECTURE_DEFAULT FALSE) 
set(CAPSTONE_ARM_SUPPORT TRUE) 
add_subdirectory(capstone) 

add_subdirectory(libADLMIDI) 