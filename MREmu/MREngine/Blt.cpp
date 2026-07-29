#include "Graphic.h"
#include "Canvas.h"

extern MREngine::Graphic* graphic;

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