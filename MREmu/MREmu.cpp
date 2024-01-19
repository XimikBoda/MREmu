#include <iostream>

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

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

    sf::RenderWindow win(sf::VideoMode::getDesktopMode(), "MREmu");
    ImGui::SFML::Init(win);
    win.setFramerateLimit(60);

    App app;
    app.load_from_file("3D_test.vxp");
    app.preparation();
    app.start();

	sf::Clock deltaClock;
	sf::Event event;
	while (win.isOpen()) {
		while (win.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);
			switch (event.type) {
			case sf::Event::Closed:
				win.close();
				break;
			case sf::Event::Resized:
				win.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
				break;
			}
		}
		ImGui::SFML::Update(win, deltaClock.restart());

		graphic.imgui_screen();
		app.graphic.imgui_layers();

		ImGui::SFML::Render(win);
		win.display();
		win.clear();
	}
	ImGui::SFML::Shutdown();

    return 0;
}
