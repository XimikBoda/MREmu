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
		void reset();

		~Graphic();
	};

	struct layer {
		void* buf = 0;
		int x = 0, y = 0, w = 0, h = 0;
		int trans_color = -1;
		clip_rect clip = { 0, 0, 0, 0, 0 };
		sf::Texture tex;
	};

	class RenderBox {
	public:
		int st_x = 0, st_y = 0;
		int end_x = 0, end_y = 0;

		RenderBox(int st_x, int st_y, int end_x, int end_y);
		RenderBox(const class canvas_frame_property& cfp);

		void clip(const class canvas_frame_property& cfp);
		void clip(const layer& layer);
		void clip(const clip_rect& clip);
		void clip(int x1, int y1, int x2, int y2);

		void include(int x, int y);

		bool in(int x, int y);
		bool x_in(int x);
		bool y_in(int y);
	};

	class AppGraphic {
	public:
		std::vector<layer> layers;
		int active_layer = 0;

		std::vector<std::pair<void*, sf::Texture>> canvases_list;
		mutex_wrapper canvases_list_mutex;

		vm_graphic_color global_color;

		clip_rect clip = { 0, 0, 0, 0, 0 };

		//old
		int old_layer = -1;
		bool old_layer_inited = false;

		layer* find_layer_by_buf(void* buf);
		clip_rect clip_by_buf(void* buf);

		int create_layer(int x, int y, int w, int h, int trans_color);
		int create_layer_ex(int x, int y, int w, int h, int trans_color, int mode, void*buf);
		void* get_layer_buf(int handle);

		int delete_layer(int handle);

		void imgui_layers();
		void imgui_canvases();
	};
}

MREngine::AppGraphic& get_current_app_graphic();