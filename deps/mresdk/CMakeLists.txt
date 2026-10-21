set(TARGET_NAME "mresdk")

set(MRE_SDK "$ENV{MRE_SDK}" CACHE PATH "Path to MRE SDK directory")

if(NOT MRE_SDK)
    message(FATAL_ERROR "Variable MRE_SDK is not set. Please set the ENV variable or pass -DMRE_SDK=...")
endif()

SET(MRE_SDK_INCLUDE_DIR "${MRE_SDK}/include")

if(NOT EXISTS "${MRE_SDK_INCLUDE_DIR}")
    message(FATAL_ERROR "Include directory not found: ${MRE_SDK_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${MRE_SDK_INCLUDE_DIR}/vmsys.h")
    message(FATAL_ERROR "Header file not found: ${MRE_SDK_INCLUDE_DIR}/vmsys.h")
endif()

message(STATUS "Found MRE_SDK: ${MRE_SDK}")

file(GLOB sdk_headers CONFIGURE_DEPENDS "${MRE_SDK_INCLUDE_DIR}/*.h")
file(GLOB local_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(${TARGET_NAME} STATIC ${sdk_headers} ${local_headers} null.c)

target_include_directories(${TARGET_NAME} PUBLIC
    "${MRE_SDK_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

source_group("MRE SDK" FILES ${sdk_headers})
source_group("Local Headers" FILES ${local_headers})