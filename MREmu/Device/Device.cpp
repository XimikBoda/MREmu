#include "Device.h"
#include "../Graphic.h"

Device::Device(sf::RenderWindow& win_device) 
	: win(win_device), keyboard(keyboardControl), menubar(keyboardControl)
{
	update_size();
}

void Device::process_events() {
	sf::Event event;
	while (win.pollEvent(event)) {
		keyboard.event(event);
		menubar.event(event);
		touch.sf_event(event);
		switch (event.type) {
		case sf::Event::Closed:
			win.close();
			break;
		case sf::Event::Resized:
			win.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
			update_size();
			break;
		}
	}
}

void Device::update_size() {
	auto& graphic = *Graphic::getPtr();
	screen_sp = sf::Sprite(graphic.screen_tex);

	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;
	menubar.screen = &screen_sp;

	int menubar_height = menubar.getHeight();

	float scale_x = win.getSize().x / (float)graphic.width;
	float scale_y = win.getSize().y / (float)(graphic.height + menubar_height);

	float scale = scale_x;

	if (scale_y < scale_x) {
		if(win.getSize().x > graphic.width * scale_y * 2)
			scale_y = win.getSize().y / (float)graphic.height, menubar_height = 0;
		scale = scale_y;
	}

	if (scale < 0)
		scale = 0;

	screen_sp.setScale(scale, scale);
	screen_sp.setPosition((win.getSize().x - graphic.width * scale) / 2, menubar_height * scale);

	keyboard.update_resize(win.getSize().x, win.getSize().y, menubar.getHeight() * scale);
	menubar.update_resize(win.getSize().x, win.getSize().y);
}

void Device::draw() {
	keyboard.draw(&win);
	menubar.draw(&win);

	screen_sp.setTexture(Graphic::getPtr()->screen_tex, true);
	win.draw(screen_sp);
}

#ifdef DEBUG_WINDOW
void Device::imgui() {
	keyboard.imgui_keyboard();
}
#endif