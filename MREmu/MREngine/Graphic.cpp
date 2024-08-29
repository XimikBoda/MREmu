#include "Graphic.h"
#include "../Memory.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <SFML/Graphics/Image.hpp>
#include <vmgraph.h>
#include <vmpromng.h>

MREngine::Graphic* graphic = 0; // Do I really need this?

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

	unsigned short trans_color = cfp->trans_color;
	bool flag = cfp->flag;

	unsigned short* buf16 = (unsigned short*)(cfp + 1);
	for (int i = 0; i < w * h; ++i) {
		if (flag && trans_color == buf16[i])
			pix_data[i * 4 + 3] = 0x00;
		else
			pix_data[i * 4 + 3] = 0xFF;
		pix_data[i * 4 + 0] = VM_COLOR_GET_RED(buf16[i]);
		pix_data[i * 4 + 1] = VM_COLOR_GET_GREEN(buf16[i]);
		pix_data[i * 4 + 2] = VM_COLOR_GET_BLUE(buf16[i]);
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
		base_buf2 = (cfp + 1);
	}
	graphic = this;
}

void MREngine::Graphic::activate()
{
	graphic = this;
}

void MREngine::Graphic::update_screen() {
	buf_to_texture(screen.data(), width, height, screen_tex);
}

void MREngine::Graphic::imgui_screen() {
	if (ImGui::Begin("Screen")) {
		ImGui::Image(screen_tex);
	}
	ImGui::End();
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
		if (x != 0 || y != 0 || w != graphic->width || h != graphic->height)
			return -1;

		layers.push_back({ graphic->base_buf1, x, y, w, h, trans_color });
		active_layer = 0;
		return 0;
	}
	else if (layers.size() == 1) {
		if (w > graphic->width || h > graphic->height)
			return -1;

		MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)graphic->base_buf2 - 1;

		cfp->width = w;
		cfp->height = h;

		layers.push_back({ graphic->base_buf2, x, y, w, h, trans_color });
		return 1;
	}
	else
		abort();
}

int MREngine::AppGraphic::create_layer_ex(int x, int y, int w, int h, int trans_color, int mode, void* buf) {
	if (!graphic)
		return -1;

	if (mode != VM_BUF)//todo
		abort();

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)((unsigned char*)buf - VM_CANVAS_DATA_OFFSET);
	if (memcmp(cs->magic, CANVAS_MAGIC, 9)) {
		cs = (MREngine::canvas_signature*)((unsigned char*)buf);
		if (memcmp(cs->magic, CANVAS_MAGIC, 9))
			return -1;
	}
	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);
	unsigned short* buf16 = (unsigned short*)(cfp + 1);

	layers.push_back({ buf16, x, y, w, h, trans_color });

	return layers.size() - 1;
}

int MREngine::AppGraphic::delete_layer(int handle)
{
	if (handle < 0 || handle != layers.size() - 1) // TODO check this;
		return -1;

	layers.erase(layers.begin() + handle);

	return 0;
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
	}
	ImGui::End();
}

void MREngine::AppGraphic::imgui_canvases() {
	std::lock_guard lock(canvases_list_mutex);
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
	}
	ImGui::End();
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

VM_GDI_HANDLE vm_graphic_create_layer_ex(VMINT x, VMINT y, VMINT width, VMINT height, VMINT trans_color, VMINT mode, VMUINT8* buf) {
	return get_current_app_graphic().create_layer_ex(x, y, width, height, trans_color, mode, buf);
}

VM_GDI_HANDLE vm_graphic_create_layer_cf(vm_graphic_color_famat cf, VMINT x, VMINT y, VMINT width, VMINT height, vm_graphic_color_argb* trans_color, VMINT mode, VMUINT8* buf, VMINT buf_size) {
	if (cf != VM_GRAPHIC_COLOR_FORMAT_16)
		return -1;

	return vm_graphic_create_layer_ex(x, y, width, height, -1, mode, buf);
}

VMINT vm_graphic_delete_layer(VMINT handle) {
	return get_current_app_graphic().delete_layer(handle);
}

VMINT vm_graphic_active_layer(VMINT handle) {
	auto& gr = get_current_app_graphic();
	if (handle < 0 || handle >= gr.layers.size())
		return VM_GDI_FAILED;
	gr.active_layer = handle;
	return VM_GDI_SUCCEED;
}

VMUINT8* vm_graphic_get_layer_buffer(VMINT handle) {
	return (VMUINT8*)get_current_app_graphic().get_layer_buf(handle);
}

