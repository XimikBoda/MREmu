#include <iostream>

#include "imgui.h"
#include "imgui-SFML.h"

#include "Memory.h"


int main() {
    Memory::init(128 * 1024 * 1024);

    return 0;
}
