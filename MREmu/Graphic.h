#pragma once
#include <vector>
#include <mutex>
#include <vmgraph.h>
#include <SFML/Graphics/Texture.hpp>

class Graphic {
	static Graphic* ptr;
public: //TODO move to private
	int width = 240, height = 320;
	bool screen_changed = true;

	sf::Texture screen_tex;

	std::vector<uint16_t> screen;
	void* base_buf1 = 0; // wrong, need to change to canvas (or not)
	void* base_buf2 = 0;

	Graphic();

	void update_screen();

	void imgui_screen();

	void screenshot();

	~Graphic();

	static Graphic* getPtr();
};
