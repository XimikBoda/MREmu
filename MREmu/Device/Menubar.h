#pragma once
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Event.hpp>
#include <string>

#include "KeyboardControl.h"

class Menubar {
	std::u8string last_app_name;
	int base_size = 20;

	float scale = 1;
	int font_scale = 1;
	int screen_corner = 0;

	sf::Texture name_tex;

	sf::RenderTexture buttons_frontend_layer;
	sf::Sprite buttons_sp;

	KeyboardControl& kc;

	int find_key_by_pos(int px, int py);
	void draw_press_key(sf::RenderTarget* rt, int key);
public:
	sf::Sprite* screen = 0;

	Menubar(KeyboardControl& kc);

	int getHeight();

	void draw(sf::RenderTarget* rt);

	bool event(sf::Event& event);

	void update_resize(int win_w, int win_h);
};