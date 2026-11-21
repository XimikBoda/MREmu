#pragma once
#include "Container.h"
#include <memory>
#include <vector>

namespace GUI::Container {
	class AlignContainer : public Container {
	public:
		std::shared_ptr<Base> ui;

		bool fill = false;

		enum class Horisontal {
			Left, Center, Right
		} horisontal;

		enum class Vertical {
			Top, Center, Bottom
		} vertical;

		AlignContainer(std::shared_ptr<Base> ui, Horisontal hr = Horisontal::Center, Vertical vr = Vertical::Center) {
			this->ui = ui;
			this->horisontal = hr;
			this->vertical = vr;
		}

		void update_size() override {
			ui->update_size();
			w = ui->w, h = ui->h;
		}

		void update_rb(RenderBox rb) override {
			this->rb = rb;

			switch (horisontal) {
			case Horisontal::Left:
				break;
			case Horisontal::Center:
				rb.x += (rb.w - ui->w) / 2;
				break;
			case Horisontal::Right:
				rb.x += rb.w - ui->w;
				break;
			}

			switch (vertical) {
			case Vertical::Top:
				break;
			case Vertical::Center:
				rb.y += (rb.h - ui->h) / 2;
				break;
			case Vertical::Bottom:
				rb.y += rb.h - ui->h;
				break;
			}

			rb.w = ui->w;
			rb.h = ui->h;

			ui->update_rb(rb);
		};

		void draw(const Layer& layer, bool focused) override {
			if (focused && fill)
				draw_focused(layer);
			ui->draw(layer, focused);
		}

		void key_event(int event, int keycode) override {
			ui->key_event(event, keycode);
		}

		void pen_event(const Touch& touch) override {
			ui->pen_event(touch);
		}

		std::shared_ptr<Base> get_focused() {
			if (ui)
				return ui->get_focused();
			return nullptr;
		}
	};
}