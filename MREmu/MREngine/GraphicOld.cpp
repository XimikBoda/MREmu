#include "Graphic.h"
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