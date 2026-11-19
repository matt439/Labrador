#include "samples/components/touch_demo_state.h"

#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglei.h"
#include "engine/render/resource_factory.h"
#include "engine/ui/pointer_target.h"

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

using namespace labrador;

namespace components
{
	namespace
	{
		const RectangleF layout(0, 0, 1280, 720);
		const RectangleF stick_region(140, 240, 300, 300);
		const RectangleF a_region(720, 280, 160, 160);
		const RectangleF b_region(930, 280, 160, 160);
		const RectangleF back_region(1000, 60, 200, 56);
		const std::string font_name = "jetbrains_mono_bold_16";
		const Colour background(12, 18, 27);
		const Colour inactive(44, 54, 70);
		const Colour a_colour(55, 215, 235);
		const Colour b_colour(255, 171, 67);
		// Mouse preview is exclusive with real touch snapshots. A new touch
		// cancels mouse ownership before any engine id can be routed.
		constexpr std::uint32_t mouse_contact = std::numeric_limits<std::uint32_t>::max();

		void rectangle(DrawList& list, TextureHandle white, const RectangleF& bounds, const Colour& colour)
		{
			list.draw_sprite(white, RectangleI(0, 0, 1, 1), bounds, colour,
				0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
		}
	}

	TouchDemoState::TouchDemoState(Application* app, StatusChanged on_status) :
		app_(app), on_status_(std::move(on_status)), stick_(stick_region),
		a_(a_region), b_(b_region), back_(back_region)
	{
	}

	void TouchDemoState::init()
	{
		this->white_ = this->app_->render_resources()->resolve_texture(white_texture_name);
		this->font_ = this->app_->render_resources()->resolve_sprite_font(font_name);
		this->refresh_view();
		this->input_epoch_ = this->app_->mouse()->state().button_epoch;
		this->wait_for_clear_ = !this->app_->touch()->points().empty() ||
			this->app_->mouse()->held(MouseButton::left) || !this->app_->keyboard()->focused();
		this->publish_status();
	}

	bool TouchDemoState::refresh_view()
	{
		const Viewport viewport(RectangleF(Vector2F::zero,
			this->app_->resolution_manager()->resolution_vec()));
		const Camera camera = Camera::frame(layout, viewport);
		const bool changed = this->view_ready_ && (viewport != this->viewport_ || camera != this->camera_);
		this->viewport_ = viewport;
		this->camera_ = camera;
		this->view_ready_ = true;
		return changed;
	}

	Vector2F TouchDemoState::world_point(const Vector2I& client) const
	{
		return this->camera_.world_position(client, this->viewport_);
	}

	bool TouchDemoState::begin_contact(std::uint32_t contact, const Vector2F& position)
	{
		// This page owns precedence: the later-drawn controls come first.
		// A busy first hit refuses the new finger;
		// it does not fall through to a control underneath or adopt it later.
		const std::array<PointerTarget, 4> targets = {
			PointerTarget{ this->back_.region(), true }, PointerTarget{ this->b_.region(), true },
			PointerTarget{ this->a_.region(), true }, PointerTarget{ this->stick_.region(), true }
		};
		const std::optional<std::size_t> hit = hit_pointer_target(targets, position);
		if (!hit) return false;
		switch (*hit)
		{
		case 0: return this->back_.begin(contact, position);
		case 1: return this->b_.begin(contact, position);
		case 2: return this->a_.begin(contact, position);
		case 3: return this->stick_.begin(contact, position);
		default: return false;
		}
	}

	bool TouchDemoState::update_touch()
	{
		const Touch& touch = *this->app_->touch();
		// Touch releases retain the previous snapshot, not a final-up position.
		// Back therefore completes from its last sampled inside position.
		const bool back = this->back_.held() && touch.released(this->back_.contact());
		for (TouchButton* button : { &this->a_, &this->b_, &this->back_ })
		{
			const std::uint32_t contact = button->contact();
			if (contact == 0) continue;
			const TouchPoint* point = touch.state().find(contact);
			if (point) button->move(contact, this->world_point(point->position));
			else if (touch.released(contact)) button->end(contact);
			else button->cancel();
		}
		const std::uint32_t contact = this->stick_.contact();
		if (contact != 0)
		{
			const TouchPoint* point = touch.state().find(contact);
			if (point) this->stick_.move(contact, this->world_point(point->position));
			else if (touch.released(contact)) this->stick_.end(contact);
			else this->stick_.cancel();
		}
		if (back) return true;
		for (const TouchPoint& point : touch.points())
		{
			if (touch.pressed(point.id)) this->begin_contact(point.id, this->world_point(point.position));
		}
		return false;
	}

