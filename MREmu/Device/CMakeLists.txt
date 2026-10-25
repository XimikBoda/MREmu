add_library (Device 
						"Device.cpp" "Device.h"
						"Keyboard.cpp" "Keyboard.h"
						"KeyboardControl.cpp" "KeyboardControl.h"
						"Menubar.cpp" "Menubar.h"
						"Touch.cpp" "Touch.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Device PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(Device sfml-graphics)
target_link_libraries(Device MREngine)