VMINT vm_graphic_flush_layer(VMINT* layer_handles, VMINT count) {//TODO
	if (layer_handles == 0)
		return -1;

	MREngine::AppGraphic& gr = get_current_app_graphic();

	for (int i = 0; i < count; ++i)
		if (layer_handles[i] < 0 || layer_handles[i] >= gr.layers.size())
			return -1;

	for (int sy = 0; sy < graphic->height; ++sy)
		for (int sx = 0; sx < graphic->width; ++sx)
			for (int lid = count - 1; lid >= 0; --lid) {
				auto& layer = gr.layers[layer_handles[lid]];
				int lx = sx - layer.x;
				int ly = sy - layer.y;
				uint16_t* buf = (uint16_t*)layer.buf;

				if (lx < 0 || lx >= layer.w || ly < 0 || ly >= layer.h)
					continue;

				uint16_t color = buf[ly * layer.w + lx];
				if (int(color) == layer.trans_color)
					continue;

				graphic->screen[sy * graphic->width + sx] = color;
				break;
			}

	return VM_GDI_SUCCEED;
}

VM_GDI_RESULT vm_graphic_flatten_layer(VMINT* hhandle, VMINT count) {
	if (hhandle == 0)
		return -1;

	MREngine::AppGraphic& gr = get_current_app_graphic();

	for (int i = 0; i < count; ++i)
		if (hhandle[i] < 0 || hhandle[i] >= gr.layers.size())
			return -1;

	for (int sy = 0; sy < graphic->height; ++sy)
		for (int sx = 0; sx < graphic->width; ++sx) {
			auto& act_layer = gr.layers[gr.active_layer];
			int act_lx = sx - act_layer.x;
			int act_ly = sy - act_layer.y;
			uint16_t* act_buf = (uint16_t*)act_layer.buf;

			if (act_lx < 0 || act_lx >= act_layer.w || act_ly < 0 || act_ly >= act_layer.h)
				continue;

			for (int lid = count - 1; lid >= 0; --lid) {
				if (hhandle[lid] == gr.active_layer)
					continue;
				auto& layer = gr.layers[hhandle[lid]];
				int lx = sx - layer.x;
				int ly = sy - layer.y;
				uint16_t* buf = (uint16_t*)layer.buf;

				if (lx < 0 || lx >= layer.w || ly < 0 || ly >= layer.h)
					continue;

				uint16_t color = buf[ly * layer.w + lx];
				if (int(color) == layer.trans_color)
					continue;

				act_buf[act_ly * act_layer.w + act_lx] = color;
				break;
			}
		}

	for (int i = 0; i < count; ++i)
		if (hhandle[count - 1 - i] != gr.active_layer)
			gr.delete_layer(hhandle[count - 1 - i]);

	return VM_GDI_SUCCEED;
}


VM_GDI_RESULT vm_graphic_translate_layer(VMINT handle, VMINT tx, VMINT ty) {
	MREngine::AppGraphic& gr = get_current_app_graphic();

	if (handle < 0 || handle >= gr.layers.size())
		return -1;

	gr.layers[handle].x = tx;
	gr.layers[handle].y = ty;
	return VM_GDI_SUCCEED;
}

VMINT vm_graphic_get_bits_per_pixel(void) {
	return 2;
}

MREngine::canvas_signature* find_canvas_signature(VMUINT8* buf) {
	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)buf;
	if (memcmp(cs->magic, CANVAS_MAGIC, 9) == 0)
		return cs;
	cs = (MREngine::canvas_signature*)(buf - VM_CANVAS_DATA_OFFSET);
	if (memcmp(cs->magic, CANVAS_MAGIC, 9) == 0)
		return cs;
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

	std::lock_guard lock(get_current_app_graphic().canvases_list_mutex);
	get_current_app_graphic().canvases_list.push_back({ canvas_buf, sf::Texture() });

	return (VMINT)ADDRESS_TO_EMU(canvas_buf);
}

VMINT vm_graphic_create_canvas_cf(vm_graphic_color_famat cf, VMINT width, VMINT height) {
	if (cf != VM_GRAPHIC_COLOR_FORMAT_16)
		return -1;

	return vm_graphic_create_canvas(width, height);
}

