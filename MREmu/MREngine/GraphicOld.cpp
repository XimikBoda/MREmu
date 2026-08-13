#include "Graphic.h"
#include "Image.h"
#include <vmgraph.h>
#include <vmgfxold.h>

void vm_graphic_lock(void) {
	MREngine::AppGraphic& gr = get_current_app_graphic();
	
	if (gr.old_layer == -1 && !gr.old_layer_inited) {
		int w = vm_graphic_get_screen_width();
		int h = vm_graphic_get_screen_height();

		gr.old_layer = vm_graphic_create_layer(0, 0, w, h, -1);

		vm_graphic_set_clip(0, 0, w, h);
	}
}

void vm_graphic_unlock(void) {
	MREngine::AppGraphic& gr = get_current_app_graphic();

	if (gr.old_layer != -1) {
		vm_graphic_flush_layer(&gr.old_layer, 1);

		if (!gr.old_layer_inited) {
			vm_graphic_delete_layer(gr.old_layer);
			gr.old_layer = -1;
		}
	}
}

VMUINT8* vm_graphic_get_buffer(void) {
	MREngine::AppGraphic& gr = get_current_app_graphic();

	if (gr.old_layer != -1)
		return vm_graphic_get_layer_buffer(gr.old_layer);
	else
		return 0;
}

void vm_graphic_flush_buffer(void) {
	MREngine::AppGraphic& gr = get_current_app_graphic();

	if (gr.old_layer != -1) {
		vm_graphic_flush_layer(&gr.old_layer, 1);
	}
}

void vm_initialize_screen_buffer(void) {
	MREngine::AppGraphic& gr = get_current_app_graphic();

	int w = vm_graphic_get_screen_width();
	int h = vm_graphic_get_screen_height();

	if (gr.old_layer == -1 && !gr.old_layer_inited) {
		gr.old_layer = vm_graphic_create_layer(0, 0, w, h, -1);

		if (gr.old_layer != -1)
			gr.old_layer_inited = true;
	}

	vm_graphic_set_clip(0, 0, w, h);
}

void vm_finalize_screen_buffer(void) {
	MREngine::AppGraphic& gr = get_current_app_graphic();

	if (gr.old_layer_inited) {
		if (gr.old_layer != -1) {
			vm_graphic_delete_layer(gr.old_layer);
			gr.old_layer = -1;
		}

		gr.old_layer_inited = false;
	}
}

void vm_graphic_drawtext(VMINT x, VMINT y, VMWSTR s, VMINT32 length, VMINT color) {
	MREngine::AppGraphic& gr = get_current_app_graphic();

	VMUINT8* disp_buf = 0;

	if (gr.old_layer != -1)
		disp_buf = vm_graphic_get_layer_buffer(gr.old_layer);
	else if(gr.layers.size())
		disp_buf = (VMUINT8*)gr.layers[0].buf;

	vm_graphic_textout(disp_buf, x, y, s, length, color);
}

void vm_dd_initialize_clip_rect(void) {
	int w = vm_graphic_get_screen_width();
	int h = vm_graphic_get_screen_height();

	vm_graphic_set_clip(0, 0, w, h);
}

void vm_dd_set_clip(VMINT x, VMINT y, VMINT width, VMINT height) {
	vm_graphic_set_clip(x, y, x + width - 1, y + height - 1);
}

void vm_dd_reset_clip(void) {
	vm_dd_initialize_clip_rect();
}

VMUINT8* vm_dd_load_image(VMUINT8* img, VMINT img_len) {
	return (VMUINT8*)vm_graphic_load_image_FIX(img, img_len);
}

struct frame_prop* vm_dd_get_img_property(VMUINT8* img, VMUINT8 frame_index) {
	return vm_graphic_get_img_property_FIX(img, frame_index);
}

VMINT vm_dd_get_frame_number(VMUINT8* img) {
	return vm_graphic_get_frame_number_FIX(img);
}

void vm_dd_clean(VMUINT8* buf, VMUINT16 color16) {
	vm_graphic_fill_rect(buf, 0, 0, 1000, 1000, color16, color16);
}