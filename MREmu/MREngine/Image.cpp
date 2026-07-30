#include "Graphic.h"
#include "Image.h"
#include "Blt.h"
#include "Canvas.h"
#include "../Memory.h"
#include <SFML/Graphics/Image.hpp>
#include <vmgraph.h>

extern write_color_t write_color_funcs[4];

VMINT_CANVAS vm_graphic_load_image_FIX(VMUINT8* img, VMINT img_len) {
	return vm_graphic_load_image_cf_FIX(VM_GRAPHIC_COLOR_FORMAT_16, img, img_len);
}

VMINT_CANVAS vm_graphic_load_image_cf_FIX(vm_graphic_color_famat cf, VMUINT8* img_data, VMINT img_len) {
	sf::Image im;
	if (!im.loadFromMemory(img_data, img_len))
		return 0;

	if (cf >= VM_GRAPHIC_COLOR_FORMAT_END)
		return 0;

	int image_size = im.getSize().x * im.getSize().y * color_format_size(cf);
	void* canvas_buf = vm_malloc(VM_CANVAS_DATA_OFFSET + image_size);

	if (canvas_buf == 0)
		return 0;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

	*cs = MREngine::canvas_signature();
	memcpy(cs->magic, CANVAS_MAGIC, 9);
	cs->color_format = cf;

	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

	int t = sizeof(MREngine::canvas_frame_property);

	*cfp = MREngine::canvas_frame_property();
	cfp->width = im.getSize().x;
	cfp->height = im.getSize().y;

	cfp->offset = image_size;

	void* image_buf = (void*)(cfp + 1);
	ColorRGBA* rgb_buf = (ColorRGBA*)im.getPixelsPtr();

	auto write_func = write_color_funcs[cf];

	for (int i = 0; i < image_size / 2; ++i) 
		write_func(image_buf, i, -1, rgb_buf[i]);

	std::lock_guard lock(get_current_app_graphic().canvases_list_mutex);
	get_current_app_graphic().canvases_list.push_back({ canvas_buf, sf::Texture() });

	return (VMINT_CANVAS)canvas_buf;
}

VMINT_CANVAS vm_graphic_load_image_resized_FIX(VMUINT8* img, VMINT img_len, VMINT width, VMINT height) {
	return vm_graphic_load_image_resized_cf_FIX(VM_GRAPHIC_COLOR_FORMAT_16, img, img_len, width, height);
}

VMINT_CANVAS vm_graphic_load_image_resized_cf_FIX(vm_graphic_color_famat cf, VMUINT8* img, VMINT img_len, VMINT width, VMINT height) {
	sf::Image im;
	if (!im.loadFromMemory(img, img_len))
		return 0;

	if (cf >= VM_GRAPHIC_COLOR_FORMAT_END)
		return 0;

	int image_size = width * height * color_format_size(cf);
	void* canvas_buf = vm_malloc(VM_CANVAS_DATA_OFFSET + image_size);

	if (canvas_buf == 0)
		return 0;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

	*cs = MREngine::canvas_signature();
	memcpy(cs->magic, CANVAS_MAGIC, 9);
	cs->color_format = cf;

	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

	int t = sizeof(MREngine::canvas_frame_property);

	*cfp = MREngine::canvas_frame_property();
	cfp->width = width;
	cfp->height = height;

	cfp->offset = image_size;

	void* image_buf = (void*)(cfp + 1);
	ColorRGBA* rgb_buf = (ColorRGBA*)im.getPixelsPtr();
	int im_width = im.getSize().x;
	int im_height = im.getSize().y;

	auto write_func = write_color_funcs[cf];

	for (int y = 0; y < height; ++y) {
		int ny = y * im_height / height;

		for (int x = 0; x < width; ++x) {
			int nx = x * im_width / width;

			write_func(image_buf, x + y * width, -1, rgb_buf[nx + ny * im_width]);
		}
	}

	std::lock_guard lock(get_current_app_graphic().canvases_list_mutex);
	get_current_app_graphic().canvases_list.push_back({ canvas_buf, sf::Texture() });

	return (VMINT_CANVAS)canvas_buf;
}

struct frame_prop* vm_graphic_get_img_property_FIX(VMINT_CANVAS hcanvas, VMUINT8 frame_index) {
	if (hcanvas == 0)
		return 0;

	static struct frame_prop* info = (frame_prop*)Memory::shared_malloc(sizeof(frame_prop));

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)(hcanvas);
	if (memcmp(cs->magic, CANVAS_MAGIC, 9))
		return 0;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs + 1);

	//TODO frame index

	info->flag = cfp_dst->flag;
	info->left = cfp_dst->left;
	info->top = cfp_dst->top;
	info->width = cfp_dst->width;
	info->height = cfp_dst->height;
	info->delay_time = cfp_dst->delay * 10; //todo check this
	info->trans_color_index = cfp_dst->trans_color_index;
	info->trans_color = cfp_dst->trans_color;
	info->offset = cfp_dst->offset;

	return info;
}

VM_GDI_RESULT vm_graphic_get_img_property_ex(VMUINT8* img_data, VMINT img_len, vm_graphic_imgprop* img_prop) {
	sf::Image im;
	if (!img_prop || !im.loadFromMemory(img_data, img_len))
		return VM_GDI_FAILED;

	img_prop->width = im.getSize().x;
	img_prop->height = im.getSize().y;

	return VM_GDI_SUCCEED;
}