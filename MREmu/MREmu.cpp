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
#include "Touch.h"
#include "Log.h"

#include "MREngine/Graphic.h"
#include "MREngine/IO.h"
#include "MREngine/SIM.h"
#include "MREngine/CharSet.h"
#include "MREngine/SystemTextBox.h"
#include <cmdparser.hpp>

#include "NativeApps/Menu/AppSelector.h"
#include "NativeApp.h"
#include "ArmApp.h"
#include "DLLApp.h"

AppManager* g_appManager = 0;

sf::Texture u16text_to_texture(std::u16string str, sf::Color c);

#include "DragAndDrop.h"

using DragAndDrop::show_warning;
using DragAndDrop::warning_clock;

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif


static void open_folder(const fs::path& p) {
	fs::create_directories(p);
	fs::path abs_p = fs::absolute(p);
#ifdef _WIN32
	ShellExecuteW(NULL, L"open", abs_p.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
	std::string cmd = "open \"" + abs_p.string() + "\" &";
	system(cmd.c_str());
#else
	std::string cmd = "xdg-open \"" + abs_p.string() + "\" &";
	system(cmd.c_str());
#endif
}

static void clear_user_data(const fs::path& root_fs) {
	std::error_code ec;
	if (!fs::exists(root_fs, ec))
		return;

	std::vector<fs::path> files_to_delete;
	for (auto it = fs::recursive_directory_iterator(root_fs, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (ec) break;
		if (it->is_regular_file(ec)) {
			std::string ext = it->path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
			if (ext != ".vxp") {
				files_to_delete.push_back(it->path());
			}
		}
	}

	for (const auto& f : files_to_delete) {
		fs::remove(f, ec);
	}

	std::vector<fs::path> dirs_to_check;
	for (auto it = fs::recursive_directory_iterator(root_fs, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (ec) break;
		if (it->is_directory(ec)) {
			dirs_to_check.push_back(it->path());
		}
	}

	std::sort(dirs_to_check.begin(), dirs_to_check.end(), [](const fs::path& a, const fs::path& b) {
		return a.string().length() > b.string().length();
	});

	for (const auto& d : dirs_to_check) {
		std::string s = d.generic_string();
		if (s == "fs/c" || s == "fs/d" || s == "fs/e" || s == "fs/e/mre")
			continue;
		if (fs::is_empty(d, ec)) {
			fs::remove(d, ec);
		}
	}

	fs::create_directories(root_fs / "c", ec);
	fs::create_directories(root_fs / "d", ec);
	fs::create_directories(root_fs / "e" / "mre", ec);
}

sf::Clock global_clock;

bool work = true;

static std::mutex g_error_mutex;
std::string error_message = "";
std::atomic<bool> show_error = false;
std::atomic<bool> g_request_hard_reset = false;

void trigger_hard_reset_with_error(const std::string& err_msg) {
	{
		std::lock_guard<std::mutex> lock(g_error_mutex);
		error_message = err_msg;
	}
	show_error = true;
	g_request_hard_reset = true;
	Cpu::stop();
}

#ifdef ANDROID
#include <spdlog/sinks/android_sink.h>

std::atomic<int> storage_permission_state{0};

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_notifyPermissionState(JNIEnv *env, jobject thiz, jboolean granted) {
    storage_permission_state = granted ? 1 : -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_nativeLoadVxpFile(JNIEnv *env, jobject thiz, jstring j_path) {
    const char *path_cstr = env->GetStringUTFChars(j_path, nullptr);

    while(!g_appManager)
        sf::sleep(sf::seconds(0.2));

    g_appManager->add_app_for_launch(path_cstr, false);

    env->ReleaseStringUTFChars(j_path, path_cstr);
}
#endif


void mre_main(AppManager* appManager_p) {
	AppManager& appManager = *appManager_p;

	sf::Clock deltaClock;
	while (work) {
		uint32_t delta_ms = deltaClock.restart().asMilliseconds();

		GDB::update();
		appManager.update(delta_ms);

		sf::sleep(sf::milliseconds(1000 / 120));
	}
}

int main(int argc, char** argv) {
    std::string app_path = "";
    bool path_is_local = false;

	Log::set_module("Main");
	spdlog::set_level(spdlog::level::debug);

#ifndef ANDROID
	cli::Parser parser(argc, argv);
	{
		parser.set_optional<std::string>("", "", "", "Path to vxp");
		parser.set_optional<bool>("l", "path_is_local", false, "Set to run from local filesystem");
		parser.set_optional<bool>("g", "gdb", false, "Set to run gdb server");
		parser.set_optional<int>("p", "gdb_port", 1234, "Port for gdb server");
	}
	parser.run_and_exit_if_error();
	app_path = parser.get<std::string>("");
	if (app_path.size() >= 2 && app_path.front() == '"' && app_path.back() == '"')
		app_path = app_path.substr(1, app_path.size() - 2);
	path_is_local = parser.get<bool>("l");

	if (app_path.size())
		spdlog::info("Vxp path from args: {} {}", app_path, path_is_local ? "(local path)" : "");

	GDB::gdb_mode = parser.get<bool>("g");
	GDB::gdb_port = parser.get<int>("p");

	fs::current_path(fs::path(argv[0]).parent_path());
#else
    auto android_logger = spdlog::android_logger_mt("android_logger", "MREmu");

    spdlog::set_default_logger(android_logger);
#endif

    AppManager appManager;
    g_appManager = &appManager;

	if(GDB::gdb_mode)
		GDB::wait();

	Memory::init(32 * 1024 * 1024);
	Cpu::init();
	Bridge::init();

	MREngine::SIM::init();
	MREngine::System::init();
	MREngine::CharSet::init();
	MREngine::AppAudio::init();
	MREngine::SystemTextBox::init();
	MREngine::Graphic graphic;

#ifndef ANDROID
	sf::RenderWindow win_debug(sf::VideoMode(1000, 600), "MREmu Debug");
	sf::RenderWindow win_device(sf::VideoMode(graphic.width, graphic.height + 208), "MREmu Device");
	ImGui::SFML::Init(win_debug);
	win_debug.setFramerateLimit(60);
	win_device.setFramerateLimit(60);
	//win_debug.setVerticalSyncEnabled(true);
#else
	sf::RenderWindow win_device(sf::VideoMode::getDesktopMode(), "MREmu");
	win_device.setFramerateLimit(60);

	while (win_device.isOpen()) {
		sf::Event event;
		while (win_device.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				win_device.close();
			}
		}

		int perm_state = storage_permission_state.load();

		if (perm_state == 1)
			break;
		else if (perm_state == 0)
			win_device.clear(sf::Color::Black);
		else if (perm_state == -1)
			win_device.clear(sf::Color::Red);

		win_device.display();
	}
#endif

#ifndef ANDROID
	DragAndDrop::init(win_device, &win_debug);
#else
	DragAndDrop::init(win_device, nullptr);
#endif

	MREngine::IO::init();

	if (GDB::gdb_mode)
		GDB::cpu_state = GDB::Stop;

	std::thread second_thread(mre_main, &appManager);

	Keyboard keyboard;
	Touch touch;

	if (app_path.size()) {
		if (fs::exists(app_path) || path_is_local) {
			appManager.add_app_for_launch(app_path, path_is_local);
		} else {
			error_message = "VXP file does not exist:\n" + app_path;
			show_error = true;
		}
	}
	else
		appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);


	int base_w = graphic.width;
	int base_h = graphic.height + 208;
	DragAndDrop::set_base_size(base_w, base_h);

	float scale = 1.f;
	sf::Sprite screen_sp(graphic.screen_tex);
	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;

	auto repaint_device = [&]() {
		win_device.clear(sf::Color::Black);
		if (MREngine::SystemTextBox::is_open()) {
			MREngine::SystemTextBox::draw((VMUINT8*)graphic.screen.data(), graphic.width, graphic.height);
			graphic.screen_changed = true;
			graphic.update_screen();
		}
		screen_sp.setTexture(graphic.screen_tex, true);
		win_device.draw(screen_sp);

		if (show_warning) {
			float elapsed = warning_clock.getElapsedTime().asSeconds();
			if (elapsed < 4.f) {
				if (int(elapsed * 4.f) % 2 == 0) {
					sf::Texture warn_text_texture = u16text_to_texture(u"Invalid VXP file!", sf::Color(255, 64, 64));
					sf::Sprite warn_text_sprite(warn_text_texture);
					float tw = (float)warn_text_texture.getSize().x;
					float th = (float)warn_text_texture.getSize().y;
					float box_size_h = warn_text_texture.getSize().y;
					float box_size_w = warn_text_texture.getSize().x;
					float padding = 4.0f;

					float px = std::floor((win_device.getSize().x - box_size_w - 2 * padding) / 2.f);
					float py = std::floor((graphic.height * scale - box_size_h - 2 * padding) / 2.f);

					sf::RectangleShape bg(sf::Vector2f(box_size_w + 2 * padding, box_size_h + 2 * padding));
					bg.setFillColor(sf::Color(0, 0, 0, 0));
					bg.setOutlineColor(sf::Color(255, 64, 64, 230));
					bg.setOutlineThickness(1.f);
					bg.setPosition(px, py);

					warn_text_sprite.setOrigin(std::floor(tw / 2.f), std::floor(th / 2.f));
					warn_text_sprite.setPosition(px + std::floor(box_size_w / 2.f) + padding, py + std::floor(box_size_h / 2.f) + padding);

					win_device.draw(bg);
					win_device.draw(warn_text_sprite);
				}
			}
			else {
				show_warning = false;
			}
		}

		keyboard.draw(&win_device);
		win_device.display();
	};

	auto update_screen_size = [&](unsigned int new_w, unsigned int new_h) {
		static bool resizing = false;
		if (resizing)
			return;
		resizing = true;

		constexpr float scale_step = 0.05f;
		float scale_x = (float)new_w / (float)base_w;
		float scale_y = (float)new_h / (float)base_h;

		scale = std::round(std::max(scale_x, scale_y) / scale_step) * scale_step;
		if (scale < 1.0f)
			scale = 1.0f;

		unsigned int target_w = (unsigned int)std::round((float)base_w * scale);
		unsigned int target_h = (unsigned int)std::round((float)base_h * scale);

		if (win_device.getSize().x != target_w || win_device.getSize().y != target_h)
			win_device.setSize(sf::Vector2u(target_w, target_h));

		win_device.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)target_w, (float)target_h)));

		screen_sp.setScale(scale, scale);
		screen_sp.setPosition(0.f, 0.f);

		keyboard.update_resize(target_w, target_h);

		repaint_device();

		resizing = false;
	};

	DragAndDrop::set_callbacks(update_screen_size, repaint_device);

	DragAndDrop::restore_window_state();

