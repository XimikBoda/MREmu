#include "AppManager.h"
#include "Keyboard.h"
#include <vmio.h>
#include <vector>
#include <SFML/Graphics.hpp>

#include "imgui.h"
#include "imgui-SFML.h"

KeyboardControl::pkey_t::pkey_t(int key_code, key_source source) {
	this->key_code = key_code;
	this->source = source;
}

void KeyboardControl::update() {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].tim.getElapsedTime().asMilliseconds() >= 500) {
			pkey[i].tim.restart();
			if (pkey[i].key_status < VM_KEY_EVENT_REPEAT)
				pkey[i].key_status++;
		}
}

int KeyboardControl::find_key(int key_code) {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].key_code == key_code)
			return i;
	return -1;
}

void KeyboardControl::press_key(int key_code, key_source source) {
	int i = find_key(key_code);
	if (i == -1) {
		pkey.push_back(pkey_t(key_code, source));
		add_keyboard_event(VM_KEY_EVENT_DOWN, key_code);
	}
}

void KeyboardControl::unpress_key(int key_code) {
	int i = find_key(key_code);
	if (i != -1) {
		pkey.erase(pkey.begin() + i);
		add_keyboard_event(VM_KEY_EVENT_UP, key_code);
	}
}

void KeyboardControl::unpress_by_source(key_source source) {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].source == source) {
			add_keyboard_event(VM_KEY_EVENT_UP, pkey[i].key_code);
			pkey.erase(pkey.begin() + i);
			--i;
		}
}

struct Keys {
	char name[20] = "";
	int code = 0;
};
const Keys keys_imgui[3 * 7] =
{
	{"Left S",VM_KEY_LEFT_SOFTKEY},
	{"UP",VM_KEY_UP},
	{"Right S",VM_KEY_RIGHT_SOFTKEY},
	{"LEFT",VM_KEY_LEFT},
	{"OK",VM_KEY_OK},
	{"RIGHT",VM_KEY_RIGHT},
	{" ",MREMU_KEY_SEND},
	{"Down",VM_KEY_DOWN},
	{" ",MREMU_KEY_POWER},
	{"1.,",VM_KEY_NUM1},
	{"2abc",VM_KEY_NUM2},
	{"3def",VM_KEY_NUM3},
	{"4ghi",VM_KEY_NUM4},
	{"5jkl",VM_KEY_NUM5},
	{"6mno",VM_KEY_NUM6},
	{"7pqrs",VM_KEY_NUM7},
	{"8tuv",VM_KEY_NUM8},
	{"9wxyz",VM_KEY_NUM9},
	{"*",VM_KEY_STAR},
	{"0",VM_KEY_NUM0},
	{"#",VM_KEY_POUND},
};

const std::map<sf::Keyboard::Key, int> key_to_key = 
{
	{sf::Keyboard::Up, VM_KEY_UP},
	{sf::Keyboard::Down, VM_KEY_DOWN},
	{sf::Keyboard::Left, VM_KEY_LEFT},
	{sf::Keyboard::Right, VM_KEY_RIGHT},
	{sf::Keyboard::Slash, VM_KEY_LEFT_SOFTKEY},
	{sf::Keyboard::RShift, VM_KEY_RIGHT_SOFTKEY},
	{sf::Keyboard::Enter, VM_KEY_OK},
	{sf::Keyboard::Numpad7, VM_KEY_NUM1},
	{sf::Keyboard::Numpad8, VM_KEY_NUM2},
	{sf::Keyboard::Numpad9, VM_KEY_NUM3},
	{sf::Keyboard::Numpad4, VM_KEY_NUM4},
	{sf::Keyboard::Numpad5, VM_KEY_NUM5},
	{sf::Keyboard::Numpad6, VM_KEY_NUM6},
	{sf::Keyboard::Numpad1, VM_KEY_NUM7},
	{sf::Keyboard::Numpad2, VM_KEY_NUM8},
	{sf::Keyboard::Numpad3, VM_KEY_NUM9},
	{sf::Keyboard::Divide, VM_KEY_STAR},
	{sf::Keyboard::Numpad0, VM_KEY_NUM0},
	{sf::Keyboard::Multiply, VM_KEY_POUND},
};

