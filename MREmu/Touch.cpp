#include "Touch.h"
#include <SFML/Graphics/Texture.hpp>
#include <vmio.h>

void add_pen_event(int event, int x, int y);

void Touch::update() {}

sf::Vector2i Touch::to_screen_coords(sf::Vector2i d_pos) {
	sf::Vector2f touch_pos = { (float)d_pos.x, (float)d_pos.y };

	auto pos = screen->getPosition();
	auto scale = screen->getScale();

	touch_pos -= pos;
	touch_pos.x /= scale.x;
	touch_pos.y /= scale.y;

	return { (int)touch_pos.x, (int)touch_pos.y };
}

bool Touch::is_on_screen(sf::Vector2i pos) {
	auto size = screen->getTexture()->getSize();
	return 0 <= pos.x && pos.x < size.x &&
		0 <= pos.y && pos.y < size.y;
}

bool Touch::d_event(d_event_e ev, touch_source s, sf::Vector2i d_pos) {
	if (!touching) {
		if (ev == Began) {
			auto s_pos = to_screen_coords(d_pos);
			if (is_on_screen(s_pos)) {
				touching = true;
				last_touch_pos = s_pos;
				source = s;
				add_pen_event(VM_PEN_EVENT_TAP, s_pos.x, s_pos.y);
				return true;
			}
		}
	}
	else if (s == source) {
		if (ev == End) {
			auto s_pos = to_screen_coords(d_pos);
			if (is_on_screen(s_pos))
				last_touch_pos = s_pos;
			touching = false;
			add_pen_event(VM_PEN_EVENT_RELEASE, last_touch_pos.x, last_touch_pos.y);
			return true;
		}
		else if (ev == Moved) {
			auto s_pos = to_screen_coords(d_pos);
			if (is_on_screen(s_pos)) {
				last_touch_pos = s_pos;
				add_pen_event(VM_PEN_EVENT_MOVE, s_pos.x, s_pos.y);
				return true;
			}
		}
	}

	return false;
}

bool Touch::sf_event(sf::Event& event) {
	switch (event.type) {
	case sf::Event::MouseButtonPressed:
		if (event.mouseButton.button == sf::Mouse::Button::Left)
			return d_event(Began, Mouse, { event.mouseButton.x, event.mouseButton.y });
		break;
	case sf::Event::MouseMoved:
		return d_event(Moved, Mouse, { event.mouseMove.x, event.mouseMove.y });
		break;
	case sf::Event::MouseButtonReleased:
		if (event.mouseButton.button == sf::Mouse::Button::Left)
			return d_event(End, Mouse, { event.mouseButton.x, event.mouseButton.y });
		break;

	case sf::Event::TouchBegan:
		return d_event(Began, (touch_source)(Touch0 + event.touch.finger), { event.touch.x, event.touch.y });
		break;
	case sf::Event::TouchMoved:
		return d_event(Moved, (touch_source)(Touch0 + event.touch.finger), { event.touch.x, event.touch.y });
		break;
	case sf::Event::TouchEnded:
		return d_event(End, (touch_source)(Touch0 + event.touch.finger), { event.touch.x, event.touch.y });
		break;
	}
	return false;
}