void vm_graphic_release_canvas(VMINT hcanvas) {
	void* hcanvas_adr = ADDRESS_FROM_EMU(hcanvas);
	auto& canvases_list = get_current_app_graphic().canvases_list;

	std::lock_guard lock(get_current_app_graphic().canvases_list_mutex);

	for (int i = 0; i < canvases_list.size(); ++i)
		if (canvases_list[i].first == hcanvas_adr) {
			canvases_list.erase(canvases_list.begin() + i);
			break;
		}

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

	std::lock_guard lock(get_current_app_graphic().canvases_list_mutex);
	get_current_app_graphic().canvases_list.push_back({ canvas_buf, sf::Texture() });

	return (VMINT)ADDRESS_TO_EMU(canvas_buf);
}

struct frame_prop* vm_graphic_get_img_property(VMINT hcanvas, VMUINT8 frame_index) {
	if (hcanvas == 0)
		return 0;

	static struct frame_prop* info = (frame_prop*)Memory::shared_malloc(sizeof(frame_prop));

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)(ADDRESS_FROM_EMU(hcanvas));
	if (memcmp(cs->magic, CANVAS_MAGIC, 9))
		return 0;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs + 1);

	//TODO frame index

	info->flag = cfp_dst->flag;
	info->left = cfp_dst->left;
	info->top = cfp_dst->top;
	info->width = cfp_dst->width;
	info->height = cfp_dst->height;
	info->delay_time = cfp_dst->delay * 10; //todo check this
	info->trans_color_index = cfp_dst->trans_color_index;
	info->trans_color = cfp_dst->trans_color;
	info->offset = cfp_dst->offset;

	return info;
}

void vm_graphic_blt(VMBYTE* dst_disp_buf, VMINT x_dest, VMINT y_dest, VMBYTE* src_disp_buf,
	VMINT x_src, VMINT y_src, VMINT width, VMINT height, VMINT frame_index) {
	if (dst_disp_buf == 0 || src_disp_buf == 0)
		return;

	MREngine::canvas_signature* cs_dst = find_canvas_signature(dst_disp_buf);
	MREngine::canvas_signature* cs_src = find_canvas_signature(src_disp_buf);
	if (!cs_dst || !cs_src)
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

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	bool flag = cfp_src->flag;
	unsigned short trans_color = cfp_src->trans_color;

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int im_x = sx - x_dest + x_src;
			int im_y = sy - y_dest + y_src;
			unsigned short scr_color = buf16_src[im_y * cfp_src->width + im_x];
			if (!flag || scr_color != trans_color)
				buf16_dst[sy * cfp_dst->width + sx] = scr_color;
		}
}

void vm_graphic_blt_ex(VMBYTE* dst_disp_buf, VMINT x_dest, VMINT y_dest, VMBYTE* src_disp_buf,
	VMINT x_src, VMINT y_src, VMINT width, VMINT height, VMINT frame_index, VMINT alpha) {
	if (dst_disp_buf == 0 || src_disp_buf == 0)
		return;

	MREngine::canvas_signature* cs_dst = find_canvas_signature(dst_disp_buf);
	MREngine::canvas_signature* cs_src = find_canvas_signature(src_disp_buf);
	if (!cs_dst || !cs_src)
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

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	bool flag = cfp_src->flag;
	unsigned short trans_color = cfp_src->trans_color;

	//TODO alpha blend

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int im_x = sx - x_dest + x_src;
			int im_y = sy - y_dest + y_src;
			unsigned short scr_color = buf16_src[im_y * cfp_src->width + im_x];
			if (!flag || scr_color != trans_color)
				buf16_dst[sy * cfp_dst->width + sx] = scr_color;
		}
}

void vm_graphic_rotate(VMBYTE* buf, VMINT x_des, VMINT y_des,
	VMBYTE* src_buf, VMINT frame_index, VMINT degrees) { //Need more testing
	if (buf == 0 || src_buf == 0)
		return;

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	MREngine::canvas_signature* cs_src = find_canvas_signature(src_buf);
	if (!cs_dst || !cs_src)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	MREngine::canvas_frame_property* cfp_src = (MREngine::canvas_frame_property*)(cs_src + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);
	unsigned short* buf16_src = (unsigned short*)(cfp_src + 1);

	int width = cfp_src->width;
	int height = cfp_src->height;

	if (degrees == VM_ROTATE_DEGREE_90 || degrees == VM_ROTATE_DEGREE_270)
		std::swap(width, height);

	int st_x = std::max(0, x_des);
	int st_y = std::max(0, y_des);

	int end_x = std::min<int>(cfp_dst->width, x_des + width);
	int end_y = std::min<int>(cfp_dst->height, y_des + height);

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	bool flag = cfp_src->flag;
	unsigned short trans_color = cfp_src->trans_color;

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int im_x = sx - x_des;
			int im_y = sy - y_des;

			if (degrees == VM_ROTATE_DEGREE_90) {
				std::swap(im_x, im_y);
				im_y = width - im_y - 1;
			}
			else if (degrees == VM_ROTATE_DEGREE_270) {
				std::swap(im_x, im_y);
				im_x = height - im_x - 1;
			}
			else if (degrees == VM_ROTATE_DEGREE_180)
				im_x = width - im_x - 1, im_y = height - im_y - 1;


			unsigned short scr_color = buf16_src[im_y * cfp_src->width + im_x];
			if (!flag || scr_color != trans_color)
				buf16_dst[sy * cfp_dst->width + sx] = scr_color;
		}
}


