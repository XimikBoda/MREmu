#include "Device.h"

Device::Device(sf::RenderWindow& win_device, MREngine::Graphic& graphic_) 
	: win(win_device), graphic(graphic_) 
{
	update_size();
}

void Device::process_events() {
	sf::Event event;
	while (win.pollEvent(event)) {
		keyboard.event(event);
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
	screen_sp = sf::Sprite(graphic.screen_tex);

	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;

	int scale_x = win.getSize().x / graphic.width;
	int scale_y = win.getSize().y / (graphic.height + graphic.height / 2);

	scale = std::min(scale_x, scale_y);
	if (scale < 1)
		scale = 1;

	screen_sp.setScale(scale, scale);
	screen_sp.setPosition((win.getSize().x - graphic.width * scale) / 2, 0);

	keyboard.update_resize(win.getSize().x, win.getSize().y);
}

void Device::draw() {
	keyboard.draw(&win);

	screen_sp.setTexture(graphic.screen_tex, true);
	win.draw(screen_sp);
}

void Device::imgui() {
	keyboard.imgui_keyboard();
}