#ifndef ANDROID
	win_debug.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)win_debug.getSize().x, (float)win_debug.getSize().y)));
#endif

	update_screen_size(win_device.getSize().x, win_device.getSize().y);

	sf::Clock fps;

	sf::Clock deltaClock;
	sf::Event event;

	bool request_clear_user_data = false;

	while (win_device.isOpen()
#ifndef ANDROID
        && win_debug.isOpen()
#endif
    ) {
		if (g_request_hard_reset) {
			g_request_hard_reset = false;

			work = false;
			Cpu::stop();
			if (second_thread.joinable())
				second_thread.join();

			appManager.reset();

			if (request_clear_user_data) {
				request_clear_user_data = false;
				clear_user_data("fs");
			}

			Cpu::deinit();
			Memory::deinit();
			Memory::init(32 * 1024 * 1024);
			Cpu::init();
			Bridge::init();

			MREngine::SIM::init();
			MREngine::System::init();
			MREngine::CharSet::init();
			MREngine::AppAudio::init();
			MREngine::IO::init();
			MREngine::SystemTextBox::init();

			graphic.reset();

			keyboard.kc.pkey.clear();
			touch.touching = false;

			if (GDB::gdb_mode)
				GDB::cpu_state = GDB::Stop;

			appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);

			work = true;
			second_thread = std::thread(mre_main, &appManager);
		}

