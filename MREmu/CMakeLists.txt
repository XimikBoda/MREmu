cmake_minimum_required (VERSION 3.8)

add_executable (MREmu "MREmu.cpp" "MREmu.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MREmu PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MREmu sfml-graphics)
target_link_libraries(MREmu imgui)
target_link_libraries(MREmu unicorn)