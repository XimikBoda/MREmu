#pragma once
#include <vmio.h>
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

		// Auto-repeat bookkeeping. The first repeat fires after
		// INITIAL_REPEAT_DELAY_MS; each subsequent repeat fires sooner, so a held
		// key accelerates.
		//
		// The floor is REPEAT_DELAY_MIN rather than something lower on purpose. The
		// app selector fires a vibration tick on every selection change, and a
		// rotating-mass motor needs that long to spin up and settle between
		// pulses. Repeating faster than that cancels the mass mid spin-up and the
		// buzz comes out as a stutter instead of a tick.
		bool repeat_started = false;
		int repeat_delay_ms = 300;   // delay between repeats, after the initial hold
		static constexpr int INITIAL_REPEAT_DELAY_MS = 500;
		static constexpr int REPEAT_DELAY_MIN = 120;
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
