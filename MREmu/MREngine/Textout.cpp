#include "Graphic.h"
#include "../Memory.h"
#include <vmgraph.h>
#include "unifont.h"

extern MREngine::Graphic* graphic;

VMINT vm_graphic_get_character_height(void) {
	return 16; // temp
}

VMINT vm_graphic_get_character_width(VMWCHAR c) {
	int data_offset = ((unsigned int*)unifont_15_1_04_bin)[c];

	if (data_offset == 0)
		return 0;

	int ch_d = unifont_15_1_04_bin[data_offset];
	int ch_w = ch_d & 0xF;
	return ch_w + 1;
}

VMINT vm_graphic_get_string_width(VMWSTR str) {
	int w = 0;
	for (int i = 0; str[i]; ++i) {
		int data_offset = ((unsigned int*)unifont_15_1_04_bin)[str[i]];

		if (data_offset == 0)
			continue;

		int ch_d = unifont_15_1_04_bin[data_offset];
		int ch_w = ch_d & 0xF;

		w += ch_w + 1;
	}
	return w;
}

VMINT vm_graphic_get_string_height(VMWSTR str) {
	return vm_graphic_get_character_height();
}

VMINT vm_graphic_measure_character(VMWCHAR c, VMINT* width, VMINT* height) {
	if (width == 0 || height == 0)
		return VM_GDI_FAILED;

	*width = vm_graphic_get_character_width(c);
	*height = vm_graphic_get_character_height();

	return VM_GDI_SUCCEED;
}

VMINT vm_graphic_get_character_info(VMWCHAR c, vm_graphic_char_info* char_info) {
	if (char_info == 0)
		return -1;

	int data_offset = ((unsigned int*)unifont_15_1_04_bin)[c];

	if (data_offset == 0)
		return -1;

	int ch_d = unifont_15_1_04_bin[data_offset];
	int ch_w = ch_d & 0xF;

	char_info->dwidth = ch_w - (ch_d >> 4);
	char_info->width = ch_w + 1;
	char_info->height = 16;
	char_info->ascent = 2;
	char_info->descent = 0;

	return 0;
}

void vm_graphic_set_font(font_size_t size) {
}

void vm_graphic_textout(VMUINT8* disp_buf, VMINT x, VMINT y, VMWSTR s, VMINT length, VMUINT16 color) {
	if (disp_buf == 0)
		return;

	MREngine::canvas_signature* cs_dst = (MREngine::canvas_signature*)(disp_buf - VM_CANVAS_DATA_OFFSET);
	if (memcmp(cs_dst->magic, CANVAS_MAGIC, 9))
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
		if (right > clip.right)
			right = clip.right;
		if (bottom > clip.bottom)
			bottom = clip.bottom;
	}

	int st_y = std::max(top, y);
	int end_y = std::min<int>(bottom, y + 16);

	int x_off = x;
	for (int i = 0; s[i]; ++i) {
		int data_offset = ((unsigned int*)unifont_15_1_04_bin)[s[i]];

		if (data_offset == 0)
			continue;

		int ch_d = unifont_15_1_04_bin[data_offset];
		int ch_w = ch_d & 0xF;
		bool sho = ch_w >= 8;

		if (x_off >= right)
			break;

		if (x_off + ch_w < left) {
			x_off += ch_w + 1;
			continue;
		}

		int st_x = std::max(left, x_off);
		int end_x = std::min<int>(right, x_off + ch_w + 1);

		for (int sy = st_y; sy < end_y; ++sy) {
			int tex_ty = sy - y;
			unsigned short line = 0;
			if (sho)
				line = (unifont_15_1_04_bin[data_offset + 2 + tex_ty * 2] << 8) |
				unifont_15_1_04_bin[data_offset + 2 + tex_ty * 2 + 1];
			else
				line = unifont_15_1_04_bin[data_offset + 2 + tex_ty] << 8;

			for (int sx = st_x; sx < end_x; ++sx) {
				int im_x = sx - x_off;

				if ((line >> (15 - im_x)) & 1)
					buf16_dst[sy * cfp_dst->width + sx] = color;
			}
		}

		x_off += ch_w + 1;
	}
}

void vm_graphic_textout_by_baseline(VMUINT8* disp_buf, VMINT x, VMINT y, VMWSTR s, VMINT length, VMUINT16 color, VMINT baseline) {
	vm_graphic_textout(disp_buf, x, y, s, length, color); //TODO
}

VM_GDI_RESULT vm_font_set_font_size(VMINT size) {
	return VM_GDI_SUCCEED;
}

VM_GDI_RESULT vm_font_set_font_style(VMINT bold, VMINT italic, VMINT underline) {
	return VM_GDI_SUCCEED;
}

VM_GDI_RESULT vm_graphic_textout_to_layer(VMINT handle, VMINT x, VMINT y, VMWSTR s, VMINT length) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return VM_GDI_FAILED;

	auto& layer = layers[handle];

	vm_graphic_textout((VMUINT8*)layer.buf, x, y, s, length, get_current_app_graphic().global_color.vm_color_565);

	return VM_GDI_SUCCEED;
}

VMINT vm_graphic_get_string_baseline(VMWSTR string) {
	return 2;
}

VM_GDI_RESULT vm_graphic_textout_to_layer_by_baseline(VMINT handle, VMINT x, VMINT y, VMWSTR s, VMINT length, VMINT baseline);

VMINT vm_graphic_is_use_vector_font(void) {
	return FALSE;
}

VM_GDI_RESULT vm_graphic_draw_abm_text(VMINT handle, VMINT x, VMINT y, VMINT color, VMUINT8* font_data, VMINT font_width, VMINT font_height);

VMUINT vm_graphic_get_char_num_in_width(VMWCHAR* string, VMUINT width, VMINT  checklinebreak, VMUINT gap);

VMUINT vm_graphic_get_char_num_in_width_ex(VMWCHAR* string, VMUINT width, VMINT  checklinebreak, VMUINT gap);

VMUINT vm_get_string_width_height_ex(
	VMWCHAR* string,
	VMINT gap,
	VMINT n,
	VMINT* pWidth,
	VMINT* pHeight,
	VMINT max_width,
	VMUINT8 checkLineBreak,
	VMUINT8 checkCompleteWord);

vm_font_engine_error_message_enum vm_graphic_show_truncated_text(VM_GDI_HANDLE dest_layer_handle,
	VMINT x,
	VMINT y,
	VMINT xwidth,
	VMWCHAR* st,
	VMWCHAR* truncated_symbol,
	VMINT bordered,
	VMUINT16 color);

VMINT vm_graphic_get_character_height_ex(VMUWCHAR c);

VMINT vm_graphic_get_highest_char_height_of_all_language(void);

VMINT vm_graphic_get_char_height_alllang(VMINT size);

VMINT vm_graphic_get_char_baseline_alllang(VMINT size);