	bool TouchDemoState::update_mouse()
	{
		const Mouse& mouse = *this->app_->mouse();
		if (this->mouse_active_ &&
			(!mouse.focused() || mouse.state().button_epoch != this->mouse_epoch_))
		{
			this->cancel_controls();
			return false;
		}
		const Vector2F position = this->world_point(mouse.position());
		if (!this->mouse_active_ && mouse.pressed(MouseButton::left))
		{
			this->mouse_active_ = this->begin_contact(mouse_contact, position);
			this->mouse_epoch_ = mouse.state().button_epoch;
		}
		if (!this->mouse_active_) return false;
		this->stick_.move(mouse_contact, position);
		for (TouchButton* button : { &this->a_, &this->b_, &this->back_ }) button->move(mouse_contact, position);
		if (mouse.held(MouseButton::left)) return false;
		const bool back = mouse.released(MouseButton::left) && this->back_.held();
		this->stick_.end(mouse_contact);
		for (TouchButton* button : { &this->a_, &this->b_, &this->back_ }) button->end(mouse_contact);
		this->mouse_active_ = false;
		return back;
	}

	void TouchDemoState::update(float /*dt*/)
	{
		this->update_cursor();
		const Keyboard& keyboard = *this->app_->keyboard();
		if (keyboard.pressed(Key::escape) || this->app_->gamepads()->pressed(0, GamepadButton::b))
		{
			this->leave();
			return;
		}
		const Touch& touch = *this->app_->touch();
		const Mouse& mouse = *this->app_->mouse();
		// The mouse epoch retains a canvas focus/capture loss even when it
		// returns before a poll; touch focus intentionally follows page presence.
		const bool context_changed = mouse.state().button_epoch != this->input_epoch_;
		this->input_epoch_ = mouse.state().button_epoch;
		if (this->refresh_view() || context_changed || !touch.focused() || !keyboard.focused())
		{
			this->cancel_controls();
			this->wait_for_clear_ = true;
			this->publish_status();
			return;
		}
		if (this->wait_for_clear_)
		{
			if (touch.points().empty() && !mouse.held(MouseButton::left))
			{
				this->wait_for_clear_ = false;
				this->mouse_quiet_ticks_ = touch.previous_state().count > 0 ? 1 : 0;
			}
			return;
		}
		bool back = false;
		if (touch.state().count > 0 || touch.previous_state().count > 0)
		{
			if (this->mouse_active_) this->cancel_controls();
			this->mouse_quiet_ticks_ = 1;
			back = this->update_touch();
		}
		else if (this->mouse_quiet_ticks_ > 0)
		{
			--this->mouse_quiet_ticks_;
		}
		else
		{
			back = this->update_mouse();
		}
		if (back) this->leave();
		else this->publish_status();
	}

	TouchDemoReading TouchDemoState::reading() const
	{
		return { this->a_.held(), this->b_.held(), this->stick_.value() };
	}

	void TouchDemoState::publish_status()
	{
		this->update_cursor();
		const TouchDemoReading now = this->reading();
		if (this->reported_ && now == this->reported_reading_) return;
		this->reported_ = true;
		this->reported_reading_ = now;
		const int x = static_cast<int>(std::lround(now.stick.x * 100.0f));
		const int y = static_cast<int>(std::lround(now.stick.y * 100.0f));
		this->message_ = std::wstring(L"Touch controls: A ") + (now.a_held ? L"held" : L"up") +
			L"  B " + (now.b_held ? L"held" : L"up") + L"  Stick " +
			std::to_wstring(x) + L"%, " + std::to_wstring(y) + L"%";
		if (this->on_status_) this->on_status_(this->message_);
	}

	void TouchDemoState::cancel_controls()
	{
		this->stick_.cancel();
		this->a_.cancel();
		this->b_.cancel();
		this->back_.cancel();
		this->mouse_active_ = false;
	}

