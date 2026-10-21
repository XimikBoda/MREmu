#include "Menubar.h"
#include "../AppContext.h"
#include "../MREngine/CharSet.h"
#include "../jni/ScreenCorner.h"
#include <SFML/Graphics.hpp>

sf::Texture u16text_to_texture(std::u16string str, sf::Color c);

Menubar::Menubar(KeyboardControl& keyboardControl) : kc(keyboardControl){
	screen_corner = ScreenCorner().getRadius();
}

int Menubar::getHeight() {
	return base_size;
}

void Menubar::draw(sf::RenderTarget* rt) {
	{
		auto app_name = AppContext::get_foreground_app_name();

		if (last_app_name != app_name) {
			last_app_name = app_name;
			auto u16str = utf8_to_utf16(last_app_name);
			name_tex = u16text_to_texture(u16str, sf::Color::White);
		}

		sf::Sprite sp(name_tex);
		sp.setScale(font_scale, font_scale);
		sp.setPosition(scale + screen_corner, scale);

		rt->draw(sp);
	}

	for (int i = 0; i < kc.pkey.size(); ++i)
		draw_press_key(rt, kc.pkey[i].key_code);

	rt->draw(buttons_sp);
}

void Menubar::draw_press_key(sf::RenderTarget* rt, int key) {
	sf::Color c(160, 75, 160);

	int x = -1;
	float kw = base_size * scale;

	switch (key) {
	case VM_KEY_VOL_UP:
		x = 0;
		break;
	case VM_KEY_VOL_DOWN:
		x = 1;
		break;
	case MREMU_KEY_PRINTSCREEN:
		x = 2;
		break;
	default:
		return;
	}

	sf::RectangleShape rs;
	rs.setSize({ kw, kw });
	rs.setPosition(buttons_sp.getPosition());
	rs.setFillColor(c);
	rs.move(kw * x, 0);

	rt->draw(rs);
}

static bool in_box(int x, int y, int bx, int by, int bw, int bh) {
	return x >= bx && y >= by && x < bx + bw && y < by + bh;
}

static bool in_box(int x, int y, sf::Sprite sp) {
	return in_box(x, y, sp.getPosition().x, sp.getPosition().y, sp.getTextureRect().width, sp.getTextureRect().height);
}

const int key_map[4] = {
	VM_KEY_VOL_UP, VM_KEY_VOL_DOWN, MREMU_KEY_PRINTSCREEN, MREMU_KEY_NONE
};

int Menubar::find_key_by_pos(int px, int py) {
	if (in_box(px, py, buttons_sp)) {
		int x = buttons_sp.getPosition().x;
		int y = buttons_sp.getPosition().y;
		int w = buttons_sp.getTextureRect().width;
		int h = buttons_sp.getTextureRect().height;

		float kw = (float)w / 4;

		int kpx = (px - x) / kw;

		return key_map[kpx];
	}

	return MREMU_KEY_NONE;
}

bool Menubar::event(sf::Event& event) {
	switch (event.type) {
	case sf::Event::MouseButtonPressed:
		if (event.mouseButton.button == sf::Mouse::Button::Left)
			kc.press_key(find_key_by_pos(event.mouseButton.x, event.mouseButton.y), kc.Mouse);
		return true;
	case sf::Event::MouseButtonReleased:
		if (event.mouseButton.button == sf::Mouse::Button::Left)
			kc.unpress_by_source(kc.Mouse);
		return true;
	case sf::Event::TouchBegan:
		kc.press_key(find_key_by_pos(event.touch.x, event.touch.y),
			(KeyboardControl::key_source)(kc.Touch0 + event.touch.finger));
		return true;
	case sf::Event::TouchEnded:
		kc.unpress_by_source((KeyboardControl::key_source)(kc.Touch0 + event.touch.finger));
		return true;
	}
	return false;
}

static void add_line(std::vector<sf::Vertex>& v, float x1, float y1, float x2, float y2) {
	v.push_back(sf::Vertex(sf::Vector2f(x1, y1)));
	v.push_back(sf::Vertex(sf::Vector2f(x2, y2)));
}

static const char16_t* keys_marks[4] = {
	u"\u2795", u"\u2796", u"\u26F6", u"\u2630"
};

void Menubar::update_resize(int win_w, int win_h) {
	screen_corner = ScreenCorner().getRadius();

	int screen_x = screen->getPosition().x;
	int screen_y = screen->getPosition().y;
	int screen_w = screen->getScale().x * screen->getTextureRect().width;
	int screen_h = screen->getScale().y * screen->getTextureRect().height;

	scale = screen->getScale().x;
	float scaled_size = base_size * scale;

	font_scale = std::max<int>(scaled_size / 16, 1);

	{
		int w = scaled_size * 4 + 1, h = scaled_size;

		buttons_frontend_layer.create(w, h);
		buttons_frontend_layer.clear(sf::Color::Transparent);

		buttons_sp = sf::Sprite(buttons_frontend_layer.getTexture());
		buttons_sp.setPosition(win_w - w - screen_corner, 0);

		std::vector<sf::Vertex> lines;

		for (int ix = 0; ix < 5; ++ix) {
			int x = ix * scaled_size + 1;
			add_line(lines, x, 0, x, h);
		}

		add_line(lines, 0, 0, w, 0);
		add_line(lines, 0, h - 1, w, h - 1);

		for (int ix = 0; ix < 4; ++ix) {
			auto tex = u16text_to_texture(keys_marks[ix], sf::Color::White);
			sf::Sprite sp(tex);
			sp.setOrigin(sp.getTextureRect().width / 2, sp.getTextureRect().height / 2);
			sp.setPosition(ix * scaled_size + scaled_size / 2.f, scaled_size / 2.f - 1);
			sp.setScale(font_scale, font_scale);
			buttons_frontend_layer.draw(sp);
		}

		for (int i = 0; i < lines.size(); ++i)
			lines[i].color = sf::Color::White;

		buttons_frontend_layer.draw(lines.data(), lines.size(), sf::Lines);
		buttons_frontend_layer.display();
	}
}