void vm_graphic_mirror(VMBYTE* buf, VMINT x_des, VMINT y_des, VMBYTE* src_buf, VMINT frame_index, VMINT direction) {
	if (buf == 0 || src_buf == 0)
		return;

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	MREngine::canvas_signature* cs_src = find_canvas_signature(src_buf);
	if (!cs_dst || !cs_src)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	MREngine::canvas_frame_property* cfp_src = (MREngine::canvas_frame_property*)(cs_src + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);
	unsigned short* buf16_src = (unsigned short*)(cfp_src + 1);

	int width = cfp_src->width;
	int height = cfp_src->height;

	int st_x = std::max(0, x_des);
	int st_y = std::max(0, y_des);

	int end_x = std::min<int>(cfp_dst->width, x_des + width);
	int end_y = std::min<int>(cfp_dst->height, y_des + height);

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	bool flag = cfp_src->flag;
	unsigned short trans_color = cfp_src->trans_color;

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int im_x = sx - x_des;
			int im_y = sy - y_des;

			if (direction == VM_MIRROR_X)
				im_x = width - im_x - 1;
			else if (direction == VM_MIRROR_Y)
				im_y = height - im_y - 1;

			unsigned short scr_color = buf16_src[im_y * cfp_src->width + im_x];
			if (!flag || scr_color != trans_color)
				buf16_dst[sy * cfp_dst->width + sx] = scr_color;
		}
}

void vm_graphic_set_pixel(VMUINT8* buf, VMINT x, VMINT y, VMUINT16 color) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int left = 0;
	int top = 0;
	int right = cfp_dst->width;
	int bottom = cfp_dst->height;

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (left < clip.left)
			left = clip.left;
		if (top < clip.top)
			top = clip.top;
		if (right > clip.right + 1)
			right = clip.right + 1;
		if (bottom > clip.bottom + 1)
			bottom = clip.bottom + 1;
	}

	if (x < left || x >= right || y < top || y >= bottom)
		return;

	buf16_dst[y * cfp_dst->width + x] = color;
}

void vm_graphic_set_pixel_ex(VMINT handle, VMINT x1, VMINT y1) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = get_current_app_graphic().global_color.vm_color_565;

	vm_graphic_set_pixel((VMUINT8*)layer.buf, x1, y1, c);
}

void vm_graphic_line(VMUINT8* buf, VMINT x0, VMINT y0, VMINT x1, VMINT y1, VMUINT16 color) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	if (x0 == x1 && y0 == y1)
		return vm_graphic_set_pixel(buf, x0, y0, color);

	int left = 0;
	int top = 0;
	int right = cfp_dst->width;
	int bottom = cfp_dst->height;

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (left < clip.left)
			left = clip.left;
		if (top < clip.top)
			top = clip.top;
		if (right > clip.right + 1)
			right = clip.right + 1;
		if (bottom > clip.bottom + 1)
			bottom = clip.bottom + 1;
	}
	if (abs(x1 - x0) >= abs(y1 - y0)) {
		if (x0 > x1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}
		int st_x = std::max(left, x0);
		int end_x = std::min(right, x1 + 1);

		for (int x = st_x; x < end_x; ++x) {
			int y = y1 - (x1 - x) * (y1 - y0) / (x1 - x0);
			if (y < top || y >= bottom)
				continue;

			buf16_dst[y * cfp_dst->width + x] = color;
		}
	}
	else {
		if (y0 > y1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}
		int st_y = std::max(top, y0);
		int end_y = std::min(bottom, y1 + 1);

		for (int y = st_y; y < end_y; ++y) {
			int x = x1 - (y1 - y) * (x1 - x0) / (y1 - y0);
			if (x < left || x >= right)
				continue;

			buf16_dst[y * cfp_dst->width + x] = color;
		}
	}
}

