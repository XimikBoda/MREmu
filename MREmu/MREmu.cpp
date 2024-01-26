#include <iostream>
#include <thread>

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Memory.h"
#include "Cpu.h"
#include "Bridge.h"
#include "App.h"
#include "AppManager.h"

#include "MREngine/Graphic.h"
#include "MREngine/IO.h"
#include "MREngine/SIM.h"

sf::Clock global_clock;

bool work = true;

AppManager* g_appManager = 0;

void mre_main(AppManager* appManager_p) {
	AppManager& appManager = *appManager_p;

	sf::Clock deltaClock;
	while (work) {
		uint32_t delta_ms = deltaClock.restart().asMilliseconds();

		appManager.launch_apps();
		//app.timer.update(delta_ms);

		sf::sleep(sf::milliseconds(1000/60));
	}
}

int main() {
    Memory::init(128 * 1024 * 1024);
    Cpu::init();
    Bridge::init();

	MREngine::IO::init();
	MREngine::SIM::init();
	MREngine::Graphic graphic;

	AppManager appManager;
	g_appManager = &appManager;

	std::thread second_thread(mre_main, &appManager);

    sf::RenderWindow win(sf::VideoMode::getDesktopMode(), "MREmu");
    ImGui::SFML::Init(win);
    win.setFramerateLimit(60);

	appManager.add_app_for_launch("minecraft.vxp", true);

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
		App* active_app = appManager.get_active_app();
		if (active_app) {
			active_app->graphic.imgui_layers();
			active_app->graphic.imgui_canvases();
		}

		Cpu::imgui_REG();

		ImGui::SFML::Render(win);
		win.display();
		win.clear();
	}

	work = false;
	second_thread.join();

	ImGui::SFML::Shutdown();
    return 0;
}
