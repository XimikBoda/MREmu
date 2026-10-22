// Canvas stores address in int, 
// which will cause a problem in the 64-bit build of MREmu.
// So there fixed versions with new canvas type
#pragma once 
#include "vmgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

// Canvas

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

// Images

VMINT_CANVAS vm_graphic_load_image_FIX(VMUINT8* img, VMINT img_len);
VMINT_CANVAS vm_graphic_load_image_cf_FIX(vm_graphic_color_famat cf, VMUINT8* img, VMINT img_len);

VMINT_CANVAS vm_graphic_load_image_resized_FIX(VMUINT8* img_data, VMINT img_len, VMINT width, VMINT height);
VMINT_CANVAS vm_graphic_load_image_resized_cf_FIX(vm_graphic_color_famat cf, VMUINT8* img_data, VMINT img_len, VMINT width, VMINT height);

struct frame_prop* vm_graphic_get_img_property_FIX(VMINT_CANVAS hcanvas, VMUINT8 frame_index);

#ifdef __cplusplus
}
#endif