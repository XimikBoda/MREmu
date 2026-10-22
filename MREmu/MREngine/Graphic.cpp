#include "Graphic.h"
#include "../Graphic.h"
#include "Canvas.h"
#include "System.h"
#include "../Memory.h"
#include "../AppContext.h"
#include <SFML/Graphics/Image.hpp>
#include <vmgraph.h>
#include <vmpromng.h>
#include <format>
#include <cstring>

#ifdef DEBUG_WINDOW
#include <imgui.h>
#include <imgui-SFML.h>
#endif

MREngine::RenderBox::RenderBox(int st_x, int st_y, int end_x, int end_y) {
	this->st_x = st_x;
	this->st_y = st_y;
	this->end_x = end_x;
	this->end_y = end_y;
}

MREngine::RenderBox::RenderBox(const canvas_frame_property& cfp) {
	this->st_x = 0;
	this->st_y = 0;
	this->end_x = cfp.width;
	this->end_y = cfp.height;
}

void MREngine::RenderBox::clip(const canvas_frame_property& cfp) {
	if (st_x < 0)
		st_x = 0;
	if (st_y < 0)
		st_y = 0;
	if (end_x > cfp.width)
		end_x = cfp.width;
	if (end_y > cfp.height)
		end_y = cfp.height;
}

void MREngine::RenderBox::clip(const layer& layer) {
	if (st_x < 0)
		st_x = 0;
	if (st_y < 0)
		st_y = 0;
	if (end_x > layer.w)
		end_x = layer.w;
	if (end_y > layer.h)
		end_y = layer.h;
}

void MREngine::RenderBox::clip(const clip_rect& clip) {
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
}

void MREngine::RenderBox::clip(int x1, int y1, int x2, int y2) {
	if (st_x < x1)
		st_x = x1;
	if (st_y < y1)
		st_y = y1;
	if (end_x > x2)
		end_x = x2;
	if (end_y > y2)
		end_y = y2;
}

void MREngine::RenderBox::include(int x, int y) {
	if (st_x > x)
		st_x = x;
	if (st_y > y)
		st_y = y;
	if (end_x <= x)
		end_x = x;
	if (end_y <= y)
		end_y = y;
}

bool MREngine::RenderBox::in(int x, int y) {
	return x >= st_x && y >= st_y && x < end_x && y < end_y;
}

bool MREngine::RenderBox::x_in(int x) {
	return x >= st_x && x < end_x;
}

bool MREngine::RenderBox::y_in(int y) {
	return y >= st_y && y < end_y;
}


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

MREngine::layer* MREngine::AppGraphic::find_layer_by_buf(void* buf)
{
	for (auto& layer : layers)
		if (layer.buf == buf)
			return &layer;

	return 0;
}

clip_rect MREngine::AppGraphic::clip_by_buf(void* buf) {
	auto layer = find_layer_by_buf(buf);
	if (layer)
		return layer->clip;
	else
		return { 0, 0, 0, 0, 0 };
}

int MREngine::AppGraphic::create_layer(int x, int y, int w, int h, int trans_color) {
	auto graphic = Graphic::getPtr();

	if (layers.size() == 0) {
		if (x != 0 || y != 0 || w != graphic->width || h != graphic->height)
			return -1;

		layers.push_back({ graphic->base_buf1, x, y, w, h, trans_color, {0, 0, (short)w, (short)h, 0} });
		active_layer = 0;
		return 0;
	}
	else if (layers.size() == 1) {
		if (w > graphic->width || h > graphic->height)
			return -1;

		MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)graphic->base_buf2 - 1;

		cfp->width = w;
		cfp->height = h;

		layers.push_back({ graphic->base_buf2, x, y, w, h, trans_color, {0, 0, (short)w, (short)h, 0} });
		return 1;
	}
	else
		abort();
}

