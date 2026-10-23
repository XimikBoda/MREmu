add_library (MREngine 
						"Graphic.cpp" "Graphic.h"
						"GraphicOld.cpp" "GraphicOld.h"
						"Canvas.cpp" "Canvas.h" "Image.cpp"
						"Blt.cpp" "Blt.h"

						"System.cpp" "System.h" 
						"Resources.cpp" "Resources.h"
						"Tags.cpp" "Tags.h"
						"Timer.cpp" "Timer.h"
						"ProgramManager.cpp" "ProgramManager.h"
						"IO.cpp" "IO.h"
						"SIM.cpp" "SIM.h"
						"Log.cpp"
						"Sock.cpp" "Sock.h" "SockImpl.h"
						"Sensor.cpp" "Sensor.h"
						"Audio.cpp" "Audio.h" "AudioBitstream.cpp" "AudioBitstreamRec.cpp"
						"CharSet.cpp" "CharSet.h"
						"Textout.cpp" 
						"STDLib.cpp" 
						"ItemsMng.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MREngine PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MREngine sfml-graphics)
target_link_libraries(MREngine sfml-audio)
target_link_libraries(MREngine ADLMIDI_static)
target_link_libraries(MREngine libiconv)
target_link_libraries(MREngine spdlog)
target_link_libraries(MREngine asio::asio)
target_link_libraries(MREngine mresdk)

if(DEBUG_WINDOW)
    target_link_libraries(MREngine imgui)
    target_compile_definitions(MREngine PUBLIC DEBUG_WINDOW)
endif()