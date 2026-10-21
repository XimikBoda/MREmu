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

	// Lowest pixel row occupied by the on-screen keypad. Anything below this in
	// the device window is free space (used to place the Options button).
	int m_bottom = 0;

	sf::RenderTexture frontend_layer_left; // numpad
	sf::RenderTexture frontend_layer_right; // dpad
public:

	sf::Sprite sp_left;
	sf::Sprite sp_right;

	sf::Sprite *screen;

	Keyboard(KeyboardControl& kc);

	// The shared key state. The Device owns one KeyboardControl and hands it
	// to both the Keyboard and the Menubar, so a key pressed on one is
	// released on both. Exposed because the MCP host drives the same state
	// when an agent injects a keypress.
	KeyboardControl& control() { return kc; }

	bool event(sf::Event& event);

	void update();

	void imgui_keyboard();

	void draw(sf::RenderTarget* rt);

	void draw_press_key(sf::RenderTarget* rt, int key);

	int find_key_by_pos(int px, int py);

	void update_resize(int w, int h, int menu_h);

	// Bottom edge of the keypad, in device-window coordinates.
	int bottom() const { return m_bottom; }
};