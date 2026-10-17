#pragma once
#include "Keyboard.h"
#include "Touch.h"
#include "../MREngine/Graphic.h"

class Device {
	Keyboard keyboard;
	Touch touch;

	sf::RenderWindow& win;
	MREngine::Graphic& graphic;

	sf::Sprite screen_sp;

	int scale = 1;
public:
	Device(sf::RenderWindow& win, MREngine::Graphic& graphic);

	void update_size();

	void process_events();

	void draw();
	void imgui();
};