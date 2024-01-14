#include <iostream>

#include "imgui.h"
#include "imgui-SFML.h"

#include "Memory.h"
#include "Cpu.h"
#include "App.h"


int main() {
    Memory::init(128 * 1024 * 1024);
    Cpu::init();

    App app;
    app.load_from_file("3D_test.vxp");
    app.preparation();

    return 0;
}
