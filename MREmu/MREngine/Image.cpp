#include "Graphic.h"
#include "Image.h"
#include "Blt.h"
#include "Canvas.h"
#include "../Memory.h"
#include <SFML/Graphics/Image.hpp>
#include <vmgraph.h>
#include <cstring>
#include <vector>

#define GIF_MAX_WIDTH (16000)
#define GIF_IMPLEMENTATION
#define GIF_OUTPUT_FORMAT GIF_OUTPUT_RGBA8888
#include "gif.h"

extern write_color_t write_color_funcs[4];

struct gif_frame {
	int img_w, img_h;
	int min_x, min_y; 
	int max_x, max_y;
	std::vector<uint8_t> ind;
	std::vector<ColorRGBA> palette;
	int transparent; 
	bool has_transparency;
	bool inited = false;
	int delay_ms;
	int ret = 0;
};

static gif_frame gif_get_next_frame(GIF_Context* ctx) {
	gif_frame frame = {};

	{
		int w, h;
		gif_get_info(ctx, &w, &h);

		frame.img_w = w, frame.img_h = h;
		frame.min_x = w, frame.min_y = h;
		frame.max_x = 0, frame.max_y = 0;
	}

	auto begin_callback = [](void* user, int w, int h) {};

	auto blit_callback = [](void* user, int x, int y, int w, int h,
		const uint8_t* idx, int stride,
		const uint8_t* palette, int num_colors,
		int transparent, int has_transparency) {
			gif_frame* f = (gif_frame*)user;

			if (!f->inited) {
				f->palette.resize(num_colors);
				for (int i = 0; i < num_colors; ++i)
					f->palette[i] = { palette[3 * i + 0], palette[3 * i + 1], palette[3 * i + 2], 0xFF };

				f->ind.resize(f->img_w * f->img_h, transparent);
				f->transparent = transparent;
				f->has_transparency = has_transparency;
				f->inited = true;
			}

			if (f->min_x > x)
				f->min_x = x;
			if (f->min_y > y)
				f->min_y = y;
			if (f->max_x < x + w)
				f->max_x = x + w;
			if (f->max_y < y + h)
				f->max_y = y + h;

			for (int iy = 0; iy < h; ++iy)
				for (int ix = 0; ix < w; ++ix)
					f->ind[ix + x + (iy + y) * (f->img_w)] = idx[ix + iy * stride];
		};

	auto end_callback = [](void* user, int delay_ms) {
		gif_frame* f = (gif_frame*)user;
		f->delay_ms = delay_ms;
		};

	GIF_Renderer renderer = {
		.user = &frame,
		.begin = begin_callback,
		.blit_indexed = blit_callback,
		.end = end_callback
	};

	int delay_ms;
	frame.ret = gif_next_frame_render(ctx, &renderer, &delay_ms);

	return frame;
}

struct frame_t {
	int img_w, img_h;
	int x, y, w, h;
	std::vector<ColorRGBA> buf;
	ColorRGBA transparent_color;
	int transparent_ind;
	bool has_transparency;
	int delay_ms;
};

std::vector<frame_t> load_image_gif(const uint8_t* data, size_t size) {
	GIF_Context ctx;
	std::vector<frame_t> frames;

	std::vector<uint8_t> scratch(GIF_SCRATCH_BUFFER_REQUIRED_SIZE);

	int result = gif_init(&ctx, data, size, scratch.data(), scratch.size());
	if (result != GIF_SUCCESS) {
		return {};
	}

	int width, height;
	gif_get_info(&ctx, &width, &height);

	while (1) {
		auto g_f = gif_get_next_frame(&ctx);
		ctx.loop_count = 0;

		if (g_f.ret == GIF_ERROR_NO_FRAME || g_f.ret == GIF_ERROR_EARLY_EOF)
			break;
		if (g_f.ret != GIF_SUCCESS) {
			gif_close(&ctx);
			return {};
		}

		frame_t frame;
		frame.img_w = g_f.img_w, frame.img_h = g_f.img_h;
		frame.x = g_f.min_x, frame.y = g_f.min_y;
		frame.w = g_f.max_x - g_f.min_x, frame.h = g_f.max_y - g_f.min_y;
		frame.transparent_ind = g_f.transparent;
		frame.has_transparency = g_f.has_transparency;
		frame.delay_ms = g_f.delay_ms;

		if (g_f.has_transparency) {
			auto to_rgb565 = [](const ColorRGBA& c) -> uint16_t {
				return VM_COLOR_888_TO_565(c.r, c.g, c.b);
				};

			uint16_t safe_trans_565 = 0xF81F;
			bool collision = true;

			while (collision) {
				collision = false;
				for (int i = 0; i < (int)g_f.palette.size(); ++i) {
					if (i != g_f.transparent && to_rgb565(g_f.palette[i]) == safe_trans_565) {
						collision = true;
						safe_trans_565++;
						break;
					}
				}
			}

			g_f.palette[g_f.transparent] = {
				(uint8_t)VM_COLOR_GET_RED(safe_trans_565),
				(uint8_t)VM_COLOR_GET_GREEN(safe_trans_565),
				(uint8_t)VM_COLOR_GET_BLUE(safe_trans_565),
				0x00
			};

			frame.transparent_color = g_f.palette[g_f.transparent];
		}
		else
			frame.transparent_color = { 0xFF, 0xFF, 0xFF, 0xFF };


		int buf_size = g_f.img_w * g_f.img_h;
		frame.buf.resize(buf_size);
		for (int i = 0; i < buf_size; ++i)
			frame.buf[i] = g_f.palette[g_f.ind[i]];

		frames.push_back(frame);
	}
	gif_close(&ctx);
	return frames;
}

