#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <vmio.h>

#define MREMU_KEY_SEND (VM_KEY_QWERTY_MENU + 1)
#define MREMU_KEY_POWER (MREMU_KEY_SEND + 1)

#define MREMU_NEGATIVE_KEY_COUNT VM_KEY_BACK
#define MREMU_FULL_KEY_COUNT (MREMU_KEY_POWER + 1 + MREMU_NEGATIVE_KEY_COUNT)

class KeyboardControl {
public:
	enum key_source {
		Unknow,
		Keyboard,
		Mouse,
		ImGui,
		Touch
	};

	struct pkey_t {
		int key_code;
		sf::Clock tim;
		int8_t key_status = VM_KEY_EVENT_DOWN;
		key_source source = Unknow;

		pkey_t() = default;
		pkey_t(int key_code, key_source source);
	};

	std::vector<pkey_t> pkey;

	void update();

	int find_key(int key_code);

	void press_key(int key_code, key_source source);
	void unpress_key(int key_code);
	void unpress_by_source(key_source source);
};

class Keyboard {
public:
	struct key_t {
		sf::Vector2f v[4];
		int key_code;
	};
	key_t keys_nav[5];

	KeyboardControl kc;

	sf::RenderTexture frontend_layer;

	int x, y, w, h;



	//key_t keys[MREMU_FULL_KEY_COUNT];

	bool keyboard_event(sf::Event& event);

	void update();

	void imgui_keyboard();

	void draw(sf::RenderTarget* rt);

	void draw_press_key(sf::RenderTarget* rt, int key);

	int find_key_by_pos(int px, int py);

	void update_pos_and_size(int x, int y, int w, int h);
};