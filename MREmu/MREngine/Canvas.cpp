#include "Graphic.h"
#include "image.h"
#include "../Memory.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <SFML/Graphics/Image.hpp>
#include <vmgraph.h>
#include <vmpromng.h>

void canvas_to_texture(std::pair<void*, sf::Texture>& p) {
	static std::vector<unsigned char> pix_data;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)p.first;

	if (memcmp(cs->magic, CANVAS_MAGIC, 9))
		return;

	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

	int w = cfp->width, h = cfp->height;

	if (!(w * h))
		return;

	if (pix_data.size() < w * h * 4)
		pix_data.resize(w * h * 4);

	unsigned short trans_color = cfp->trans_color;
	bool flag = cfp->flag;

	unsigned short* buf16 = (unsigned short*)(cfp + 1);
	for (int i = 0; i < w * h; ++i) {
		if (flag && trans_color == buf16[i])
			pix_data[i * 4 + 3] = 0x00;
		else
			pix_data[i * 4 + 3] = 0xFF;
		pix_data[i * 4 + 0] = VM_COLOR_GET_RED(buf16[i]);
		pix_data[i * 4 + 1] = VM_COLOR_GET_GREEN(buf16[i]);
		pix_data[i * 4 + 2] = VM_COLOR_GET_BLUE(buf16[i]);
	}
	sf::Image im;
	im.create(w, h, pix_data.data());

	p.second.loadFromImage(im);
}

void MREngine::AppGraphic::imgui_canvases() {
	std::lock_guard lock(canvases_list_mutex);
	if (ImGui::Begin("Canvases")) {
		for (int i = 0; i < canvases_list.size(); ++i) {
			auto& el = canvases_list[i];
			canvas_to_texture(el);

			MREngine::canvas_signature* cs = (MREngine::canvas_signature*)el.first;

			if (memcmp(cs->magic, CANVAS_MAGIC, 9)) {
				ImGui::Text("Wrong canvas magic");
				break;
			}

			MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

			int w = cfp->width, h = cfp->height;

			ImGui::Text("Id: %d, x: %d, y: %d, w: %d, h: %d",
				i, cfp->left, cfp->top, cfp->width, cfp->height);
			ImGui::Image(el.second);
		}
	}
	ImGui::End();
}

MREngine::canvas_signature* find_canvas_signature(VMUINT8* buf) {
	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)buf;
	if (memcmp(cs->magic, CANVAS_MAGIC, 9) == 0)
		return cs;
	cs = (MREngine::canvas_signature*)(buf - VM_CANVAS_DATA_OFFSET);
	if (memcmp(cs->magic, CANVAS_MAGIC, 9) == 0)
		return cs;
	return 0;
}

//MRE API


VMINT_CANVAS vm_graphic_create_canvas_FIX(VMINT width, VMINT height) {
	int image_size = width * height * 2;
	void* canvas_buf = vm_malloc(VM_CANVAS_DATA_OFFSET + image_size);

	if (canvas_buf == 0)
		return 0;

	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

	*cs = MREngine::canvas_signature();
	memcpy(cs->magic, CANVAS_MAGIC, 9);

	MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

	*cfp = MREngine::canvas_frame_property();
	cfp->width = width;
	cfp->height = height;

	cfp->offset = image_size;

	std::lock_guard lock(get_current_app_graphic().canvases_list_mutex);
	get_current_app_graphic().canvases_list.push_back({ canvas_buf, sf::Texture() });

	return (VMINT_CANVAS)canvas_buf;
}

VMINT_CANVAS vm_graphic_create_canvas_cf_FIX(vm_graphic_color_famat cf, VMINT width, VMINT height) {
	if (cf != VM_GRAPHIC_COLOR_FORMAT_16)
		return (VMINT_CANVAS)0;

	return vm_graphic_create_canvas_FIX(width, height);
}

void vm_graphic_release_canvas_FIX(VMINT_CANVAS hcanvas) {
	if (!hcanvas)
		return;

	void* hcanvas_adr = hcanvas;
	auto& canvases_list = get_current_app_graphic().canvases_list;

	std::lock_guard lock(get_current_app_graphic().canvases_list_mutex);

	for (int i = 0; i < canvases_list.size(); ++i)
		if (canvases_list[i].first == hcanvas_adr) {
			canvases_list.erase(canvases_list.begin() + i);
			break;
		}

	vm_free(hcanvas_adr);
}

void vm_graphic_release_canvas_ex_FIX(VMINT_CANVAS hcanvas) {
	vm_graphic_release_canvas_FIX(hcanvas); // TODO
}

VMUINT8* vm_graphic_get_canvas_buffer_FIX(VMINT_CANVAS hcanvas) {
	return (VMUINT8*)hcanvas;
}

VM_GDI_RESULT vm_graphic_canvas_set_trans_color_FIX(VMINT_CANVAS hcanvas, VMINT trans_color) {
	MREngine::canvas_signature* cs = (MREngine::canvas_signature*)(hcanvas);
	if (!hcanvas || memcmp(cs->magic, CANVAS_MAGIC, 9))
		return VM_GDI_FAILED;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs + 1);

	//TODO frame index
	cfp_dst->flag = 1;
	cfp_dst->trans_color = trans_color;
	return VM_GDI_SUCCEED;
}