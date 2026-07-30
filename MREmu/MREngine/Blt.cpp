#include "Graphic.h"
#include "Canvas.h"
#include "Blt.h"
#include <vmgraph.h>

extern MREngine::Graphic* graphic;

ColorRGBA read_16_color(const void* buf, int idx, int trans_color) {
	uint16_t c = ((const uint16_t*)buf)[idx];
	if ((int)c == trans_color)
		return { 0, 0, 0, 0 };
	else
		return { (uint8_t)VM_COLOR_GET_RED(c), (uint8_t)VM_COLOR_GET_GREEN(c), 
			(uint8_t)VM_COLOR_GET_BLUE(c), 255 };
}

ColorRGBA read_24_color(const void* buf, int idx, int trans_color) {
	const uint8_t* c = ((const uint8_t*)buf) + idx * 3;

	uint32_t color_val = c[0] | (c[1] << 8) | (c[2] << 16);

	if ((int)color_val == trans_color)
		return { 0, 0, 0, 0 };
	else
		return { c[0], c[1], c[2], 255 };
}

ColorRGBA read_32_color(const void* buf, int idx, int trans_color) {
	return ((const ColorRGBA*)buf)[idx];
}

void write_16_color(void* buf, int idx, int trans_color, ColorRGBA c) {
	uint16_t* d_c = ((uint16_t*)buf) + idx;

	if (c.a == 0 && trans_color != VM_NO_TRANS_COLOR)
		*d_c = trans_color;
	else
		*d_c = VM_COLOR_888_TO_565(c.r, c.g, c.b);
}

void write_24_color(void* buf, int idx, int trans_color, ColorRGBA c) {
	uint8_t* p = (uint8_t*)buf + (idx * 3);

	if (c.a == 0 && trans_color != VM_NO_TRANS_COLOR) {
		p[0] = trans_color & 0xFF; 
		p[1] = (trans_color >> 8) & 0xFF; 
		p[2] = (trans_color >> 16) & 0xFF;
	}
	else {
		p[0] = c.r, p[1] = c.g, p[2] = c.b;
	}
}

void write_32_color(void* buf, int idx, int trans_color, ColorRGBA c) {
	((ColorRGBA*)buf)[idx] = c;
}

read_color_t read_color_funcs[4] = { read_16_color, read_24_color, read_32_color, read_32_color };
write_color_t write_color_funcs[4] = { write_16_color, write_24_color, write_32_color, write_32_color };

template <bool HasGlobalAlpha, bool IsSrcPARGB>
ColorRGBA blend_rgba(ColorRGBA src, ColorRGBA dst, uint8_t global_alpha) {
	if constexpr (HasGlobalAlpha) {
		src.a = (src.a * global_alpha) / 255;

		if constexpr (IsSrcPARGB) {
			src.r = (src.r * global_alpha) / 255;
			src.g = (src.g * global_alpha) / 255;
			src.b = (src.b * global_alpha) / 255;
		}
	}

	if (src.a == 0) return dst;
	if (src.a == 255) return src;

	ColorRGBA out;
	uint8_t src_r_pre = src.r;
	uint8_t src_g_pre = src.g;
	uint8_t src_b_pre = src.b;

	if constexpr (!IsSrcPARGB) {
		src_r_pre = (src.r * src.a) / 255;
		src_g_pre = (src.g * src.a) / 255;
		src_b_pre = (src.b * src.a) / 255;
	}

	if (dst.a == 255) {
		out.a = 255;
		out.r = src_r_pre + (dst.r * (255 - src.a)) / 255;
		out.g = src_g_pre + (dst.g * (255 - src.a)) / 255;
		out.b = src_b_pre + (dst.b * (255 - src.a)) / 255;
		return out;
	}

	out.a = src.a + (dst.a * (255 - src.a)) / 255;
	out.r = (src_r_pre * 255 + dst.r * dst.a * (255 - src.a) / 255) / out.a;
	out.g = (src_g_pre * 255 + dst.g * dst.a * (255 - src.a) / 255) / out.a;
	out.b = (src_b_pre * 255 + dst.b * dst.a * (255 - src.a) / 255) / out.a;
	return out;
}

