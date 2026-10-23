#include "Graphic.h"
#include "MREngine/Canvas.h"
#include "AppContext.h"
#include "Memory.h"
#include "Path.h"
#include <vmgraph.h>
#include <vmpromng.h>
#include <format>
#include <cstring>

#ifdef DEBUG_WINDOW
#include <imgui.h>
#include <imgui-SFML.h>
#endif


static void buf_to_texture(void* buf, int w, int h, sf::Texture& tex) {
	static std::vector<unsigned char> pix_data;

	if (pix_data.size() < w * h * 4)
		pix_data.resize(w * h * 4);

	unsigned short* buf16 = (unsigned short*)buf;
	for (int i = 0; i < w * h; ++i) {
		pix_data[i * 4 + 0] = VM_COLOR_GET_RED(buf16[i]);
		pix_data[i * 4 + 1] = VM_COLOR_GET_GREEN(buf16[i]);
		pix_data[i * 4 + 2] = VM_COLOR_GET_BLUE(buf16[i]);
		pix_data[i * 4 + 3] = 0xFF;
	}

	tex.update(pix_data.data(), w, h, 0, 0);
}

Graphic* Graphic::ptr = nullptr;

Graphic::Graphic()
{
	if (ptr != nullptr)
		throw std::runtime_error("Graphic already exists!");

	ptr = this;

	screen_tex.create(width, height);
	screen.resize(width * height);
	{
		int image_size = width * height * 2;
		void* canvas_buf = Memory::shared_malloc(VM_CANVAS_DATA_OFFSET + image_size);

		if (canvas_buf == 0) abort();

		MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

		*cs = MREngine::canvas_signature();
		memcpy(cs->magic, CANVAS_MAGIC, 9);

		MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

		*cfp = MREngine::canvas_frame_property();
		cfp->width = width;
		cfp->height = height;
		base_buf1 = (cfp + 1);
	}
	{
		int image_size = width * height * 2;
		void* canvas_buf = Memory::shared_malloc(VM_CANVAS_DATA_OFFSET + image_size);

		if (canvas_buf == 0) abort();

		MREngine::canvas_signature* cs = (MREngine::canvas_signature*)canvas_buf;

		*cs = MREngine::canvas_signature();
		memcpy(cs->magic, CANVAS_MAGIC, 9);

		MREngine::canvas_frame_property* cfp = (MREngine::canvas_frame_property*)(cs + 1);

		*cfp = MREngine::canvas_frame_property();
		cfp->width = width;
		cfp->height = height;
		base_buf2 = (cfp + 1);
	}
}

void Graphic::update_screen() {
	if (screen_changed)
		buf_to_texture(screen.data(), width, height, screen_tex);
}

#ifdef DEBUG_WINDOW
void Graphic::imgui_screen() {
	if (ImGui::Begin("Screen")) {
		ImGui::Image(screen_tex);
		if (ImGui::Button("Paint"))
			vm_graphic_flush_screen();
	}
	ImGui::End();
}
#endif

void Graphic::screenshot() {
	vm_time_t time;
	vm_get_time(&time);

	auto name = AppContext::get_executing_app_path();

	int ms = vm_get_tick_count();

	std::string file_name = std::format("{:04}_{:02}_{:02}_{:02}_{:02}_{:02}_{:03}_{}.png",
		time.year, time.mon, time.day, time.hour, time.min, time.sec, ms % 1000, (char*)name.c_str());

	auto im = screen_tex.copyToImage();
	im.saveToFile((Path::get_screenshots_path() / file_name).string());
}

Graphic::~Graphic() {
	ptr = nullptr;
}

Graphic* Graphic::getPtr() {
	return ptr;
}