#pragma once
#include <vector>
#include <SFML/Graphics/Texture.hpp>


namespace MREngine {
	class Graphic {
	public:
		int width = 240, height = 320;

		sf::Texture screen_tex;

		std::vector<uint16_t> screen;
		void* base_buf1 = 0; // wrong, need to change to canvas
		void* base_buf2 = 0;

		Graphic();

		void activate();

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

		int create_layer(int x, int y, int w, int h, int trans_color);
		void* get_layer_buf(int handle);

		void imgui_layers();
	};
}

MREngine::AppGraphic& get_current_app_graphic();