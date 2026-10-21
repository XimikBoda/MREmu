#include "Device.h"

Device::Device(sf::RenderWindow& win_device, MREngine::Graphic& graphic_,
	KeyboardControl& keyboardControl)
	: kc(keyboardControl), keyboard(keyboardControl), menubar(keyboardControl),
	  win(win_device), graphic(graphic_)
{
	update_size();
}

void Device::update() {
	kc.update();
}

void Device::event(sf::Event& event) {
	keyboard.event(event);
	menubar.event(event);
	touch.sf_event(event);
}

int Device::menubar_height() const {
	return show_menubar ? menubar.getHeight() : 0;
}

void Device::update_size() {
	screen_sp = sf::Sprite(graphic.screen_tex);

	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;
	menubar.screen = &screen_sp;

	// The menubar strip sits above the emulated screen, so the screen is
	// offset down by its height rather than by nothing.
	int menubar_height = this->menubar_height();

	screen_sp.setScale(scale, scale);
	screen_sp.setPosition(0.f, (float)(menubar_height * scale));

	keyboard.update_resize(win.getSize().x, win.getSize().y, menubar_height * scale);
	menubar.update_resize(win.getSize().x, win.getSize().y);
}

void Device::draw() {
	if (show_menubar)
		menubar.draw(&win);
	keyboard.draw(&win);

	screen_sp.setTexture(graphic.screen_tex, true);
	win.draw(screen_sp);
}

void Device::imgui() {
#ifdef DEBUG_WINDOW
	keyboard.imgui_keyboard();
#endif
}
