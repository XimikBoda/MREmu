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

VM_GDI_RESULT vm_graphic_canvas_set_trans_color_FIX(VMINT_CANVAS hcanvas, VMINT trans_color);

MREngine::canvas_signature* find_canvas_signature(VMUINT8* buf);