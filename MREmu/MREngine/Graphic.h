#pragma once
#include <vector>
#include <mutex>
#include <vmgraph.h>
#include <SFML/Graphics/Texture.hpp>
#include "../mutex_wrapper.h"

const char* const CANVAS_MAGIC = "MTKCANVAS"; // Do we have an app that checks for this?

typedef void* VMINT_CANVAS;

VMINT_CANVAS vm_graphic_create_canvas_FIX(VMINT width, VMINT height);
VMINT_CANVAS vm_graphic_create_canvas_cf_FIX(vm_graphic_color_famat cf, VMINT width, VMINT height);
void vm_graphic_release_canvas_FIX(VMINT_CANVAS hcanvas);
VMUINT8* vm_graphic_get_canvas_buffer_FIX(VMINT_CANVAS hcanvas);

VMINT_CANVAS vm_graphic_load_image_FIX(VMUINT8* img, VMINT img_len);
struct frame_prop* vm_graphic_get_img_property_FIX(VMINT_CANVAS hcanvas, VMUINT8 frame_index);

VM_GDI_RESULT vm_graphic_canvas_set_trans_color_FIX(VMINT_CANVAS hcanvas, VMINT trans_color);

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

#pragma pack (push, 1)
	struct canvas_signature {
		char magic[9];
		uint8_t frame_count = 1;
		uint8_t i_dont_know = 0xFF;
		uint8_t color_format = 1;
	};

	struct canvas_frame_property {
		uint8_t flag = 0; //?
		uint16_t left = 0;
		uint16_t top = 0;
		uint16_t width = 0;
		uint16_t height = 0;
		uint16_t delay = 0;
		uint8_t trans_color_index = 0;
		uint16_t trans_color = 0;
		uint16_t reserved = 0; 
		uint32_t offset = 0;
	};
#pragma pack(pop)

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