template <bool HasGlobalAlpha, bool IsSrcPARGB = false, typename CoordMapper>
void master_blt(
	void* dst_buf, int dst_w, int dst_h, vm_graphic_color_famat dst_fmt, int dst_tc,
	const void* src_buf, int src_w, int src_h, vm_graphic_color_famat src_fmt, int src_tc,
	int st_x, int st_y, int end_x, int end_y,
	uint8_t global_alpha, CoordMapper map_coords) 
{
	if (dst_fmt >= VM_GRAPHIC_COLOR_FORMAT_END || src_fmt >= VM_GRAPHIC_COLOR_FORMAT_END)
		return;

	if constexpr (!IsSrcPARGB) {
		if (src_fmt == VM_GRAPHIC_COLOR_FORMAT_32_PARGB) {
			master_blt<HasGlobalAlpha, true>(
				dst_buf, dst_w, dst_h, dst_fmt, dst_tc,
				src_buf, src_w, src_h, src_fmt, src_tc,
				st_x, st_y, end_x, end_y, global_alpha, map_coords);
			return;
		}
	}

	if (dst_fmt == VM_GRAPHIC_COLOR_FORMAT_16 &&
		src_fmt == VM_GRAPHIC_COLOR_FORMAT_16 && 
		!HasGlobalAlpha) 
	{
		uint16_t* buf16_dst = (uint16_t*)dst_buf;
		const uint16_t* buf16_src = (const uint16_t*)src_buf;

		bool flag = src_tc != -1;
		uint16_t trans_color = src_tc;

		for (int sy = st_y; sy < end_y; ++sy)
			for (int sx = st_x; sx < end_x; ++sx) {
				auto im_c = map_coords(sx, sy);

				uint16_t scr_color = buf16_src[im_c.second * src_w + im_c.first];
				if (!flag || scr_color != trans_color)
					buf16_dst[sy * dst_w + sx] = scr_color;
			}
	}
	else {
		auto dst_read_func = read_color_funcs[dst_fmt];
		auto src_read_func = read_color_funcs[src_fmt];
		auto dst_write_func = write_color_funcs[dst_fmt];

		for (int sy = st_y; sy < end_y; ++sy)
			for (int sx = st_x; sx < end_x; ++sx) {
				auto im_c = map_coords(sx, sy);

				int dst_ind = sy * dst_w + sx;
				int src_ind = im_c.second * src_w + im_c.first;

				auto dst_color = dst_read_func(dst_buf, dst_ind, dst_tc);
				auto src_color = src_read_func(src_buf, src_ind, src_tc);

				auto blended_color = blend_rgba<HasGlobalAlpha, IsSrcPARGB>(
					src_color, dst_color, global_alpha);

				dst_write_func(dst_buf, dst_ind, dst_tc, blended_color);
			}
		
	}
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

	graphic->screen_changed = true;
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

	graphic->screen_changed = true;
	return VM_GDI_SUCCEED;
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
	void* buf_dst = (void*)(cfp_dst + 1);
	void* buf_src = (void*)(cfp_src + 1);

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

	auto blt_mapper = [x_dest, y_dest, x_src, y_src](int sx, int sy) -> std::pair<int, int> {
		return { sx - x_dest + x_src, sy - y_dest + y_src };
		};

	master_blt<false>(
		buf_dst, cfp_dst->width, cfp_dst->height, cs_dst->color_format, cfp_dst->trans_color,
		buf_src, cfp_src->width, cfp_src->height, cs_src->color_format, cfp_src->trans_color,
		st_x, st_y, end_x, end_y,
		255, blt_mapper);
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
	void* buf_dst = (void*)(cfp_dst + 1);
	void* buf_src = (void*)(cfp_src + 1);

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

	auto blt_mapper = [x_dest, y_dest, x_src, y_src](int sx, int sy) -> std::pair<int, int> {
		return { sx - x_dest + x_src, sy - y_dest + y_src };
		};

	master_blt<true>(
		buf_dst, cfp_dst->width, cfp_dst->height, cs_dst->color_format, cfp_dst->trans_color,
		buf_src, cfp_src->width, cfp_src->height, cs_src->color_format, cfp_src->trans_color,
		st_x, st_y, end_x, end_y,
		alpha, blt_mapper);
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
	void* buf_dst = (void*)(cfp_dst + 1);
	void* buf_src = (void*)(cfp_src + 1);

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

	int src_w = cfp_src->width;
	int src_h = cfp_src->height;
	vm_graphic_color_famat dst_fmt = cs_dst->color_format;
	vm_graphic_color_famat src_fmt = cs_src->color_format;
	int dst_tc = cfp_dst->trans_color;
	int src_tc = cfp_src->trans_color;

	switch (degrees) {
	case VM_ROTATE_DEGREE_90:
		master_blt<false>(
			buf_dst, cfp_dst->width, cfp_dst->height, dst_fmt, dst_tc,
			buf_src, src_w, src_h, src_fmt, src_tc,
			st_x, st_y, end_x, end_y, 255,
			[x_des, y_des, src_h](int sx, int sy) -> std::pair<int, int> {
				return { sy - y_des, src_h - (sx - x_des) - 1 };
			}
		);
		break;

	case VM_ROTATE_DEGREE_180:
		master_blt<false>(
			buf_dst, cfp_dst->width, cfp_dst->height, dst_fmt, dst_tc,
			buf_src, src_w, src_h, src_fmt, src_tc,
			st_x, st_y, end_x, end_y, 255,
			[x_des, y_des, src_w, src_h](int sx, int sy) -> std::pair<int, int> {
				return { src_w - (sx - x_des) - 1, src_h - (sy - y_des) - 1 };
			}
		);
		break;

	case VM_ROTATE_DEGREE_270:
		master_blt<false>(
			buf_dst, cfp_dst->width, cfp_dst->height, dst_fmt, dst_tc,
			buf_src, src_w, src_h, src_fmt, src_tc,
			st_x, st_y, end_x, end_y, 255,
			[x_des, y_des, src_w](int sx, int sy) -> std::pair<int, int> {
				return { src_w - (sy - y_des) - 1, sx - x_des };
			}
		);
		break;

	default:
		master_blt<false>(
			buf_dst, cfp_dst->width, cfp_dst->height, dst_fmt, dst_tc,
			buf_src, src_w, src_h, src_fmt, src_tc,
			st_x, st_y, end_x, end_y, 255,
			[x_des, y_des](int sx, int sy) -> std::pair<int, int> {
				return { sx - x_des, sy - y_des };
			}
		);
		break;
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
	void* buf_dst = (void*)(cfp_dst + 1);
	void* buf_src = (void*)(cfp_src + 1);

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

	int src_w = cfp_src->width;
	int src_h = cfp_src->height;
	vm_graphic_color_famat dst_fmt = cs_dst->color_format;
	vm_graphic_color_famat src_fmt = cs_src->color_format;
	int dst_tc = cfp_dst->trans_color;
	int src_tc = cfp_src->trans_color;

	if (direction == VM_MIRROR_X) {
		master_blt<false>(
			buf_dst, cfp_dst->width, cfp_dst->height, dst_fmt, dst_tc,
			buf_src, src_w, src_h, src_fmt, src_tc,
			st_x, st_y, end_x, end_y, 255,
			[x_des, y_des, src_w](int sx, int sy) -> std::pair<int, int> {
				return { src_w - (sx - x_des) - 1, sy - y_des };
			}
		);
	}
	else if (direction == VM_MIRROR_Y) {
		master_blt<false>(
			buf_dst, cfp_dst->width, cfp_dst->height, dst_fmt, dst_tc,
			buf_src, src_w, src_h, src_fmt, src_tc,
			st_x, st_y, end_x, end_y, 255,
			[x_des, y_des, src_h](int sx, int sy) -> std::pair<int, int> {
				return { sx - x_des, src_h - (sy - y_des) - 1 };
			}
		);
	}
	else {
		master_blt<false>(
			buf_dst, cfp_dst->width, cfp_dst->height, dst_fmt, dst_tc,
			buf_src, src_w, src_h, src_fmt, src_tc,
			st_x, st_y, end_x, end_y, 255,
			[x_des, y_des](int sx, int sy) -> std::pair<int, int> {
				return { sx - x_des, sy - y_des };
			}
		);
	}
}