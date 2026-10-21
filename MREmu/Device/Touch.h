#pragma once
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Event.hpp>

class Touch {
public:
	enum touch_source {
		Unknow,
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

	enum d_event_e {
		Began,
		Moved,
		End
	};

	bool touching = false;
	sf::Vector2i last_touch_pos;
	touch_source source;

	sf::Sprite *screen;

	sf::Vector2i to_screen_coords(sf::Vector2i d_pos);
	bool is_on_screen(sf::Vector2i pos);

	void update();
	bool d_event(d_event_e ev, touch_source s, sf::Vector2i d_pos);
	bool sf_event(sf::Event& event);
};