void vm_graphic_line_ex(VMINT handle, VMINT x0, VMINT y0, VMINT x1, VMINT y1) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = get_current_app_graphic().global_color.vm_color_565;

	vm_graphic_line((VMUINT8*)layer.buf, x0, y0, x1, y1, c);
}

void vm_graphic_rect(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 color) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int st_x = std::max(0, x);
	int st_y = std::max(0, y);

	int end_x = std::min<int>(cfp_dst->width, x + width);
	int end_y = std::min<int>(cfp_dst->height, y + height);

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	if (st_x <= x && x < end_x)
		for (int sy = st_y; sy < end_y; ++sy)
			buf16_dst[sy * cfp_dst->width + x] = color;

	if (st_x <= x + width - 1 && x + width - 1 < end_x)
		for (int sy = st_y; sy < end_y; ++sy)
			buf16_dst[sy * cfp_dst->width + x + width - 1] = color;

	if (st_y <= y && y < end_y)
		for (int sx = st_x; sx < end_x; ++sx)
			buf16_dst[y * cfp_dst->width + sx] = color;

	if (st_y <= y + height - 1 && y + height - 1 < end_y)
		for (int sx = st_x; sx < end_x; ++sx)
			buf16_dst[(y + height - 1) * cfp_dst->width + sx] = color;
}

void vm_graphic_rect_ex(VMINT handle, VMINT x, VMINT y, VMINT width, VMINT height) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = get_current_app_graphic().global_color.vm_color_565;

	vm_graphic_rect((VMUINT8*)layer.buf, x, y, width, height, c);
}

void vm_graphic_fill_rect(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 line_color, VMUINT16 back_color) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int st_x = std::max(0, x);
	int st_y = std::max(0, y);

	int end_x = std::min<int>(cfp_dst->width, x + width);
	int end_y = std::min<int>(cfp_dst->height, y + height);

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx)
			if (x == sx || y == sy || sx == x + width - 1 || sy == y + height - 1)
				buf16_dst[sy * cfp_dst->width + sx] = line_color;
			else
				buf16_dst[sy * cfp_dst->width + sx] = back_color;
}

void vm_graphic_fill_rect_ex(VMINT handle, VMINT  x, VMINT  y, VMINT  width, VMINT  height) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = get_current_app_graphic().global_color.vm_color_565;

	vm_graphic_fill_rect((VMUINT8*)layer.buf, x, y, width, height, c, c);
}

inline bool on_round(int dx, int dy, int r) {
	if (dx < 0 || dy < 0)
		return false;
	if (dx >= r || dy >= r)
		return false;
	int cx = r - dx, cy = r - dy;
	int cr = cx * cx + cy * cy;
	return cr < (r) * (r) && cr > (r-1) * (r-1);
}

void vm_graphic_roundrect(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMINT corner_width, VMUINT16 color) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int st_x = std::max(0, x);
	int st_y = std::max(0, y);

	int end_x = std::min<int>(cfp_dst->width, x + width);
	int end_y = std::min<int>(cfp_dst->height, y + height);

	corner_width = std::min(corner_width, std::min(width / 2, height / 2));

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int dx1 = sx - x, dx2 = x + width - 1 - sx;
			int dy1 = sy - y, dy2 = y + height - 1 - sy;
			if (on_round(dx1, dy1, corner_width)
				|| on_round(dx2, dy1, corner_width)
				|| on_round(dx1, dy2, corner_width)
				|| on_round(dx2, dy2, corner_width)
				|| dx1 >= corner_width && dx2 >= corner_width && (dy1 == 0 || dy2 == 0)
				|| dy1 >= corner_width && dy2 >= corner_width && (dx1 == 0 || dx2 == 0)
				)
				buf16_dst[sy * cfp_dst->width + sx] = color;
		}
}

void vm_graphic_roundrect_ex(VMINT handle, VMINT x, VMINT y, VMINT width, VMINT height, VMINT frame_width) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = get_current_app_graphic().global_color.vm_color_565;

	vm_graphic_roundrect((VMUINT8*)layer.buf, x, y, width, height, frame_width, c);
}

inline bool in_round(int dx, int dy, int r) {
	if (dx < 0 || dy < 0)
		return false;
	if (dx >= r || dy >= r)
		return true;
	int cx = r - dx, cy = r - dy;
	return cx * cx + cy * cy < r * r;
}

