#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <vmio.h>

#include "KeyboardControl.h"

class Keyboard {
	struct key_t {
		sf::Vector2f v[4];
		int key_code;
	};
	key_t keys_nav[5];

	KeyboardControl& kc;

	sf::RenderTexture frontend_layer_left; // numpad
	sf::RenderTexture frontend_layer_right; // dpad
public:

	sf::Sprite sp_left;
	sf::Sprite sp_right;

	sf::Sprite *screen;

	Keyboard(KeyboardControl& kc);

	bool event(sf::Event& event);

	void update();

	void imgui_keyboard();

	void draw(sf::RenderTarget* rt);

	void draw_press_key(sf::RenderTarget* rt, int key);

	int find_key_by_pos(int px, int py);

	void update_resize(int w, int h, int menu_h);
};