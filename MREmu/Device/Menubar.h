#pragma once
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <string>

class Menubar {
	std::u8string last_app_name;
	int base_size = 20;

	int font_scale = 1;

	sf::Texture name_tex;

	sf::RenderTexture buttons_frontend_layer;
	sf::Sprite buttons_sp;
public:
	sf::Sprite* screen;

	int getHeight();

	void draw(sf::RenderTarget* rt);

	void update_resize(int win_w, int win_h);
};