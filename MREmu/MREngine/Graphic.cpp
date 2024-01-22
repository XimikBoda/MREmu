#include "Graphic.h"
#include "../Memory.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <SFML/Graphics/Image.hpp>
#include <vmgraph.h>

static MREngine::Graphic* graphic = 0; // Do I really need this?

void buf_to_texture(void* buf, int w, int h, sf::Texture& tex) {
	static std::vector<unsigned char> pix_data;

	if (pix_data.size() < w * h * 4)
		pix_data.resize(w * h * 4);

	unsigned short* buf16 = (unsigned short*)buf;
	for (int i = 0; i < w * h; ++i) {
		pix_data[i * 4 + 0] = VM_COLOR_GET_RED(buf16[i]);
		pix_data[i * 4 + 1] = VM_COLOR_GET_GREEN(buf16[i]);
		pix_data[i * 4 + 2] = VM_COLOR_GET_BLUE(buf16[i]);
		pix_data[i * 4 + 3] = 0xFF;
	}
	sf::Image im;
	im.create(w, h, &pix_data[0]);

	//sf::Texture tex;
	tex.loadFromImage(im);

	//return tex;
}

MREngine::Graphic::Graphic()
{
	activate();
	screen.resize(width*height);
	base_buf1 = Memory::shared_malloc(width * height * 2);
	base_buf2 = Memory::shared_malloc(width * height * 2);
}

void MREngine::Graphic::activate()
{
	graphic = this;
}

void MREngine::Graphic::imgui_screen() {
	if (ImGui::Begin("Screen")) {
		buf_to_texture(screen.data(), width, height, screen_tex);
		ImGui::Image(screen_tex);
		ImGui::End();
	}
}

MREngine::Graphic::~Graphic()
{
	if (graphic == this)
		graphic = 0;
}


int MREngine::AppGraphic::create_layer(int x, int y, int w, int h, int trans_color) {
	if (!graphic)
		return -1;

	if (layers.size() == 0) {
		//if (x != 0 || y != 0 || w != graphic->width || h != graphic->height)
		//	return -1;

		//layers.push_back({ graphic->base_buf1, x, y, w, h, trans_color });
		layers.push_back({ graphic->base_buf1, 0, 0, graphic->width, graphic->height, trans_color });
		return 0;
	}
}

void* MREngine::AppGraphic::get_layer_buf(int handle) {
	if (handle < 0 || handle >= layers.size())
		return 0;
	return layers[handle].buf;
}

void MREngine::AppGraphic::imgui_layers() {
	if (ImGui::Begin("Layers")) {
		for (int i = 0; i < layers.size(); ++i) {
			auto& el = layers[i];
			buf_to_texture(el.buf, el.w, el.h, el.tex);
			ImGui::Text("Id: %d, x: %d, y: %d, w: %d, h: %d, t: %d", 
				i, el.x, el.y, el.w, el.h, el.trans_color);
			ImGui::Image(el.tex);
		}
		ImGui::End();
	}
}


//MRE API

VMINT vm_graphic_get_screen_width(void)
{
	if (graphic)
		return graphic->width;
	else
		return 0;
}

VMINT vm_graphic_get_screen_height(void)
{
	if (graphic)
		return graphic->height;
	else
		return 0;
}


VMINT vm_graphic_create_layer(VMINT x, VMINT y, VMINT width, VMINT height, VMINT trans_color) {
	return get_current_app_graphic().create_layer(x, y, width, height, trans_color);
}

VMUINT8* vm_graphic_get_layer_buffer(VMINT handle) {
	return (VMUINT8*)get_current_app_graphic().get_layer_buf(handle);
}

VMINT vm_graphic_flush_layer(VMINT* layer_handles, VMINT count) {//TODO
	memcpy(graphic->screen.data(), graphic->base_buf1, graphic->screen.size()*2);//temp
	return 0;
}