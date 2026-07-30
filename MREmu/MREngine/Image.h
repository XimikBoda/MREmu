#pragma once
#include <vector>
#include <vmgraph.h>
#include "Canvas.h"

VMINT_CANVAS vm_graphic_load_image_FIX(VMUINT8* img, VMINT img_len);
VMINT_CANVAS vm_graphic_load_image_cf_FIX(vm_graphic_color_famat cf, VMUINT8* img, VMINT img_len);

VMINT_CANVAS vm_graphic_load_image_resized_FIX(VMUINT8* img_data, VMINT img_len, VMINT width, VMINT height);
VMINT_CANVAS vm_graphic_load_image_resized_cf_FIX(vm_graphic_color_famat cf, VMUINT8* img_data, VMINT img_len, VMINT width, VMINT height);

struct frame_prop* vm_graphic_get_img_property_FIX(VMINT_CANVAS hcanvas, VMUINT8 frame_index);
