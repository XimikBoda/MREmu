#pragma once
#include "Keyboard.h"
#include "KeyboardControl.h"
#include "Menubar.h"
#include "Touch.h"
#include "../MREngine/Graphic.h"

/**
 * The device front end: the emulated screen, the on-screen keypad, and the
 * menubar strip above it.
 *
 * The Device owns the widgets and the geometry, but not the window policy.
 * The caller decides how big the window is and when to redraw, because the
 * desktop build resizes the window to the scaled content while the MCP
 * window and the Android build keep a fixed size and scale the sprite inside
 * it. Feeding the Device a scale and a window is enough for both.
 *
 * The key state lives in a KeyboardControl the caller owns, so that the
 * keypresses an MCP agent injects and the ones a human types go through the
 * same state as the ones that arrive as touch events.
 */
class Device {
public:
	Device(sf::RenderWindow& win, MREngine::Graphic& graphic, KeyboardControl& kc);

	// Per-frame housekeeping: the key auto-repeat timer.
	void update();

	// Recompute the widget geometry for the current window size and scale.
	// The caller owns the window size; this only lays the widgets out.
	void update_size();

	// Feed one event to the widgets.
	void event(sf::Event& event);

	void draw();
	void imgui();

	// Scale of the emulated screen inside the window.
	void set_scale(float s) { scale = s; }
	float get_scale() const { return scale; }

	// The menubar strip can be switched off for windows that show the emulated
	// screen on its own, such as the MCP status window.
	void set_show_menubar(bool on) { show_menubar = on; }
	bool get_show_menubar() const { return show_menubar; }

	// Height of the menubar strip, in unscaled pixels. The caller needs it to
	// work out how tall the window has to be. Zero when the menubar is hidden.
	int menubar_height() const;

	// Bottom edge of the on-screen keypad, in window coordinates. Anything
	// below it is free space.
	int keyboard_bottom() const { return keyboard.bottom(); }

	// The emulated screen, as the sprite it is drawn from.
	sf::Sprite& screen_sprite() { return screen_sp; }

	// The touch handler, so a hard reset can clear the in-progress touch.
	Touch& touch_ref() { return touch; }

	// The shared key state.
	KeyboardControl& control() { return kc; }
	Keyboard& keyboard_ref() { return keyboard; }

private:
	KeyboardControl& kc;

	Keyboard keyboard;
	Menubar menubar;
	Touch touch;

	sf::RenderWindow& win;
	MREngine::Graphic& graphic;

	sf::Sprite screen_sp;

	float scale = 1;
	bool show_menubar = true;
};
