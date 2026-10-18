#include "Menubar.h"
#include "../App.h"
#include "../MREngine/CharSet.h"
#include <SFML/Graphics.hpp>

App* get_active_app();

sf::Texture u16text_to_texture(std::u16string str, sf::Color c);

int Menubar::getHeight() {
	return base_size;
}

void Menubar::draw(sf::RenderTarget* rt) {
	auto app = get_active_app();
	if (app) {
		auto& app_name = app->app_name;

		if (last_app_name != app_name) {
			last_app_name = app_name;
			auto u16str = utf8_to_utf16(app_name);
			name_tex = u16text_to_texture(u16str, sf::Color::White);
		}

		sf::Sprite sp(name_tex);
		sp.setScale(font_scale, font_scale);

		rt->draw(sp);
	}

	rt->draw(buttons_sp);
}

static void add_line(std::vector<sf::Vertex>& v, float x1, float y1, float x2, float y2) {
	v.push_back(sf::Vertex(sf::Vector2f(x1, y1)));
	v.push_back(sf::Vertex(sf::Vector2f(x2, y2)));
}

static const char16_t* keys_marks[4] = {
	u"\u2795", u"\u2796", u"\u26F6", u"\u2630"
};

void Menubar::update_resize(int win_w, int win_h) {
	int screen_x = screen->getPosition().x;
	int screen_y = screen->getPosition().y;
	int screen_w = screen->getScale().x * screen->getTextureRect().width;
	int screen_h = screen->getScale().y * screen->getTextureRect().height;

	float scale = screen->getScale().x;
	float scaled_size = base_size * scale;

	font_scale = std::max<int>(scaled_size / 16, 1);

	{
		int w = scaled_size * 4 + 1, h = scaled_size;

		buttons_frontend_layer.create(w, h);
		buttons_frontend_layer.clear(sf::Color::Transparent);

		buttons_sp = sf::Sprite(buttons_frontend_layer.getTexture());
		buttons_sp.setPosition(win_w - w, 0);

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