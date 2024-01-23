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

void canvas_to_texture(std::pair<void*, sf::Texture>& p) {
	static std::vector<unsigned char> pix_data;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)p.first;

	if (memcmp(cs->magic, CANVAS_MAGIC, 9))
		return;

	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

	int w = cfp->width, h = cfp->height;

	if (pix_data.size() < w * h * 4)
		pix_data.resize(w * h * 4);

	unsigned short* buf16 = (unsigned short*)(cfp + 1);
	for (int i = 0; i < w * h; ++i) {
		pix_data[i * 4 + 0] = VM_COLOR_GET_RED(buf16[i]);
		pix_data[i * 4 + 1] = VM_COLOR_GET_GREEN(buf16[i]);
		pix_data[i * 4 + 2] = VM_COLOR_GET_BLUE(buf16[i]);
		pix_data[i * 4 + 3] = 0xFF;
	}
	sf::Image im;
	im.create(w, h, &pix_data[0]);

	p.second.loadFromImage(im);
}

MREngine::Graphic::Graphic()
{
	activate();
	screen.resize(width * height);
	{
		int image_size = width * height * 2;
		void* canvas_buf = Memory::shared_malloc(VM_CANVAS_DATA_OFFSET + image_size);

		if (canvas_buf == 0) abort();

		MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

		*cs = MREngine::canvas_signature();
		memcpy(cs->magic, CANVAS_MAGIC, 9);

		MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

		*cfp = MREngine::canvas_frame_property();
		cfp->width = width;
		cfp->height = height;
		base_buf1 = (cfp + 1);
	}
	//base_buf2 = Memory::shared_malloc(width * height * 2 + VM_CANVAS_DATA_OFFSET);
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

void MREngine::AppGraphic::imgui_canvases() {
	if (ImGui::Begin("Canvases")) {
		for (int i = 0; i < canvases_list.size(); ++i) {
			auto& el = canvases_list[i];
			canvas_to_texture(el);

			MREngine::canvas_signature* cs = (MREngine::canvas_signature*)el.first;

			if (memcmp(cs->magic, CANVAS_MAGIC, 9)) {
				ImGui::Text("Wrong canvas magic");
				break;
			}

			MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

			int w = cfp->width, h = cfp->height;

			ImGui::Text("Id: %d, x: %d, y: %d, w: %d, h: %d",
				i, cfp->left, cfp->top, cfp->width, cfp->height);
			ImGui::Image(el.second);
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
	memcpy(graphic->screen.data(), graphic->base_buf1, graphic->screen.size() * 2);//temp
	return 0;
}

VMINT vm_graphic_create_canvas(VMINT width, VMINT height) {
	int image_size = width * height * 2;
	void* canvas_buf = vm_malloc(VM_CANVAS_DATA_OFFSET + image_size);

	if (canvas_buf == 0)
		return 0;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

	*cs = MREngine::canvas_signature();
	memcpy(cs->magic, CANVAS_MAGIC, 9);

	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

	*cfp = MREngine::canvas_frame_property();
	cfp->width = width;
	cfp->height = height;

	cfp->offset = image_size;

	get_current_app_graphic().canvases_list.push_back({ canvas_buf, sf::Texture() });

	return (VMINT)ADDRESS_TO_EMU(canvas_buf);
}

void vm_graphic_release_canvas(VMINT hcanvas) {
	void* hcanvas_adr = ADDRESS_FROM_EMU(hcanvas);
	auto& canvases_list = get_current_app_graphic().canvases_list;

	for (int i = 0; i < canvases_list.size(); ++i)
		if (canvases_list[i].first == hcanvas_adr)
			canvases_list.erase(canvases_list.begin() + i);

	vm_free(hcanvas_adr);
}

VMUINT8* vm_graphic_get_canvas_buffer(VMINT hcanvas) {
	return (VMUINT8*)ADDRESS_FROM_EMU(hcanvas);
}

VMINT vm_graphic_load_image(VMUINT8* img, VMINT img_len) {
	sf::Image im;
	if (!im.loadFromMemory(img, img_len))
		return 0;

	int image_size = im.getSize().x * im.getSize().y * 2;
	void* canvas_buf = vm_malloc(VM_CANVAS_DATA_OFFSET + image_size);

	if (canvas_buf == 0)
		return 0;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

	*cs = MREngine::canvas_signature();
	memcpy(cs->magic, CANVAS_MAGIC, 9);

	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

	int t = sizeof(MREngine::canvas_frame_property);

	*cfp = MREngine::canvas_frame_property();
	cfp->width = im.getSize().x;
	cfp->height = im.getSize().y;

	cfp->offset = image_size;

	uint16_t* image_buf = (uint16_t*)(cfp + 1);
	sf::Color* rgb_buf = (sf::Color*)im.getPixelsPtr();

	for (int i = 0; i < image_size / 2; ++i) {
		sf::Color c = rgb_buf[i];
		image_buf[i] = VM_COLOR_888_TO_565(c.r, c.g, c.b);
	}

	get_current_app_graphic().canvases_list.push_back({ canvas_buf, sf::Texture() });

	return (VMINT)ADDRESS_TO_EMU(canvas_buf);
}

void vm_graphic_blt(VMBYTE* dst_disp_buf, VMINT x_dest, VMINT y_dest, VMBYTE* src_disp_buf,
	VMINT x_src, VMINT y_src, VMINT width, VMINT height, VMINT frame_index) {
	if (dst_disp_buf == 0 || src_disp_buf == 0)
		return;

	MREngine::canvas_signature* cs_dst = (MREngine::canvas_signature*)(dst_disp_buf - VM_CANVAS_DATA_OFFSET);
	MREngine::canvas_signature* cs_src = (MREngine::canvas_signature*)(src_disp_buf);
	if (memcmp(cs_dst->magic, CANVAS_MAGIC, 9) || memcmp(cs_src->magic, CANVAS_MAGIC, 9))
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	MREngine::canvas_frame_property* cfp_src = (MREngine::canvas_frame_property*)(cs_src + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);
	unsigned short* buf16_src = (unsigned short*)(cfp_src + 1);

	if (x_src + width > cfp_src->width)
		width = cfp_src->width - x_src;
	if (y_src + height > cfp_src->height)
		height = cfp_src->height - y_src;

	int st_x = std::max(0, x_dest);
	int st_y = std::max(0, y_dest);

	int end_x = std::min<int>(cfp_dst->width, x_dest + width);
	int end_y = std::min<int>(cfp_dst->height, y_dest + height);

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int im_x = sx - x_dest + x_src;
			int im_y = sy - y_dest + y_src;
			buf16_dst[sy * cfp_dst->width + sx] = buf16_src[im_y * cfp_src->width + im_x];
		}
}