std::vector<frame_t> load_image(const uint8_t* data, size_t size) {
	if (data && size > 3 && memcmp(data, "GIF", 3) == 0) {
		auto frames = load_image_gif(data, size);
		if (frames.size())
			return frames;
	}

	sf::Image im;
	if (!im.loadFromMemory(data, size))
		return {};

	frame_t frame;
	frame.img_w = im.getSize().x, frame.img_h = im.getSize().y;
	frame.x = 0, frame.y = 0;
	frame.w = frame.img_w, frame.h = frame.img_h;
	frame.transparent_ind = 0;
	frame.transparent_color = { 0xFF, 0xFF, 0xFF, 0xFF };
	frame.has_transparency = 0;
	frame.delay_ms = 0;

	int image_size = im.getSize().x * im.getSize().y;
	ColorRGBA* rgb_buf = (ColorRGBA*)im.getPixelsPtr();

	frame.buf = std::vector<ColorRGBA>(rgb_buf, rgb_buf + image_size);

	return {frame};
}

VMINT_CANVAS vm_graphic_load_image_FIX(VMUINT8* img, VMINT img_len) {
	return vm_graphic_load_image_cf_FIX(VM_GRAPHIC_COLOR_FORMAT_16, img, img_len);
}

VMINT_CANVAS vm_graphic_load_image_cf_FIX(vm_graphic_color_famat cf, VMUINT8* img_data, VMINT img_len) {
	if (!img_data || !img_len || cf >= VM_GRAPHIC_COLOR_FORMAT_END)
		return 0;

	auto frames = load_image(img_data, img_len);
	if (!frames.size())
		return 0;

	int bytes_for_pixel = color_format_size(cf);
	const int canvas_signature_size = sizeof(MREngine::canvas_signature);
	const int canvas_frame_property_size = sizeof(MREngine::canvas_frame_property);

	int full_canvas_size = canvas_signature_size + canvas_frame_property_size * frames.size();
	for (int i = 0; i < frames.size(); ++i)
		if (i == 0)
			full_canvas_size += frames[i].img_w * frames[i].img_h * bytes_for_pixel;
		else
			full_canvas_size += frames[i].w * frames[i].h * bytes_for_pixel;

	uint8_t* canvas_buf = (uint8_t*)vm_malloc(full_canvas_size);
	if (canvas_buf == 0)
		return 0;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

	*cs = MREngine::canvas_signature();
	memcpy(cs->magic, CANVAS_MAGIC, 9);
	cs->color_format = cf;
	cs->frame_count = frames.size();

	auto write_func = write_color_funcs[cf];

	size_t offset = canvas_signature_size;

	for (int i = 0; i < frames.size(); ++i) {
		MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(canvas_buf + offset); 
		uint8_t* image_buf = canvas_buf + offset + canvas_frame_property_size;
		frame_t& frame = frames[i];

		*cfp = MREngine::canvas_frame_property();
		cfp->flag = frame.has_transparency;
		cfp->delay = frame.delay_ms;
		cfp->trans_color_index = frame.transparent_ind;
		if (frame.has_transparency)
			write_func(&cfp->trans_color, 0, -1, frame.transparent_color);
		else
			cfp->trans_color = -1;

		if (i == 0) {
			cfp->left = 0, cfp->top = 0;
			cfp->width = frame.img_w, cfp->height = frame.img_h;

			for (int j = 0; j < cfp->width * cfp->height; ++j)
				write_func(image_buf, j, -1, frame.buf[j]);
		}
		else {
			cfp->left = frame.x, cfp->top = frame.y;
			cfp->width = frame.w, cfp->height = frame.h;

			for (int iy = 0; iy < cfp->height; ++iy)
				for (int ix = 0; ix < cfp->width; ++ix)
					write_func(image_buf, ix + iy * cfp->width, -1, frame.buf[frame.x + ix + (frame.y + iy) * frame.img_w]);
		}

		cfp->offset = cfp->width * cfp->height * bytes_for_pixel;
		offset += canvas_frame_property_size + cfp->offset;
	}

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
	cfp->trans_color = -1;

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