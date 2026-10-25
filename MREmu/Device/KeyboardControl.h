#pragma once
#include <vmio.h>
#include <vector>
#include <cstdint>
#include <SFML/System/Clock.hpp>

#ifdef ANDROID
#include "../jni/Vibration.h"
#endif

#define MREMU_KEY_SEND			(VM_KEY_QWERTY_MENU + 1)
#define MREMU_KEY_POWER			(MREMU_KEY_SEND + 1)
#define MREMU_KEY_PRINTSCREEN	(MREMU_KEY_POWER + 1)
#define MREMU_KEY_NONE			(MREMU_KEY_PRINTSCREEN + 1)

#define MREMU_NEGATIVE_KEY_COUNT (-VM_KEY_BACK)
#define MREMU_FULL_KEY_COUNT	(MREMU_KEY_PRINTSCREEN + 1 + MREMU_NEGATIVE_KEY_COUNT)

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