	void TouchDemoState::update_cursor()
	{
		const Mouse& mouse = *this->app_->mouse();
		CursorShape shape = CursorShape::arrow;
		if (mouse.focused())
		{
			const Vector2F position = this->world_point(mouse.position());
			const std::array<PointerTarget, 4> targets = {
				PointerTarget{ this->back_.region(), true }, PointerTarget{ this->b_.region(), true },
				PointerTarget{ this->a_.region(), true }, PointerTarget{ this->stick_.region(), true }
			};
			const std::optional<std::size_t> hit = hit_pointer_target(targets, position);
			if (this->mouse_active_) shape = this->stick_.contact() == mouse_contact ? CursorShape::move : CursorShape::hand;
			else if (hit) shape = *hit == 3 ? CursorShape::crosshair : CursorShape::hand;
		}
		if (this->app_->cursor_shape() != shape) this->app_->set_cursor_shape(shape);
	}

	void TouchDemoState::on_deactivated()
	{
		this->cancel_controls();
		this->wait_for_clear_ = true;
		this->publish_status();
	}
	void TouchDemoState::on_suspend() { this->on_deactivated(); }
	void TouchDemoState::on_resume() { this->on_deactivated(); }

	void TouchDemoState::leave()
	{
		this->cancel_controls();
		this->publish_status();
		this->context()->pop();
	}

	void TouchDemoState::draw(Renderer& renderer) const
	{
		renderer.set_view_count(1);
		DrawList list = renderer.view(0);
		list.set_viewport(this->viewport_);
		list.set_camera(this->camera_);
		rectangle(list, this->white_, layout, background);
		list.draw_text(this->font_, L"HELD TOUCH CONTROLS", Vector2F(80, 64), Colour::white,
			2.0f, 0.0f, Vector2F::zero, 0.0f);
		list.draw_text(this->font_, L"Touch or left mouse: try the controls. Multiple fingers combine them.",
			Vector2F(80, 140), Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
		list.draw_text(this->font_, L"Escape / gamepad B: back", Vector2F(80, 180), Colour::gray,
			1.0f, 0.0f, Vector2F::zero, 0.0f);
		rectangle(list, this->white_, this->stick_.region(), inactive);
		const Vector2F centre = this->stick_.centre();
		rectangle(list, this->white_, RectangleF(stick_region.x, centre.y - 1, stick_region.width, 2), Colour::gray);
		rectangle(list, this->white_, RectangleF(centre.x - 1, stick_region.y, 2, stick_region.height), Colour::gray);
		const float size = this->b_.held() ? 64.0f : 32.0f;
		const Vector2F marker = centre + this->stick_.value() * (this->stick_.radius() - size * 0.5f);
		rectangle(list, this->white_, RectangleF(marker, size * 0.5f, size * 0.5f),
			this->a_.held() ? a_colour : Colour::white);
		rectangle(list, this->white_, this->a_.region(), this->a_.held() ? a_colour : inactive);
		rectangle(list, this->white_, this->b_.region(), this->b_.held() ? b_colour : inactive);
		list.draw_text(this->font_, L"A", Vector2F(787, 338), this->a_.held() ? background : Colour::white,
			2.0f, 0.0f, Vector2F::zero, 0.0f);
		list.draw_text(this->font_, L"B", Vector2F(997, 338), this->b_.held() ? background : Colour::white,
			2.0f, 0.0f, Vector2F::zero, 0.0f);
		rectangle(list, this->white_, this->back_.region(), this->back_.held() ? Colour::gray : inactive);
		list.draw_text(this->font_, L"Back", Vector2F(1074, 76), Colour::white,
			1.0f, 0.0f, Vector2F::zero, 0.0f);
		list.draw_text(this->font_, this->message_, Vector2F(80, 590), Colour::white,
			1.0f, 0.0f, Vector2F::zero, 0.0f);
		list.draw_text(this->font_, L"A colours the marker. B makes it larger. Stick +y points down.",
			Vector2F(80, 630), Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
		list.draw_text(this->font_, L"Resize, focus or capture changes cancel; lift before starting again.",
			Vector2F(80, 674), Colour::white, 0.9f, 0.0f, Vector2F::zero, 0.0f);
	}
}
