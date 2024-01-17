#include <iostream>

#include "imgui.h"
#include "imgui-SFML.h"

#include "Memory.h"
#include "Cpu.h"
#include "Bridge.h"
#include "App.h"

#include "MREngine/Graphic.h"

int main(int argc, char** argv) {
    // Parse command arguments
    if (argc < 2) {
        std::cout << "MREmu by Ximik_Boda\n";
        std::cout << "Current supported file types: vxp\n";
        std::cout << "Usage: " << argv[0] << " [your_file]\n";
        return 0;
    }
    
    // Init subsystems
    Memory::init(128 * 1024 * 1024);
    Cpu::init();
    Bridge::init();

    MREngine::Graphic graphic;

    // Load and start app
    App app;
    app.load_from_file(argv[1]);
    app.preparation();
    app.start();

    return 0;
}
