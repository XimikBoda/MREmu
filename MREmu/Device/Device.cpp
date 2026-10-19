#include "Device.h"

Device::Device(sf::RenderWindow& win_device, MREngine::Graphic& graphic_) 
	: win(win_device), graphic(graphic_), keyboard(keyboardControl), menubar(keyboardControl), screen_sp(graphic.screen_tex)
{
	update_size();
}

void Device::process_events() {
	while (const std::optional event = win.pollEvent()) {
		keyboard.event(*event);
		menubar.event(*event);
		touch.sf_event(*event);

		if (event->is<sf::Event::Closed>())
			win.close();
		else if (const auto* resied = event->getIf<sf::Event::Resized>()) {
			win.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, { (float)resied->size.x, (float)resied->size.y })));
			update_size();
		}
	}
}

void Device::update_size() {
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

	screen_sp.setScale({ scale, scale });
	screen_sp.setPosition({ (win.getSize().x - graphic.width * scale) / 2, menubar_height * scale });

	keyboard.update_resize(win.getSize().x, win.getSize().y, menubar.getHeight() * scale);
	menubar.update_resize(win.getSize().x, win.getSize().y);
}

void Device::draw() {
	keyboard.draw(&win);
	menubar.draw(&win);

	screen_sp.setTexture(graphic.screen_tex, true);
	win.draw(screen_sp);
}

void Device::imgui() {
	keyboard.imgui_keyboard();
}