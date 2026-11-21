#pragma once
#include "Container.h"
#include <memory>
#include <vector>

namespace GUI::Container {
	class MainContainer : public Container {
	public:
		std::shared_ptr<Base> ui;
		std::shared_ptr<Base> ui_menu;

		const int soft_bar_height = 30;
		const uint16_t soft_keys_color = VM_COLOR_888_TO_565(20, 20, 20);

		GUI::SoftKeySet f_soft_key_set;

		struct AlingText {
			std::shared_ptr<GUI::View::Text> text;
			std::shared_ptr<GUI::Container::AlignContainer> ui;

			bool focused = false;
			bool pen_pressed = false;

			using Vertical = GUI::Container::AlignContainer::Vertical;
			using Horisontal = GUI::Container::AlignContainer::Horisontal;
			AlingText(std::u16string str = u"",
				Horisontal hor = Horisontal::Center,
				Vertical ver = Vertical::Center)
			{
				text = std::make_shared<GUI::View::Text>(str);
				ui = std::make_shared<GUI::Container::AlignContainer>(text, hor, ver);
				ui->fill = true;
			}
		};

		AlingText ui_soft[SoftKeySet::Count] = {
			AlingText(u"", GUI::Container::AlignContainer::Horisontal::Left),
			AlingText(u"", GUI::Container::AlignContainer::Horisontal::Center),
			AlingText(u"", GUI::Container::AlignContainer::Horisontal::Right)
		};

		void update_size() override {
			ui->update_size();
			if(ui_menu) ui_menu->update_size();
			w = ui->w, h = ui->h;
		}

		void update_rb(RenderBox rb) override {
			this->rb = rb;
			rb.h -= soft_bar_height;
			ui->update_rb(rb);
			if (ui_menu) ui_menu->update_rb(rb);

			update_soft_keys();
		};

		std::u16string get_soft_key_text(std::optional<SoftKey>& sk) {
			if (sk.has_value())
				return sk->text;
			else
				return u"";
		}

		void update_soft_keys() {
			auto a_ui = ui_menu ? ui_menu : ui;
			auto f_ui = a_ui->get_focused();

			if (f_ui)
				f_soft_key_set = f_ui->get_soft_keys();
			else
				f_soft_key_set = {};

			auto rb = this->rb;

			rb.y = this->rb.y + this->rb.h - soft_bar_height;
			rb.h = soft_bar_height;

			rb.w = this->rb.w / 3;

			for (int i = 0; i < SoftKeySet::Count; ++i) {
				ui_soft[i].text->str = get_soft_key_text(f_soft_key_set.keys[i]);
				ui_soft[i].ui->update_size();

				rb.x = rb.w * i;
				ui_soft[i].ui->update_rb(rb);
			}
		}

		void draw(const Layer& layer, bool focused) override {
			ui->draw(layer, focused);
			if (ui_menu) ui_menu->draw(layer, focused);

			int soft_bar_y = rb.y + rb.h - soft_bar_height;
			int soft_button_w = rb.w / 3;

			vm_graphic_fill_rect(layer.buf, rb.x, soft_bar_y,
				rb.w, soft_bar_y, soft_keys_color, soft_keys_color);

			for (int i = 0; i < SoftKeySet::Count; ++i)
				ui_soft[i].ui->draw(layer, ui_soft[i].focused);

			vm_graphic_line(layer.buf, rb.x, soft_bar_y, 
				rb.x + rb.w, soft_bar_y, 0xFFFF);

			vm_graphic_line(layer.buf, 
				rb.x + soft_button_w, soft_bar_y,
				rb.x + soft_button_w, rb.h, 0xFFFF);

			vm_graphic_line(layer.buf,
				rb.x + soft_button_w * 2, soft_bar_y,
				rb.x + soft_button_w * 2, rb.h, 0xFFFF);
		}

		void call_soft_func(std::optional<SoftKey>& sk) {
			if (sk.has_value() && sk->func)
				sk->func(ui_menu);
		}

		void key_event(int event, int keycode) override {
			auto a_ui = ui_menu ? ui_menu : ui;
			SoftKeySet::Keys keys = SoftKeySet::Count;

			if (keycode == VM_KEY_LEFT_SOFTKEY)
				keys = SoftKeySet::Left;
			else if (keycode == VM_KEY_OK)
				keys = SoftKeySet::Ok;
			else if (keycode == VM_KEY_RIGHT_SOFTKEY)
				keys = SoftKeySet::Right;

			if (event == VM_KEY_EVENT_DOWN) {
				if (keys != SoftKeySet::Count)
					ui_soft[keys].focused = true;
			}
			else if (event == VM_KEY_EVENT_UP) {
				if (keys != SoftKeySet::Count) {
					ui_soft[keys].focused = false;
					call_soft_func(f_soft_key_set.keys[keys]);
				}
				else
					if (a_ui)
						a_ui->key_event(event, keycode);
			}

			update_soft_keys();
		}

		void pen_event(const Touch& touch, AlingText &soft_button, std::optional<SoftKey>& sk) {
			if (!touch.in_renderbox(soft_button.ui->rb)) {
				soft_button.focused = false;

				if (touch.event == VM_PEN_EVENT_RELEASE)
					soft_button.pen_pressed = false;

				return;
			}

			if (touch.event == VM_PEN_EVENT_TAP)
				soft_button.pen_pressed = true;

			if (touch.event == VM_PEN_EVENT_RELEASE) {
				call_soft_func(sk);
				soft_button.pen_pressed = false;
			}

			soft_button.focused = soft_button.pen_pressed;
		}

		void pen_event(const Touch& touch) override {
			auto a_ui = ui_menu ? ui_menu : ui;

			if(touch.in_renderbox(a_ui->rb))
				a_ui->pen_event(touch);

			for (int i = 0; i < SoftKeySet::Count; ++i)
				pen_event(touch, ui_soft[i], f_soft_key_set.keys[i]);

			update_soft_keys();
		}

		std::shared_ptr<Base> get_focused() {
			auto a_ui = ui_menu ? ui_menu : ui;

			if (a_ui)
				return a_ui->get_focused();
			return nullptr;
		}
	};
}