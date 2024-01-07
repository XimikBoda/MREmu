cmake_minimum_required (VERSION 3.8)

file(GLOB imgui_SRC
     "*.h"
     "*.cpp"
)

add_library(imgui ${imgui_SRC})

target_link_libraries(imgui sfml-graphics)

target_include_directories(imgui PUBLIC ".")