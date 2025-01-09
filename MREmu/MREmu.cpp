#include <iostream>
#include <thread>

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Memory.h"
#include "Cpu.h"
#include "GDB.h"
#include "Bridge.h"
#include "App.h"
#include "AppManager.h"
#include "Keyboard.h"

#include "MREngine/Graphic.h"
#include "MREngine/IO.h"
#include "MREngine/SIM.h"
#include "MREngine/CharSet.h"
#include <cmdparser.hpp>

sf::Clock global_clock;

bool work = true;

AppManager* g_appManager = 0;

void mre_main(AppManager* appManager_p) {
	AppManager& appManager = *appManager_p;

	sf::Clock deltaClock;
	while (work) {
		uint32_t delta_ms = deltaClock.restart().asMilliseconds();

		GDB::update();
		appManager.update(delta_ms);

		sf::sleep(sf::milliseconds(1000 / 60));
	}
}

int main(int argc, char** argv) {
	cli::Parser parser(argc, argv);
	{
		parser.set_optional<std::string>("", "", "", "Path to vxp");
		parser.set_optional<bool>("l", "path_is_local", false, "Set to run from local filesystem");
		parser.set_optional<bool>("g", "gdb", false, "Set to run gdb server");
		parser.set_optional<int>("p", "gdb_port", 1234, "Port for gdb server");
	}
	parser.run_and_exit_if_error();
	auto app_path = parser.get<std::string>("");
	bool path_is_local = parser.get<bool>("l");

	GDB::gdb_mode = parser.get<bool>("g");
	GDB::gdb_port = parser.get<int>("p");

	fs::current_path(fs::path(argv[0]).parent_path());

	if(GDB::gdb_mode)
		GDB::wait();

	Memory::init(32 * 1024 * 1024);
	Cpu::init();
	Bridge::init();

	MREngine::IO::init();
	MREngine::SIM::init();
	MREngine::CharSet::init();
	MREngine::Graphic graphic;

	AppManager appManager;
	g_appManager = &appManager;

	if (GDB::gdb_mode)
		GDB::cpu_state = GDB::Stop;

	std::thread second_thread(mre_main, &appManager);

	sf::RenderWindow win_debug(sf::VideoMode::getDesktopMode(), "MREmu Debug");
	ImGui::SFML::Init(win_debug);
	win_debug.setFramerateLimit(60);
	//win_debug.setVerticalSyncEnabled(true);

	Keyboard keyboard;

	if (app_path.size())
		if (fs::exists(app_path) || path_is_local)
			appManager.add_app_for_launch(app_path, path_is_local);
		else {
			printf("vxp file don't exists\n");
			exit(1);
		}

	keyboard.update_pos_and_size(0, graphic.height, 240, 208);

	sf::Clock fps;

	sf::Clock deltaClock;
	sf::Event event;
	while (win_debug.isOpen()) {
		while (win_debug.pollEvent(event)) {
			sf::IntRect kb_rect(keyboard.x, keyboard.y, keyboard.w, keyboard.h);;
			ImGui::SFML::ProcessEvent(event);
			keyboard.keyboard_event(event);
			switch (event.type) {
			case sf::Event::Closed:
				win_debug.close();
				break;
			case sf::Event::Resized:
				win_debug.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
				break;
			case sf::Event::MouseButtonPressed:
				if (event.mouseButton.button == sf::Mouse::Button::Left) {
					if (event.mouseButton.x >= kb_rect.left &&
						event.mouseButton.x < kb_rect.left + kb_rect.width &&
						event.mouseButton.y >= kb_rect.top &&
						event.mouseButton.y < kb_rect.top + kb_rect.height)
						keyboard.kc.press_key(keyboard.find_key_by_pos(event.mouseButton.x - kb_rect.left,
							event.mouseButton.y - kb_rect.top), keyboard.kc.Mouse);
				}
				break;
			case sf::Event::MouseButtonReleased:
				if (event.mouseButton.button == sf::Mouse::Button::Left)
					keyboard.kc.unpress_by_source(keyboard.kc.Mouse);
				break;
			}
		}
		ImGui::SFML::Update(win_debug, deltaClock.restart());

		graphic.update_screen();
		graphic.imgui_screen();
		App* active_app = appManager.get_active_app();
		if (active_app) {
			active_app->graphic.imgui_layers();
			active_app->graphic.imgui_canvases();
		}

		if (ImGui::Begin("Memory") && active_app) {
			float size = active_app->app_memory.get_memory_size();
			float free_size = active_app->app_memory.get_free_memory_size();
			float used_size = size - free_size;
			ImGui::Text("All:\n%1.0f bytes\n%1.1f kb\n %1.3f mb\n", 
				size, size / 1024.f, size / 1024.f / 1024.f);
			ImGui::Text("Free:\n%1.0f bytes\n%1.1f kb\n %1.3f mb\n", 
				free_size, free_size / 1024.f, free_size / 1024.f / 1024.f);
			ImGui::Text("Used:\n%1.0f bytes\n%1.1f kb\n %1.3f mb\n", 
				used_size, used_size / 1024.f, used_size / 1024.f / 1024.f);
			ImGui::Text("Used: %1.2f%%%", 100.f*used_size / size);
		}
		ImGui::End();

		if (ImGui::Begin("Fps")) {
			ImGui::Text("%1.3f", 1.f / fps.restart().asSeconds());
		}
		ImGui::End();

		{
			sf::Sprite screen(graphic.screen_tex);
			//screen.setScale(2, 2);
			win_debug.draw(screen);
		}

		Cpu::imgui_REG();

		keyboard.imgui_keyboard();
		keyboard.draw(&win_debug);

		ImGui::SFML::Render(win_debug);
		win_debug.display();
		win_debug.clear();
	}

	work = false;
	second_thread.join();

	ImGui::SFML::Shutdown();
	return 0;
}