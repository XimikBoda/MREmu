#include "Graphic.h"
#include "Canvas.h"
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

	tex.loadFromImage(im);
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
	if (screen_changed) {
		buf_to_texture(screen.data(), width, height, screen_tex);
		screen_changed = false;
	}
}

void MREngine::Graphic::imgui_screen() {
	if (ImGui::Begin("Screen")) {
		ImGui::Image(screen_tex);
		if (ImGui::Button("Paint"))
			vm_graphic_flush_screen();
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

VMINT vm_graphic_clear_layer_bg(VMINT handle) {
	auto& gr = get_current_app_graphic();

	if (handle < 0 || handle >= gr.layers.size())
		return VM_GDI_FAILED;

	auto& layer = gr.layers[handle];
	auto trans_color = layer.trans_color;

	if(trans_color == -1)
		return VM_GDI_FAILED;

	vm_graphic_fill_rect((VMUINT8*)layer.buf, 0, 0, layer.w, layer.h, trans_color, trans_color);
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

VMUINT16 vm_graphic_get_pixel(VMUINT8* buf, VMINT x, VMINT y) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return 0;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int left = 0;
	int top = 0;
	int right = cfp_dst->width;
	int bottom = cfp_dst->height;

	if (x < left || x >= right || y < top || y >= bottom)
		return 0;

	return buf16_dst[y * cfp_dst->width + x];
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
	return cr < (r) * (r) && cr >(r - 1) * (r - 1);
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

void vm_graphic_ellipse(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 color) {
	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int clip_left = 0;
	int clip_top = 0;
	int clip_right = cfp_dst->width - 1;
	int clip_bottom = cfp_dst->height - 1;

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		clip_left = std::max(clip_left, (int)clip.left);
		clip_top = std::max(clip_top, (int)clip.top);
		clip_right = std::min(clip_right, (int)clip.right);
		clip_bottom = std::min(clip_bottom, (int)clip.bottom);
	}

	auto draw_pixel = [&](int px, int py) {
		if (px >= clip_left && px <= clip_right && py >= clip_top && py <= clip_bottom) {
			buf16_dst[py * cfp_dst->width + px] = color;
		}
		};

	int x0 = x;
	int y0 = y;
	int x1 = x + width - 1;
	int y1 = y + height - 1;

	long long a = std::abs(x1 - x0);
	long long b = std::abs(y1 - y0);
	long long b1 = b & 1;

	long long dx = 4LL * (1LL - a) * b * b;
	long long dy = 4LL * (b1 + 1LL) * a * a;
	long long err = dx + dy + b1 * a * a;
	long long e2;

	if (x0 > x1) { x0 = x1; x1 += a; }
	if (y0 > y1) { y0 = y1; }

	y0 += (b + 1) / 2;
	y1 = y0 - b1;

	a *= 8LL * a;
	b1 = 8LL * b * b;

	do {
		draw_pixel(x1, y0);
		draw_pixel(x0, y0);
		draw_pixel(x0, y1);
		draw_pixel(x1, y1);

		e2 = 2LL * err;
		if (e2 <= dy) {
			y0++;
			y1--;
			err += dy += a;
		}
		if (e2 >= dx || 2LL * err > dy) {
			x0++;
			x1--;
			err += dx += b1;
		}
	} while (x0 <= x1);

	while (y0 - y1 < b) {
		draw_pixel(x0 - 1, y0);
		draw_pixel(x1 + 1, y0++);
		draw_pixel(x0 - 1, y1);
		draw_pixel(x1 + 1, y1--);
	}
}

void vm_graphic_fill_ellipse(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 color) {
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

	long long RX = width;
	long long RY = height;
	long long RX_sq = RX * RX;
	long long RY_sq = RY * RY;
	long long RX_sq_RY_sq = RX_sq * RY_sq;

	for (int sy = st_y; sy < end_y; ++sy) {
		long long DY = 2LL * sy + 1LL - 2LL * y - height;
		long long DY_sq_RX_sq = DY * DY * RX_sq;

		for (int sx = st_x; sx < end_x; ++sx) {
			long long DX = 2LL * sx + 1LL - 2LL * x - width;

			if ((DX * DX * RY_sq) + DY_sq_RX_sq <= RX_sq_RY_sq) {
				buf16_dst[sy * cfp_dst->width + sx] = color;
			}
		}
	}
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