void vm_graphic_fill_roundrect(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMINT corner_width, VMUINT16 color) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int st_x = std::max(0, x);
	int st_y = std::max(0, y);

	int end_x = std::min<int>(cfp_dst->width, x + width);
	int end_y = std::min<int>(cfp_dst->height, y + height);

	corner_width = std::min(corner_width, std::min(width / 2, height / 2));

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int dx1 = sx - x, dx2 = x + width - 1 - sx;
			int dy1 = sy - y, dy2 = y + height - 1 - sy;
			if (in_round(dx1, dy1, corner_width)
				&& in_round(dx2, dy1, corner_width)
				&& in_round(dx1, dy2, corner_width)
				&& in_round(dx2, dy2, corner_width) 
				)
				buf16_dst[sy * cfp_dst->width + sx] = color;
		}
}

void vm_graphic_fill_roundrect_ex(VMINT handle, VMINT x, VMINT y, VMINT width, VMINT height, VMINT frame_width) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = get_current_app_graphic().global_color.vm_color_565;

	vm_graphic_fill_roundrect((VMUINT8*)layer.buf, x, y, width, height, frame_width, c);
}


bool is_point_in_path(int x, int y, vm_graphic_point* point, VMINT npoints) {
	int j = npoints - 1;
	bool c = false;
	for (int i = 0; i < npoints; ++i) {
		if (x == point[i].x && y == point[i].y)
			return true;
		if ((point[i].y > y) != (point[j].y > y)) {
			int slope = (x - point[i].x) * (point[j].y - point[i].y) - (point[j].x - point[i].x) * (y - point[i].y);
			if (slope == 0)
				return true;
			if ((slope < 0) != (point[j].y < point[i].y))
				c = !c;
		}
		j = i;
	}
	return c;
}

void vm_graphic_fill_polygon(VMINT handle, vm_graphic_point* point, VMINT npoints) {
	if (npoints == 0)
		return;

	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	int st_x = point[0].x, st_y = point[0].y;
	int end_x = st_x, end_y = st_y;

	for (int i = 1; i < npoints; ++i) {
		if (st_x > point[i].x)
			st_x = point[i].x;
		if (st_y > point[i].y)
			st_y = point[i].y;
		if (end_x < point[i].x)
			end_x = point[i].x;
		if (end_y < point[i].y)
			end_y = point[i].y;
	}
	++end_x, ++end_y;

	if (st_x < 0)
		st_x = 0;
	if (st_y < 0)
		st_y = 0;
	if (end_x > layer.w)
		end_x = layer.w;
	if (end_y > layer.h)
		end_y = layer.h;


	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right + 1)
			end_x = clip.right + 1;
		if (end_y > clip.bottom + 1)
			end_y = clip.bottom + 1;
	}

	unsigned short color = get_current_app_graphic().global_color.vm_color_565;
	unsigned short* buf16_dst = (unsigned short*)layer.buf;

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx)
			if (is_point_in_path(sx, sy, point, npoints))
				buf16_dst[sy * layer.w + sx] = color;
}

void vm_graphic_set_clip(VMINT x1, VMINT y1, VMINT x2, VMINT y2) {
	auto& clip = get_current_app_graphic().clip;

	clip.left = x1;
	clip.top = y1;
	clip.right = x2;
	clip.bottom = y2;
	clip.flag = 1;
}

void vm_graphic_reset_clip(void) {
	auto& clip = get_current_app_graphic().clip;

	clip.left = 0;
	clip.top = 0;
	clip.right = graphic->width;
	clip.bottom = graphic->height;
	clip.flag = 0;
}

void vm_graphic_flush_screen(void) {
	void add_system_event(int phandle, int message, int param);
	add_system_event(vm_pmng_get_current_handle(), VM_MSG_PAINT, 0);
}

VMINT vm_graphic_is_r2l_state(void) {
	return 0;
}

VM_GDI_RESULT vm_graphic_setcolor(vm_graphic_color* color) {
	get_current_app_graphic().global_color = *color;
	return VM_GDI_SUCCEED;
}

VM_GDI_RESULT vm_graphic_canvas_set_trans_color(VMINT hcanvas, VMINT trans_color) {
	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)(ADDRESS_FROM_EMU(hcanvas));
	if (memcmp(cs->magic, CANVAS_MAGIC, 9))
		return VM_GDI_FAILED;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs + 1);

	//TODO frame index
	cfp_dst->flag = 1;
	cfp_dst->trans_color = trans_color;
	return VM_GDI_SUCCEED;
}