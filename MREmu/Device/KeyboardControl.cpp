#include "KeyboardControl.h"
#include "../MREngine/IO.h"
#include "../AppContext.h"
#include <vmio.h>
#include <vector>

static void add_keyboard_event(int event, int keycode) {
	AppContext::push_event(Event::input{ Event::keyboard{ event, keycode } });
}

KeyboardControl::pkey_t::pkey_t(int key_code, key_source source) {
	this->key_code = key_code;
	this->source = source;
}

void KeyboardControl::update() {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].tim.getElapsedTime().asMilliseconds() >= 500) {
			pkey[i].tim.restart();
			if (pkey[i].key_status < VM_KEY_EVENT_REPEAT)
				pkey[i].key_status++;
		}
}

int KeyboardControl::find_key(int key_code) {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].key_code == key_code)
			return i;
	return -1;
}

void KeyboardControl::press_key(int key_code, key_source source) {
	if (key_code == MREMU_KEY_NONE)
		return;

	int i = find_key(key_code);
	if (i == -1) {
#ifdef ANDROID
		if (Touch0 <= source && source <= Touch9) {
			vibration.vibrate(sf::milliseconds(50));
		}
#endif
		//if (key_code == MREMU_KEY_PRINTSCREEN){ //TODO
		//	auto app = get_active_app();
		//	if (app) {
		//		graphic->screenshot(app->app_name);
		//	}
		//}

		pkey.push_back(pkey_t(key_code, source));
		if(key_code <= VM_KEY_QWERTY_MENU)
			add_keyboard_event(VM_KEY_EVENT_DOWN, key_code);
	}
}

void KeyboardControl::unpress_key(int key_code) {
	int i = find_key(key_code);
	if (i != -1) {
		pkey.erase(pkey.begin() + i);
		if (key_code <= VM_KEY_QWERTY_MENU)
			add_keyboard_event(VM_KEY_EVENT_UP, key_code);
	}
}

void KeyboardControl::unpress_by_source(key_source source) {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].source == source) {
#ifdef ANDROID
			if (Touch0 <= source && source <= Touch9) {
				vibration.vibrate(sf::milliseconds(40));
			}
#endif

			if (pkey[i].key_code <= VM_KEY_QWERTY_MENU)
				add_keyboard_event(VM_KEY_EVENT_UP, pkey[i].key_code);
			pkey.erase(pkey.begin() + i);
			--i;
		}
}