int MREngine::AppGraphic::create_layer_ex(int x, int y, int w, int h, int trans_color, int mode, void* buf) {
	auto graphic = Graphic::getPtr();

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

	layers.push_back({ buf16, x, y, w, h, trans_color, {0, 0, (short)w, (short)h, 0} });

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

#ifdef DEBUG_WINDOW
void MREngine::AppGraphic::imgui_layers() {
	if (ImGui::Begin("Layers")) {
		for (int i = 0; i < layers.size(); ++i) {
			auto& el = layers[i];
			if (el.w > 1000 || el.h > 1000)
				return;
			buf_to_texture(el.buf, el.w, el.h, el.tex);
			ImGui::Text("Id: %d, x: %d, y: %d, w: %d, h: %d, t: %d",
				i, el.x, el.y, el.w, el.h, el.trans_color);
			ImGui::Image(el.tex);
		}
	}
	ImGui::End();
}
#endif

//MRE API

VMINT vm_graphic_get_screen_width(void)
{
	auto graphic = Graphic::getPtr();

	if (graphic)
		return graphic->width;
	else
		return 0;
}

VMINT vm_graphic_get_screen_height(void)
{
	auto graphic = Graphic::getPtr();

	if (graphic)
		return graphic->height;
	else
		return 0;
}


VMINT vm_graphic_create_layer(VMINT x, VMINT y, VMINT width, VMINT height, VMINT trans_color) {
	return AppContext::get_executing_app_graphic().create_layer(x, y, width, height, trans_color);
}

VM_GDI_HANDLE vm_graphic_create_layer_ex(VMINT x, VMINT y, VMINT width, VMINT height, VMINT trans_color, VMINT mode, VMUINT8* buf) {
	return AppContext::get_executing_app_graphic().create_layer_ex(x, y, width, height, trans_color, mode, buf);
}

VM_GDI_HANDLE vm_graphic_create_layer_cf(vm_graphic_color_famat cf, VMINT x, VMINT y, VMINT width, VMINT height, vm_graphic_color_argb* trans_color, VMINT mode, VMUINT8* buf, VMINT buf_size) {
	if (cf != VM_GRAPHIC_COLOR_FORMAT_16)
		return -1;

	return vm_graphic_create_layer_ex(x, y, width, height, -1, mode, buf);
}

VMINT vm_graphic_delete_layer(VMINT handle) {
	return AppContext::get_executing_app_graphic().delete_layer(handle);
}

VMINT vm_graphic_active_layer(VMINT handle) {
	auto& gr = AppContext::get_executing_app_graphic();
	if (handle < 0 || handle >= gr.layers.size())
		return VM_GDI_FAILED;
	gr.active_layer = handle;
	return VM_GDI_SUCCEED;
}

VMUINT8* vm_graphic_get_layer_buffer(VMINT handle) {
	return (VMUINT8*)AppContext::get_executing_app_graphic().get_layer_buf(handle);
}

VMINT vm_graphic_clear_layer_bg(VMINT handle) {
	auto& gr = AppContext::get_executing_app_graphic();

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
	MREngine::AppGraphic& gr = AppContext::get_executing_app_graphic();

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

	if (x < 0 || x >= 0 || y < cfp_dst->width || y >= cfp_dst->height)
		return 0;

	return buf16_dst[y * cfp_dst->width + x];
}

void vm_graphic_set_pixel(VMUINT8* buf, VMINT x, VMINT y, VMUINT16 color) {
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	MREngine::RenderBox b(*cfp_dst);

	b.clip(gr.clip);
	b.clip(gr.clip_by_buf(buf16_dst));

	if (b.in(x, y))
		buf16_dst[y * cfp_dst->width + x] = color;
}

void vm_graphic_set_pixel_ex(VMINT handle, VMINT x1, VMINT y1) {
	auto& gr = AppContext::get_executing_app_graphic();
	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = gr.global_color.vm_color_565;

	vm_graphic_set_pixel((VMUINT8*)layer.buf, x1, y1, c);
}

void vm_graphic_line(VMUINT8* buf, VMINT x0, VMINT y0, VMINT x1, VMINT y1, VMUINT16 color) {
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	if (x0 == x1 && y0 == y1)
		return vm_graphic_set_pixel(buf, x0, y0, color);

	MREngine::RenderBox b(*cfp_dst);

	b.clip(gr.clip);
	b.clip(gr.clip_by_buf(buf16_dst));

	if (abs(x1 - x0) >= abs(y1 - y0)) {
		if (x0 > x1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}
		b.clip(x0, std::min(y0, y1), x1 + 1, std::max(y0, y1) + 1);

		for (int x = b.st_x; x < b.end_x; ++x) {
			int y = y1 - (x1 - x) * (y1 - y0) / (x1 - x0);
			if (b.y_in(y))
				buf16_dst[y * cfp_dst->width + x] = color;
		}
	}
	else {
		if (y0 > y1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}
		b.clip(std::min(x0, x1), y0, std::max(x0, x1) + 1, y1 + 1);

		for (int y = b.st_y; y < b.end_y; ++y) {
			int x = x1 - (y1 - y) * (x1 - x0) / (y1 - y0);
			if (b.x_in(x))
				buf16_dst[y * cfp_dst->width + x] = color;
		}
	}
}

void vm_graphic_line_ex(VMINT handle, VMINT x0, VMINT y0, VMINT x1, VMINT y1) {
	auto& gr = AppContext::get_executing_app_graphic();
	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = gr.global_color.vm_color_565;

	vm_graphic_line((VMUINT8*)layer.buf, x0, y0, x1, y1, c);
}

void vm_graphic_rect(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 color) {
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	MREngine::RenderBox b(x, y, x + width, y + height);

	b.clip(*cfp_dst);
	b.clip(gr.clip);
	b.clip(gr.clip_by_buf(buf16_dst));

	if (b.st_x <= x && x < b.end_x)
		for (int sy = b.st_y; sy < b.end_y; ++sy)
			buf16_dst[sy * cfp_dst->width + x] = color;

	if (b.st_x <= x + width - 1 && x + width - 1 < b.end_x)
		for (int sy = b.st_y; sy < b.end_y; ++sy)
			buf16_dst[sy * cfp_dst->width + x + width - 1] = color;

	if (b.st_y <= y && y < b.end_y)
		for (int sx = b.st_x; sx < b.end_x; ++sx)
			buf16_dst[y * cfp_dst->width + sx] = color;

	if (b.st_y <= y + height - 1 && y + height - 1 < b.end_y)
		for (int sx = b.st_x; sx < b.end_x; ++sx)
			buf16_dst[(y + height - 1) * cfp_dst->width + sx] = color;
}

void vm_graphic_rect_ex(VMINT handle, VMINT x, VMINT y, VMINT width, VMINT height) {
	auto& gr = AppContext::get_executing_app_graphic();
	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = gr.global_color.vm_color_565;

	vm_graphic_rect((VMUINT8*)layer.buf, x, y, width, height, c);
}

void vm_graphic_fill_rect(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 line_color, VMUINT16 back_color) {
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	MREngine::RenderBox b(x, y, x + width, y + height);

	b.clip(*cfp_dst);
	b.clip(gr.clip);
	b.clip(gr.clip_by_buf(buf16_dst));

	for (int sy = b.st_y; sy < b.end_y; ++sy)
		for (int sx = b.st_x; sx < b.end_x; ++sx)
			if (x == sx || y == sy || sx == x + width - 1 || sy == y + height - 1)
				buf16_dst[sy * cfp_dst->width + sx] = line_color;
			else
				buf16_dst[sy * cfp_dst->width + sx] = back_color;
}

void vm_graphic_fill_rect_ex(VMINT handle, VMINT  x, VMINT  y, VMINT  width, VMINT  height) {
	auto& gr = AppContext::get_executing_app_graphic();
	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = gr.global_color.vm_color_565;

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
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	MREngine::RenderBox b(x, y, x + width , y + height);

	b.clip(*cfp_dst);
	b.clip(gr.clip);
	b.clip(gr.clip_by_buf(buf16_dst));

	corner_width = std::min(corner_width, std::min(width / 2, height / 2));

	for (int sy = b.st_y; sy < b.end_y; ++sy)
		for (int sx = b.st_x; sx < b.end_x; ++sx) {
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
	auto& gr = AppContext::get_executing_app_graphic();
	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = gr.global_color.vm_color_565;

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
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	MREngine::RenderBox b(x, y, x + width, y + height);

	b.clip(*cfp_dst);
	b.clip(gr.clip);
	b.clip(gr.clip_by_buf(buf16_dst));

	corner_width = std::min(corner_width, std::min(width / 2, height / 2));

	for (int sy = b.st_y; sy < b.end_y; ++sy)
		for (int sx = b.st_x; sx < b.end_x; ++sx) {
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
	auto& gr = AppContext::get_executing_app_graphic();
	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	unsigned short c = gr.global_color.vm_color_565;

	vm_graphic_fill_roundrect((VMUINT8*)layer.buf, x, y, width, height, frame_width, c);
}

void vm_graphic_ellipse(VMUINT8* buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 color) {
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	MREngine::RenderBox rb(x, y, x + width, y + height);

	rb.clip(*cfp_dst);
	rb.clip(gr.clip);
	rb.clip(gr.clip_by_buf(buf16_dst));

	auto draw_pixel = [&](int px, int py) {
		if (rb.in(px, py)) {
			buf16_dst[py * cfp_dst->width + px] = color;
		}};

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
	auto& gr = AppContext::get_executing_app_graphic();

	MREngine::canvas_signature* cs_dst = find_canvas_signature(buf);
	if (!cs_dst)
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	MREngine::RenderBox b(x, y, x + width, y + height);

	b.clip(*cfp_dst);
	b.clip(gr.clip);
	b.clip(gr.clip_by_buf(buf16_dst));

	long long RX = width;
	long long RY = height;
	long long RX_sq = RX * RX;
	long long RY_sq = RY * RY;
	long long RX_sq_RY_sq = RX_sq * RY_sq;

	for (int sy = b.st_y; sy < b.end_y; ++sy) {
		long long DY = 2LL * sy + 1LL - 2LL * y - height;
		long long DY_sq_RX_sq = DY * DY * RX_sq;

		for (int sx = b.st_x; sx < b.end_x; ++sx) {
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
	auto& gr = AppContext::get_executing_app_graphic();

	if (npoints == 0)
		return;

	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];

	MREngine::RenderBox b(point[0].x, point[0].y, point[0].x + 1, point[0].y +1);

	for (int i = 1; i < npoints; ++i) 
		b.include(point[i].x, point[i].y);

	b.clip(layer);
	b.clip(gr.clip);
	b.clip(layer.clip);

	unsigned short color = gr.global_color.vm_color_565;
	unsigned short* buf16_dst = (unsigned short*)layer.buf;

	for (int sy = b.st_y; sy < b.end_y; ++sy)
		for (int sx = b.st_x; sx < b.end_x; ++sx)
			if (is_point_in_path(sx, sy, point, npoints))
				buf16_dst[sy * layer.w + sx] = color;
}

static int map_value(int ab, int ae, int bb, int be, int bv) {
	return (bv - bb) * (ae - ab) / (be - bb) + ab;
}

static uint16_t map_color16(uint16_t bc, uint16_t ec, int ib, int ie, int iv) {
	if (ib == ie)
		return bc;

	int bcr5 = (bc & VM_COLOR_RED) >> 11;
	int ecr5 = (ec & VM_COLOR_RED) >> 11;
	int bcg6 = (bc & VM_COLOR_GREEN) >> 5;
	int ecg6 = (ec & VM_COLOR_GREEN) >> 5;
	int bcb5 = (bc & VM_COLOR_BLUE);
	int ecb5 = (ec & VM_COLOR_BLUE);

	int r5 = map_value(bcr5, ecr5, ib, ie, iv);
	int g6 = map_value(bcg6, ecg6, ib, ie, iv);
	int b5 = map_value(bcb5, ecb5, ib, ie, iv);

	if (r5 < 0) r5 = 0; else if (r5 > 31) r5 = 31;
	if (g6 < 0) g6 = 0; else if (g6 > 63) g6 = 63;
	if (b5 < 0) b5 = 0; else if (b5 > 31) b5 = 31;

	return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

void vm_graphic_gradient_paint_rect(VMINT handle, VMINT x1, VMINT y1, VMINT x2, VMINT y2, vm_graphic_color color_start, vm_graphic_color color_end, vm_graphic_gp_style style) {
	auto& gr = AppContext::get_executing_app_graphic();
	auto& layers = gr.layers;

	if (handle < 0 || handle >= layers.size())
		return;

	auto& layer = layers[handle];
	unsigned short* buf16_dst = (unsigned short*)layer.buf;

	MREngine::RenderBox b(x1, y1, x2, y2);

	b.clip(layer);
	b.clip(gr.clip);
	b.clip(layer.clip);

	uint16_t c1 = color_start.vm_color_565, c2 = color_end.vm_color_565;

	int min_v = 0, max_v = 0;
	switch (style) {
	case VM_GDI_GP_HOR:
		min_v = x1, max_v = x2;
		for (int sx = b.st_x; sx < b.end_x; ++sx) {
			uint16_t color = map_color16(c1, c2, min_v, max_v, sx);
			for (int sy = b.st_y; sy < b.end_y; ++sy)
				buf16_dst[sy * layer.w + sx] = color;
		}
		break;
	case VM_GDI_GP_VER:
		min_v = y1, max_v = y2;
		for (int sy = b.st_y; sy < b.end_y; ++sy) {
			uint16_t color = map_color16(c1, c2, min_v, max_v, sy);
			for (int sx = b.st_x; sx < b.end_x; ++sx)
				buf16_dst[sy * layer.w + sx] = color;
		}
		break;
	case VM_GDI_GP_DIA:
	case VM_GDI_GP_DIA_INV:
		min_v = x1 + y1, max_v = x2 + y2;
		if (style == VM_GDI_GP_DIA_INV)
			std::swap(min_v, max_v);

		for (int sy = b.st_y; sy < b.end_y; ++sy)
			for (int sx = b.st_x; sx < b.end_x; ++sx) {
				int v = sx + sy;
				buf16_dst[sy * layer.w + sx] = map_color16(c1, c2, min_v, max_v, v);
			}
		break;
	case VM_GDI_GP_FLIP:
		min_v = y1, max_v = (y1 + y2) / 2; 

		for (int sy = b.st_y; sy < b.end_y; ++sy) {
			uint16_t color = map_color16(c1, c2, min_v, max_v, std::abs(sy - max_v));
			for (int sx = b.st_x; sx < b.end_x; ++sx)
				buf16_dst[sy * layer.w + sx] = color;
		}
		break;
	default:
		return;
	}
}


VM_GDI_RESULT vm_graphic_get_layer_clip(VMINT handle, clip_rect* curcliprect) {
	auto& layers = AppContext::get_executing_app_graphic().layers;

	if (handle < 0 || handle >= layers.size() || !curcliprect)
		return VM_GDI_FAILED;

	auto& layer = layers[handle];

	*curcliprect = layer.clip;

	return VM_GDI_SUCCEED;
}

VM_GDI_RESULT vm_graphic_set_layer_clip(VMINT handle, VMINT16 x1, VMINT16 y1, VMINT16 x2, VMINT16 y2) {
	auto& layers = AppContext::get_executing_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return VM_GDI_FAILED;

	auto& layer = layers[handle];

	layer.clip = { x1, y1, x2, y2, 1 };

	return VM_GDI_SUCCEED;
}

void vm_graphic_set_clip(VMINT x1, VMINT y1, VMINT x2, VMINT y2) {
	auto& clip = AppContext::get_executing_app_graphic().clip;

	clip = { (short)x1, (short)y1, (short)x2, (short)y2, 1 };
}

void vm_graphic_reset_clip(void) {
	auto graphic = Graphic::getPtr();
	auto& clip = AppContext::get_executing_app_graphic().clip;

	clip = { 0, 0, (short)graphic->width, (short)graphic->height, 0 };
}

void vm_graphic_flush_screen(void) {
	AppContext::push_event(Event::system{ vm_pmng_get_current_handle(), Event::system_msg{ VM_MSG_PAINT, 0} });
}

VMINT vm_graphic_get_blt_layer_count(void) {
	return AppContext::get_executing_app_graphic().layers.size();
}

VMINT vm_graphic_is_r2l_state(void) {
	return 0;
}

VM_GDI_RESULT vm_graphic_setcolor(vm_graphic_color* color) {
	AppContext::get_executing_app_graphic().global_color = *color;
	return VM_GDI_SUCCEED;
}