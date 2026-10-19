#pragma once
#include "Keyboard.h"
#include "Menubar.h"
#include "Touch.h"
#include "../MREngine/Graphic.h"

class Device {
	Keyboard keyboard;
	Menubar menubar;
	Touch touch;
	KeyboardControl keyboardControl;

	sf::RenderWindow& win;
	MREngine::Graphic& graphic;

	sf::Sprite screen_sp;

	float scale = 1;
public:
	Device(sf::RenderWindow& win, MREngine::Graphic& graphic);

	void update_size();

	void process_events();

	void draw();
	void imgui();
};