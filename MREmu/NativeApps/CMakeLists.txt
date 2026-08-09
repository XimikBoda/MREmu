set(TARGET_NAME "NativeApps")

FILE(GLOB_RECURSE headers CONFIGURE_DEPENDS "*.h")
FILE(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.cpp")

add_library (${TARGET_NAME} ${sources} ${headers})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}  FILES ${sources} ${headers})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(${TARGET_NAME} MREngine)

target_include_directories(${TARGET_NAME} PRIVATE ".")
