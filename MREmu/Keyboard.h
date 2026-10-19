#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <vmio.h>

#ifdef ANDROID
#include "jni/Vibration.h"
#endif

#define MREMU_KEY_SEND (VM_KEY_QWERTY_MENU + 1)
#define MREMU_KEY_POWER (MREMU_KEY_SEND + 1)
#define MREMU_KEY_NONE (MREMU_KEY_POWER + 1)

#define MREMU_NEGATIVE_KEY_COUNT VM_KEY_BACK
#define MREMU_FULL_KEY_COUNT (MREMU_KEY_POWER + 1 + MREMU_NEGATIVE_KEY_COUNT)

class KeyboardControl {
public:
	enum key_source {
		Unknow,
		Keyboard,
		Mouse,
		ImGui,
        Touch0,
        Touch1,
        Touch2,
        Touch3,
        Touch4,
        Touch5,
        Touch6,
        Touch7,
        Touch8,
        Touch9
	};

	struct pkey_t {
		int key_code;
		sf::Clock tim;
		int8_t key_status = VM_KEY_EVENT_DOWN;
		key_source source = Unknow;

		// Auto-repeat bookkeeping. The first repeat fires after 500 ms; each
		// subsequent repeat fires sooner, so a held key accelerates.
		bool repeat_started = false;
		int repeat_delay_ms = 300;   // delay between repeats after the initial 500 ms
		static constexpr int REPEAT_DELAY_MIN = 50;
		static constexpr int REPEAT_DELAY_STEP = 50;

		pkey_t() = default;
		pkey_t(int key_code, key_source source);
	};

	std::vector<pkey_t> pkey;

#ifdef ANDROID
    Vibration vibration;
#endif

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

	KeyboardControl kc; // I don't remember why I separate this

	// Lowest pixel row occupied by the on-screen keyboard. Anything below this in the
	// device window is free space (used to place the Options button).
	int m_bottom = 0;

	sf::RenderTexture frontend_layer_left; // numpad
	sf::RenderTexture frontend_layer_right; // dpad

	sf::Sprite sp_left;
	sf::Sprite sp_right;

	sf::Sprite *screen;

	bool event(sf::Event& event);

	void update();

	void imgui_keyboard();

	void draw(sf::RenderTarget* rt);

	void draw_press_key(sf::RenderTarget* rt, int key);

	int find_key_by_pos(int px, int py);

	void update_resize(int w, int h);

	// Bottom edge of the keyboard, in device-window coordinates.
	int bottom() const { return m_bottom; }
};