bool Keyboard::keyboard_event(sf::Event &event) {
	if (event.type != sf::Event::KeyPressed && event.type != sf::Event::KeyReleased)
		return false;

	const auto& el = key_to_key.find(event.key.code);
	
	if(el != key_to_key.end())
		if(event.type == sf::Event::KeyPressed)
			kc.press_key(el->second, KeyboardControl::Keyboard);
		else
			kc.unpress_key(el->second);
}

void Keyboard::imgui_keyboard() {
	ImVec2 v = { 60,20 };
	ImGui::Begin("KeyBoard");
	for (int i = 0; i < 3 * 7; ++i) {
		if (i % 3 != 0)
			ImGui::SameLine();

		bool presed = kc.find_key(keys_imgui[i].code) != -1;

		if (presed) {
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(160, 75, 160));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(160, 75, 160));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(160, 75, 160));
		}

		if (ImGui::Button(keys_imgui[i].name, v))
			kc.unpress_key(keys_imgui[i].code);
		if (ImGui::IsItemClicked())
			kc.press_key(keys_imgui[i].code, KeyboardControl::ImGui);

		if(presed)
			ImGui::PopStyleColor(3);
	}
	ImGui::End();
}

float sign(sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3) {
	return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool point_in_triangle(sf::Vector2f pt, sf::Vector2f v1, sf::Vector2f v2, sf::Vector2f v3) {
	float d1, d2, d3;
	bool has_neg, has_pos;

	d1 = sign(pt, v1, v2);
	d2 = sign(pt, v2, v3);
	d3 = sign(pt, v3, v1);

	has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	return !(has_neg && has_pos);
}

bool point_in_quads(sf::Vector2f pt, sf::Vector2f v1, sf::Vector2f v2, sf::Vector2f v3, sf::Vector2f v4) {
	return point_in_triangle(pt, v1, v2, v3) || point_in_triangle(pt, v1, v2, v4);
}

void draw_roundline(sf::RenderTarget* rt, int x, int y, int w, int h, sf::Color c) {
	sf::CircleShape circle(int(h / 2));
	circle.setFillColor(c);
	circle.setPosition(x, y);
	rt->draw(circle);
	circle.setPosition(x + w - h, y);
	rt->draw(circle);

	sf::RectangleShape rect(sf::Vector2f(w - h, h));
	rect.setFillColor(c);
	rect.setPosition(x + h / 2, y);
	rt->draw(rect);
}

void draw_handset(sf::RenderTarget* rt, int x, int y, int w, int h, sf::Color c, sf::Color b) {
	int r = h / 3 / 2;
	int R1 = h - r;
	int R2 = h - r - r * 2;
	sf::CircleShape circle(R1);
	circle.setFillColor(c);
	circle.setPosition(x, y);
	rt->draw(circle);
	circle.setPosition(x + w - R1 * 2, y);
	rt->draw(circle);

	sf::RenderStates states;

	states.blendMode = sf::BlendMode(sf::BlendMode::One, sf::BlendMode::Zero);

	sf::RectangleShape rect(sf::Vector2f(w, h));
	rect.setFillColor(b);
	rect.setPosition(x, y + h - r);
	rt->draw(rect, states);
	rect.setPosition(x + R1, y + r * 2);
	rect.setSize(sf::Vector2f(w - R1 * 2, h - r * 2));
	rt->draw(rect, states);

	rect.setFillColor(c);
	rect.setPosition(x + R1, y);
	rect.setSize(sf::Vector2f(w - R1 * 2, r * 2));
	rt->draw(rect);

	circle.setFillColor(c);
	circle.setRadius(r);
	circle.setPosition(x, y + h - r * 2);
	rt->draw(circle);
	circle.setPosition(x + w - r * 2, y + h - r * 2);
	rt->draw(circle);


	circle.setFillColor(b);
	circle.setRadius(R2);
	circle.setPosition(x + r * 2, y + r * 2);
	rt->draw(circle, states);
	circle.setPosition(x + w - R2 * 2 - r * 2, y + r * 2);
	rt->draw(circle, states);
}

void draw_soft_button(sf::RenderTarget* rt, float x, float y, float w, float h) {
	draw_roundline(rt, x + w / 3, y + (h - h / 10) / 2, w / 3, h / 10, sf::Color::White);
}

void draw_handset_button(sf::RenderTarget* rt, float x, float y, float w, float h, sf::Color c, sf::Color b) {
	draw_handset(rt, x + w / 3, y + (h - h * 3 / 10) / 2, w / 3, h * 3 / 10, c, b);
}

void Keyboard::draw(sf::RenderTarget* rt) {
	for (int i = 0; i < kc.pkey.size(); ++i)
		draw_press_key(rt, kc.pkey[i].key_code);

	sf::Sprite sp(frontend_layer.getTexture());;
	sp.setPosition(x, y);

	rt->draw(sp);
}

void Keyboard::draw_press_key(sf::RenderTarget* rt, int key) {
	sf::Color c(160, 75, 160);

	int x = -1, y = 0;

	if (VM_KEY_NUM1 <= key && key <= VM_KEY_NUM9) {
		x = (key - VM_KEY_NUM1) % 3;
		y = (key - VM_KEY_NUM1) / 3 + 2;
	}
	else
		switch (key) {
		case VM_KEY_LEFT_SOFTKEY:
			x = 0, y = 0;
			break;
		case VM_KEY_RIGHT_SOFTKEY:
			x = 2, y = 0;
			break;
		case MREMU_KEY_SEND:
			x = 0, y = 1;
			break;
		case MREMU_KEY_POWER:
			x = 2, y = 1;
			break;
		case VM_KEY_STAR:
			x = 0, y = 5;
			break;
		case VM_KEY_NUM0:
			x = 1, y = 5;
			break;
		case VM_KEY_POUND:
			x = 2, y = 5;
			break;
		}

	sf::Vertex v[4];

	float kw = (float)w / 3;
	float kh = (float)h / 6;

	if (x != -1) {
		v[0].position = sf::Vector2f(x * kw, y * kh);
		v[1].position = sf::Vector2f((x + 1) * kw, y * kh);
		v[2].position = sf::Vector2f((x + 1) * kw, (y + 1) * kh);
		v[3].position = sf::Vector2f(x * kw, (y + 1) * kh);
	}
	else {
		for (int i = 0; i < 5; ++i)
			if (keys_nav[i].key_code == key) {
				for (int j = 0; j < 4; ++j)
					v[j].position = keys_nav[i].v[j];
				x = 0;
				break;
			}
	}

	if (x != -1) {
		for (int j = 0; j < 4; ++j) {
			v[j].position += sf::Vector2f(this->x, this->y);
			v[j].color = c;
		}
		rt->draw(v, 4, sf::Quads);
	}
}

int Keyboard::find_key_by_pos(int px, int py) {
	float kw = (float)w / 3;
	float kh = (float)h / 6;

	int kpx = px / kw, kpy = py / kh;

	if (kpy >= 2 && kpy < 5)
		return VM_KEY_NUM1 + kpx + (kpy - 2) * 3;
	else if (kpy == 5)
		if (kpx == 0) return VM_KEY_STAR;
		else if (kpx == 1) return VM_KEY_NUM0;
		else return VM_KEY_POUND;
	else if (kpx == 0)
		if (kpy == 0) return VM_KEY_LEFT_SOFTKEY;
		else return MREMU_KEY_SEND;
	else if (kpx == 2)
		if (kpy == 0) return VM_KEY_RIGHT_SOFTKEY;
		else return MREMU_KEY_POWER;
	else {
		for (int i = 0; i < 4; ++i)
			if (point_in_quads(sf::Vector2f(px, py),
				keys_nav[i].v[0], keys_nav[i].v[1],
				keys_nav[i].v[2], keys_nav[i].v[3]))
				return keys_nav[i].key_code;
		return VM_KEY_OK;
	}
}

void Keyboard::update_pos_and_size(int x, int y, int w, int h) {
	this->x = x, this->y = y, this->w = w, this->h = h;

	frontend_layer.create(w, h);
	frontend_layer.clear(sf::Color::Transparent);

	float kw = (float)w / 3;
	float kh = (float)h / 6;

	std::vector<sf::Vertex> lines;

	int it = 0;
	for (int i = 0; i < 6; ++i)
		if (i == 1) {
			lines.push_back(sf::Vertex(sf::Vector2f(0, i * kh)));
			lines.push_back(sf::Vertex(sf::Vector2f(kw, i * kh)));
			lines.push_back(sf::Vertex(sf::Vector2f(kw * 2, i * kh)));
			lines.push_back(sf::Vertex(sf::Vector2f(kw * 3, i * kh)));
		}
		else {
			lines.push_back(sf::Vertex(sf::Vector2f(0, i * kh)));
			lines.push_back(sf::Vertex(sf::Vector2f(kw * 3, i * kh)));
		}

	for (int i = 0; i < 2; ++i) {
		lines.push_back(sf::Vertex(sf::Vector2f(kw * (i + 1), 0)));
		lines.push_back(sf::Vertex(sf::Vector2f(kw * (i + 1), h)));
	}

	float ik = kh * 5 / 8;
	float ocords[4][2] = { {0, 0}, {kw, 0}, {kw, kh * 2}, {0, kh * 2} };
	float icords[4][2] = { {ik, ik}, {-ik, ik}, {-ik, -ik}, {ik, -ik} };

	for (int i = 0; i < 4; ++i) {
		ocords[i][0] += kw;
		icords[i][0] += ocords[i][0];
		icords[i][1] += ocords[i][1];
	}

	for (int i = 0; i < 4; ++i) {
		int ni = i < 3 ? i + 1 : 0;
		lines.push_back(sf::Vertex(sf::Vector2f(icords[i][0], icords[i][1])));
		lines.push_back(sf::Vertex(sf::Vector2f(icords[ni][0], icords[ni][1])));
		lines.push_back(sf::Vertex(sf::Vector2f(ocords[i][0], ocords[i][1])));
		lines.push_back(sf::Vertex(sf::Vector2f(icords[i][0], icords[i][1])));

		keys_nav[i].v[0] = sf::Vector2f(ocords[i][0], ocords[i][1]);
		keys_nav[i].v[1] = sf::Vector2f(ocords[ni][0], ocords[ni][1]);
		keys_nav[i].v[2] = sf::Vector2f(icords[ni][0], icords[ni][1]);
		keys_nav[i].v[3] = sf::Vector2f(icords[i][0], icords[i][1]);

		keys_nav[4].v[i] = sf::Vector2f(icords[i][0], icords[i][1]);
	}

	keys_nav[0].key_code = VM_KEY_UP;
	keys_nav[1].key_code = VM_KEY_RIGHT;
	keys_nav[2].key_code = VM_KEY_DOWN;
	keys_nav[3].key_code = VM_KEY_LEFT;
	keys_nav[4].key_code = VM_KEY_OK;

	for (int i = 0; i < lines.size(); ++i)
		lines[i].color = sf::Color::White;

	draw_handset_button(&frontend_layer, 0, kh, kw, kh, sf::Color::Green, sf::Color::Transparent);
	draw_handset_button(&frontend_layer, kw * 2, kh, kw, kh, sf::Color::Red, sf::Color::Transparent);
	draw_soft_button(&frontend_layer, 0, 0, kw, kh);
	draw_soft_button(&frontend_layer, kw * 2, 0, kw, kh);

	frontend_layer.draw(lines.data(), lines.size(), sf::Lines);
	frontend_layer.display();
}