#pragma once
#include <vector>
#include <mutex>
#include <vmgraph.h>
#include <SFML/Graphics/Texture.hpp>
#include "../mutex_wrapper.h"

namespace MREngine {
	class Graphic {
	public:
		int width = 240, height = 320;
		bool screen_changed = true;

		sf::Texture screen_tex;

		std::vector<uint16_t> screen;
		void* base_buf1 = 0; // wrong, need to change to canvas
		void* base_buf2 = 0;

		Graphic();

		void activate();
		void update_screen();

		void imgui_screen();

		~Graphic();
	};

	struct layer {
		void* buf = 0;
		int x = 0, y = 0, w = 0, h = 0;
		int trans_color = -1;
		sf::Texture tex;
	};


	class AppGraphic {
	public:
		std::vector<layer> layers;
		int active_layer = 0;

		std::vector<std::pair<void*, sf::Texture>> canvases_list;
		mutex_wrapper canvases_list_mutex;

		vm_graphic_color global_color;

		clip_rect clip = { 0, 0, 0, 0, 0 };

		int create_layer(int x, int y, int w, int h, int trans_color);
		int create_layer_ex(int x, int y, int w, int h, int trans_color, int mode, void*buf);
		void* get_layer_buf(int handle);

		int delete_layer(int handle);

		void imgui_layers();
		void imgui_canvases();
	};
}

MREngine::AppGraphic& get_current_app_graphic();