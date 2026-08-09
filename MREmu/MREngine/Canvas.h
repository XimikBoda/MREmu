#pragma once
#include <vector>
#include <vmgraph.h>

const char* const CANVAS_MAGIC = "MTKCANVAS"; // Do we have an app that checks for this?

typedef void* VMINT_CANVAS;

VMINT_CANVAS vm_graphic_create_canvas_FIX(VMINT width, VMINT height);
VMINT_CANVAS vm_graphic_create_canvas_cf_FIX(vm_graphic_color_famat cf, VMINT width, VMINT height);
void vm_graphic_release_canvas_FIX(VMINT_CANVAS hcanvas);
void vm_graphic_release_canvas_ex_FIX(VMINT_CANVAS hcanvas);
VMUINT8* vm_graphic_get_canvas_buffer_FIX(VMINT_CANVAS hcanvas);
VMINT vm_graphic_get_canvas_buffer_size_FIX(VMINT_CANVAS hcanvas);
VMUINT8* vm_graphic_get_img_buffer_FIX(VMINT_CANVAS hcanvas, VMUINT8 frame_index);
VMINT vm_graphic_get_frame_number_FIX(VMINT_CANVAS hcanvas);

VM_GDI_RESULT vm_graphic_canvas_set_trans_color_FIX(VMINT_CANVAS hcanvas, VMINT trans_color);

int color_format_size(vm_graphic_color_famat cf);

namespace MREngine {
	#pragma pack (push, 1)
	struct canvas_signature {
		char magic[9];
		uint8_t frame_count = 1;
		uint8_t i_dont_know = 0xFF;
		vm_graphic_color_famat color_format = 0;
	};

	struct canvas_frame_property { // see frame_prop
		uint8_t flag = 0; // has transparency?
		uint16_t left = 0;
		uint16_t top = 0;
		uint16_t width = 0;
		uint16_t height = 0;
		uint16_t delay = 0;
		uint8_t trans_color_index = 0;
		uint32_t trans_color = 0;
		uint32_t offset = 0;
	};
	#pragma pack(pop)
}

MREngine::canvas_signature* find_canvas_signature(VMUINT8* buf);
MREngine::canvas_frame_property* get_canvas_frame_by_ind(MREngine::canvas_signature* cs, int frame_index);