#ifndef ANDROID
		while (win_debug.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);
			switch (event.type) {
			case sf::Event::Closed:
				DragAndDrop::save_window_state();
				win_debug.close();
				win_device.close();
				break;
			case sf::Event::Resized:
				win_debug.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
				break;
			}
		}
#endif

		while (win_device.pollEvent(event)) {
			if (MREngine::SystemTextBox::is_open()) {
				if (MREngine::SystemTextBox::handle_sfml_event(event, graphic.width, graphic.height, scale)) {
					continue;
				}
			}
			keyboard.event(event);
			touch.sf_event(event);
			switch (event.type) {
			case sf::Event::Closed:
				DragAndDrop::save_window_state();
				win_device.close();
#ifndef ANDROID
				win_debug.close();
#endif
				break;
			case sf::Event::Resized:
				update_screen_size(event.size.width, event.size.height);
				break;
			}
		}
#ifndef ANDROID
		ImGui::SFML::Update(win_debug, deltaClock.restart());

		if (show_error) {
			ImGui::OpenPopup("VXP Error");
			show_error = false; // Only call OpenPopup once
		}
		if (ImGui::BeginPopupModal("VXP Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			std::string current_err;
			{
				std::lock_guard<std::mutex> lock(g_error_mutex);
				current_err = error_message;
			}
			ImGui::TextUnformatted("The emulator encountered an error and was reset:");
			ImGui::Spacing();

			// Read-only multi-line text input with monospace-like selectable area
			ImVec2 text_size(520, std::min(300.f, std::max(120.f, ImGui::GetTextLineHeightWithSpacing() * 15)));
			ImGui::InputTextMultiline("##error_details", const_cast<char*>(current_err.c_str()),
				current_err.size() + 1, text_size, ImGuiInputTextFlags_ReadOnly);

			ImGui::Spacing();
			if (ImGui::Button("Copy to Clipboard", ImVec2(150, 0))) {
				ImGui::SetClipboardText(current_err.c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button("OK", ImVec2(100, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
#endif

		graphic.update_screen();

#ifndef ANDROID
		graphic.imgui_screen();
		App* active_app = appManager.get_active_app();
		if (active_app) {
			active_app->graphic.imgui_layers();
			active_app->graphic.imgui_canvases();
		}

		ImGui::SetNextWindowPos(ImVec2(10, 255), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(175, 335), ImGuiCond_FirstUseEver);
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

		ImGui::SetNextWindowPos(ImVec2(10, 190), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(175, 60), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Fps")) {
			ImGui::Text("%1.3f", 1.f / fps.restart().asSeconds());
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(175, 205), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Control")) {
			if (ImGui::Button("Hard Reset"))
				g_request_hard_reset = true;
			if (ImGui::Button("Clear user data")) {
				ImGui::OpenPopup("Clear User Data");
			}
			if (ImGui::Button("Open c:/ (fs/c)"))
				open_folder("fs/c");
			if (ImGui::Button("Open d:/ (fs/d)"))
				open_folder("fs/d");
			if (ImGui::Button("Open e:/ (fs/e)"))
				open_folder("fs/e");
			if (ImGui::Button("Open e:/mre (fs/e/mre)"))
				open_folder("fs/e/mre");

			if (ImGui::BeginPopupModal("Clear User Data", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("Are you sure you want to clear all user data?\nThis will erase all saves, settings, and caches,\nwhile keeping all VXP files intact.");
				ImGui::Separator();
				if (ImGui::Button("Yes, Clear Data", ImVec2(130, 0))) {
					request_clear_user_data = true;
					g_request_hard_reset = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(100, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();

		if (show_warning && warning_clock.getElapsedTime().asSeconds() < 4.f) {
			ImGui::SetNextWindowPos(ImVec2(win_debug.getSize().x / 2.f, win_debug.getSize().y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			if (ImGui::Begin("Warning##VXP", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
				ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Imported file is not a valid VXP!");
			}
			ImGui::End();
		}
#endif

		repaint_device();

#ifndef ANDROID
		Cpu::imgui_REG();

		keyboard.imgui_keyboard();
#endif

#ifndef ANDROID
		ImGui::SFML::Render(win_debug);
		win_debug.display();
		win_debug.clear();
		DragAndDrop::ensure_device_on_top();
#endif
	}

	DragAndDrop::cleanup();

	work = false;
	second_thread.join();

#ifndef ANDROID
	ImGui::SFML::Shutdown();
#endif
	return 0;
}