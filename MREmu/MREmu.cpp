#include <iostream>

#include "imgui.h"
#include "imgui-SFML.h"

#include "Memory.h"
#include "Cpu.h"
#include "Bridge.h"
#include "App.h"

#include "MREngine/Graphic.h"


int main() {
    Memory::init(128 * 1024 * 1024);
    Cpu::init();
    Bridge::init();

    MREngine::Graphic graphic;

    App app;
    app.load_from_file("3D_test.vxp");
    app.preparation();
    app.start();

    return 0;
}
