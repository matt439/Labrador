#include "samples/components/gallery_state.h"
#include "samples/components/nine_slice.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/viewport.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace labrador;

namespace components
{
	namespace
	{
		const std::string font_name = "jetbrains_mono_bold_16";

		// The screen is laid out in this rectangle and fitted into whatever
		// the window is, so a resize scales it rather than moving the rows.
		const RectangleF layout_bounds(0.0f, 0.0f, 1280.0f, 720.0f);

		// Manual layout, as PHILOSOPHY (UI) asks: a column of labels at one
		// x, the values at another, one row every fifty units.
		constexpr float label_x = 160.0f;
		constexpr float value_x = 480.0f;
		constexpr float first_row_y = 240.0f;
		constexpr float row_spacing = 50.0f;

		// The heading and the two help lines above the rows, and the row the
		// status line takes below them.
		constexpr float heading_y = 120.0f;
		constexpr float help_y = 150.0f;
		constexpr float animation_help_y = 180.0f;
		constexpr int status_row = 6;

		// panel.dds is one 16-texel frame with 6-texel corners, drawn two
		// units a texel behind everything else.
		const RectangleI panel_source(0, 0, 16, 16);
		constexpr SliceBorder panel_border{ 6, 6, 6, 6 };
		constexpr float panel_scale = 2.0f;
		const RectangleF panel_bounds(120.0f, 90.0f, 1040.0f, 570.0f);
		const RectangleF name_target(880.0f, 490.0f, 260.0f, 40.0f);

		// The window row's choices, in the order init() lists them.
		constexpr size_t windowed_choice = 0;
		constexpr size_t full_screen_choice = 1;

		// The slider's track: as wide as the values' column allows, and a
		// little shorter than a line of text, centred on it.
		constexpr float track_width = 300.0f;
		constexpr float track_height = 10.0f;
		constexpr float track_inset = 4.0f;

		constexpr Colour heading_colour(200, 208, 222);
		constexpr Colour help_colour(150, 160, 180);
		constexpr float detail_scale = 0.7f;

		// What a pointer can choose on each row: a band the row's full
		// spacing tall, centred on its line of text, from just left of the
		// label to past the slider's number. Bands meet and do not overlap,
		// so a point between two rows belongs to exactly one.
		constexpr float band_left = label_x - 20.0f;
		constexpr float band_right = value_x + track_width + 80.0f;
		constexpr float text_middle = 10.0f;

		// A slider is grabbed on its track or within this of either end, so
		// a finger need not land on a ten-unit line; the label only focuses.
		constexpr float grab_margin = 20.0f;

		// The two previews, each with its label in a column to the left of
		// it and this far above it.
		const RectangleF loop_preview_bounds(960.0f, 260.0f, 64.0f, 64.0f);
		const RectangleF once_preview_bounds(960.0f, 410.0f, 64.0f, 64.0f);
		constexpr float preview_label_x = 900.0f;
		constexpr float preview_label_rise = 30.0f;

		// The view the page draws through, and the one a pointer is read
		// through: the layout fitted into a window of `resolution` pixels.
		struct LayoutView
		{
			Viewport viewport;
			Camera camera;
		};

		LayoutView layout_view(const Vector2F& resolution)
		{
			const Viewport viewport(RectangleF(Vector2F::zero, resolution));
			return { viewport, Camera::frame(layout_bounds, viewport) };
		}

		Vector2F preview_label_position(const RectangleF& preview)
		{
			return Vector2F(preview_label_x, preview.y - preview_label_rise);
		}

		float row_y(int row)
		{
			return first_row_y + static_cast<float>(row) * row_spacing;
		}

		RectangleF track_for_row(int row)
		{
			return RectangleF(value_x, row_y(row) + track_inset, track_width,
				track_height);
		}

		RectangleF band_for_row(int row)
		{
			return RectangleF(band_left,
				row_y(row) + text_middle - row_spacing * 0.5f,
				band_right - band_left, row_spacing);
		}

		bool any_key_pressed(const Keyboard& keyboard)
		{
			for (int value = 1; value < static_cast<int>(Key::count); ++value)
			{
				if (keyboard.pressed(static_cast<Key>(value)))
				{
					return true;
				}
			}
			return false;
		}
	}

	GalleryState::GalleryState(Application* app, StatusChanged on_status) :
		app_(app),
		on_status_(std::move(on_status))
	{

	}

	void GalleryState::init()
	{
		RenderResources* resources = this->app_->render_resources();
		const TextureHandle block = resources->resolve_texture(white_texture_name);
		const RenderResources::SheetStrip loop_strip =
			resources->resolve_sheet_strip("animation_preview", "pulse_loop");
		const RenderResources::SheetStrip once_strip =
			resources->resolve_sheet_strip("animation_preview", "pulse_once");
		this->loop_preview_.emplace(*resources, loop_strip);
		this->once_preview_.emplace(*resources, once_strip);

		this->scene_ = std::make_unique<Scene>(nullptr, nullptr);
		this->refresh_view();

		// The panel behind the rows, added first because a scene draws in
		// the order things were added: its corners kept, the rest stretched
		// to fit.
		this->scene_->add(std::make_unique<NineSlice>("panel",
			resources->resolve_texture("panel"), panel_source, panel_border,
			panel_bounds, Colour::white, panel_scale));

		this->scene_->add(std::make_unique<UiText>("heading",
			L"COMPONENTS / copy one into your game", font_name,
			Vector2F(label_x, heading_y), resources, heading_colour));
		this->scene_->add(std::make_unique<UiText>("help",
			L"Arrows, click or tap: choose and change   Enter/A: select   Esc/B: quit",
			font_name, Vector2F(label_x, help_y), resources, help_colour));
		this->animation_help_ = this->scene_->add(std::make_unique<UiText>("animation_help",
			L"R / gamepad X: restart both animations", font_name,
			Vector2F(label_x, animation_help_y), resources, help_colour));
		this->scene_->add(std::make_unique<UiText>("loop_label",
			L"Loop", font_name, preview_label_position(loop_preview_bounds),
			resources, help_colour));
		this->scene_->add(std::make_unique<UiText>("once_label",
			L"Once / holds last", font_name,
			preview_label_position(once_preview_bounds), resources,
			help_colour));

		// Empty until set_status gives it its first line, so a page told of
		// every line hears this one too.
		this->status_ = this->scene_->add(std::make_unique<UiText>("status",
			L"", font_name, Vector2F(label_x, row_y(status_row)), resources,
			help_colour, false, detail_scale));
		this->set_status(
			L"Window mode applies; difficulty and volume sliders are examples.");

		// An ordered list does not wrap; an unordered one does
		// (option_row.h).
		const std::vector<std::wstring> difficulties = {
			L"Easy", L"Normal", L"Hard" };
		this->difficulty_ = this->scene_->add(std::make_unique<OptionRow>(
			"difficulty", L"Difficulty", difficulties, 1,
			Vector2F(label_x, row_y(0)), value_x, font_name, resources,
			[this, difficulties](size_t index)
			{
				this->set_status(L"Difficulty is now " +
					difficulties[index]);
			},
			false));

		const std::vector<std::wstring> window_modes = {
			L"Windowed", L"Full screen" };
		this->window_mode_ = this->scene_->add(std::make_unique<OptionRow>(
			"window", L"Window", window_modes, windowed_choice,
			Vector2F(label_x, row_y(1)), value_x, font_name, resources,
			[this, window_modes](size_t index)
			{
				this->app_->set_fullscreen(index == full_screen_choice);
				this->set_status(L"Window mode requested: " +
					window_modes[index]);
			}));
		this->window_mode_->set_index(this->app_->fullscreen() ?
			full_screen_choice : windowed_choice);

		this->music_ = this->scene_->add(std::make_unique<Slider>(
			"music", L"Music volume", 0, 100, 10, 70,
			Vector2F(label_x, row_y(2)), track_for_row(2), font_name, block,
			resources,
			[this](int value)
			{
				this->set_status(L"Music slider example: " +
					std::to_wstring(value) + L"% (no audio loaded)");
			}));

		this->effects_ = this->scene_->add(std::make_unique<Slider>(
			"effects", L"Effects volume", 0, 100, 10, 100,
			Vector2F(label_x, row_y(3)), track_for_row(3), font_name, block,
			resources,
			[this](int value)
			{
				this->set_status(L"Effects slider example: " +
					std::to_wstring(value) + L"% (no audio loaded)");
			}));

		this->quit_ = this->scene_->add(std::make_unique<UiText>("quit",
			L"Quit", font_name, Vector2F(label_x, row_y(4)), resources));
		this->binding_ = this->scene_->add(std::make_unique<BindingRow>("binding",
			L"Rebind key", L"Restart: r", Vector2F(label_x, row_y(5)),
			value_x, font_name, resources));
		this->scene_->add(std::make_unique<UiText>("binding_help",
			L"Row: Left/Right action; Enter capture; Backspace reset", font_name,
			Vector2F(label_x, 570.0f), resources, help_colour, false, detail_scale));
		this->binding_status_ = this->scene_->add(std::make_unique<UiText>("binding_status",
			L"Capture: letters/numbers; Esc/B or another row cancels", font_name,
			Vector2F(label_x, 610.0f), resources, help_colour, false, detail_scale));
		this->refresh_binding_value();
		this->name_ = this->scene_->add(std::make_unique<TextField>("name",
			L"Name", "Player", TextFieldLimits{ 12, 180.0f },
			Vector2F(900.0f, 500.0f), 960.0f, font_name, resources));
		this->scene_->add(std::make_unique<UiText>("name_help",
			L"Enter saves / Esc cancels", font_name,
			Vector2F(900.0f, 535.0f), resources, help_colour, false, detail_scale));
		this->scene_->add(std::make_unique<UiText>("name_scope",
			L"ASCII only; 12 chars", font_name,
			Vector2F(900.0f, 555.0f), resources, help_colour, false, detail_scale));
		this->scene_->add(std::make_unique<UiText>("name_focus_help",
			L"Focus loss cancels", font_name,
			Vector2F(900.0f, 575.0f), resources, help_colour, false, detail_scale));
		this->name_status_ = this->scene_->add(std::make_unique<UiText>(
			"name_status", L"Enter/click Name to edit", font_name,
			Vector2F(900.0f, 610.0f), resources, help_colour, false, detail_scale));

		// Left/right change settings rows or select an action to rebind. Quit
		// closes the page; Binding and Name start their page-owned input modes.
		this->focus_.add(this->difficulty_);
		this->focus_.add(this->window_mode_);
		this->focus_.add(this->music_);
		this->focus_.add(this->effects_);
		this->focus_.add(this->quit_, [this]() { this->app_->quit(); });
		this->focus_.add(this->binding_, [this]() { this->begin_binding_capture(); });
		this->focus_.add(this->name_, [this]() { this->begin_name_edit(); });

		// The same rows, in the same order, as places a pointer can choose.
		UiWidget* const rows[] = { this->difficulty_, this->window_mode_,
			this->music_, this->effects_, this->quit_, this->binding_ };
		int row = 0;
		for (UiWidget* widget : rows)
		{
			this->pointer_targets_.push_back({ band_for_row(row), true });
			this->pointer_widgets_.push_back(widget);
			++row;
		}
		this->pointer_targets_.push_back({ name_target, true });
		this->pointer_widgets_.push_back(this->name_);

		// Adds are pending until a tick ends, and the frame drawn before the
		// first update() is a real one.
		this->scene_->end_tick();

		this->repeat_.start_held(this->held_direction());
	}

	Direction GalleryState::held_direction() const
	{
		const Keyboard& keyboard = *this->app_->keyboard();

		if (keyboard.held(Key::up))
		{
			return Direction::up;
		}

		if (keyboard.held(Key::down))
		{
			return Direction::down;
		}

		if (keyboard.held(Key::left))
		{
			return Direction::left;
		}

		if (keyboard.held(Key::right))
		{
			return Direction::right;
		}

		return pad_direction(this->app_->gamepads()->state(0));
	}

	bool GalleryState::adjust_focused(Direction direction)
	{
		const UiWidget* focused = this->focus_.focused(0);

		if (focused == this->difficulty_)
		{
			return this->difficulty_->adjust(direction);
		}

		if (focused == this->window_mode_)
		{
			return this->window_mode_->adjust(direction);
		}

		if (focused == this->music_)
		{
			return this->music_->adjust(direction);
		}

		if (focused == this->effects_)
		{
			return this->effects_->adjust(direction);
		}
		if (focused == this->binding_)
		{
			return this->adjust_binding_selection(direction);
		}

		return false;
	}

	bool GalleryState::move_row(Direction direction)
	{
		// The two bottom rows occupy separate columns. Their visual bounds
		// overlap vertically, so the page supplies its reading order instead
		// of asking spatial navigation to guess between those columns.
		for (size_t index = 0; index < this->pointer_widgets_.size(); ++index)
		{
			if (this->pointer_widgets_[index] == this->focus_.focused(0))
			{
				const size_t count = this->pointer_widgets_.size();
				const size_t next = direction == Direction::up ?
					(index + count - 1) % count : (index + 1) % count;
				return this->focus_.set_focused(0, this->pointer_widgets_[next]);
			}
		}
		return false;
	}

	void GalleryState::update(float dt)
	{
		// A browser can refuse a request or leave full screen externally.
		// set_index follows that report without calling the request callback.
		this->window_mode_->set_index(this->app_->fullscreen() ?
			full_screen_choice : windowed_choice);

		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();
		const bool was_editing = this->name_->editing();
		const bool was_capturing = this->binding_->capturing();
		bool binding_owned = false;
		if (was_editing)
		{
			this->update_name_edit();
			if (!this->name_->editing())
			{
				// A queued release on Name cannot reopen an edit just ended
				// by Enter, Escape or keyboard-focus loss in the same poll.
				this->pointer_.cancel();
				this->pressed_row_ = nullptr;
				this->dragging_ = nullptr;
			}
			else
			{
				this->update_pointer();
			}
		}
		else if ((binding_owned = this->update_binding_input()))
		{
			// Capture owns keys before navigation and game actions. Drop any
			// pointer gesture when a key acts, including a queued release that
			// could otherwise reopen capture just ended by commit or cancel.
			if (!this->binding_->capturing() || any_key_pressed(keyboard) ||
				pads.pressed(0, GamepadButton::a) || pads.pressed(0, GamepadButton::b))
			{
				this->pointer_.cancel();
				this->pressed_row_ = nullptr;
				this->dragging_ = nullptr;
			}
			else
			{
				this->update_pointer();
			}
		}
		else
		{
			if (keyboard.pressed(Key::escape) || pads.pressed(0, GamepadButton::b))
			{
				this->app_->quit();
				return;
			}

			const Direction direction =
				this->repeat_.update(this->held_direction(), dt);

			// Both returns say whether anything moved, which is what a cursor
			// sound would hang on. This sample has no audio, so they go unused.
			if (direction == Direction::up || direction == Direction::down)
			{
				this->move_row(direction);
			}
			else if (direction == Direction::left || direction == Direction::right)
			{
				this->adjust_focused(direction);
			}

			const bool confirmed = keyboard.pressed(Key::enter) ||
				keyboard.pressed(Key::space) || pads.pressed(0, GamepadButton::a);
			if (confirmed)
			{
				this->focus_.activate(0);
			}

			// THE KEYS AND THE PAD COME FIRST. A tick in which either moved the
			// cursor, changed a row or chose one drops any click or drag in
			// progress, so the two can never both act on a row in one tick.
			if (direction != Direction::none || confirmed)
			{
				this->pointer_.cancel();
				this->pressed_row_ = nullptr;
				this->dragging_ = nullptr;
			}
			else
			{
				this->update_pointer();
			}
		}

		this->loop_preview_->update(dt);
		this->once_preview_->update(dt);
		const bool actions_enabled = !was_editing && !this->name_->editing() &&
			!was_capturing && !this->binding_->capturing() && !binding_owned;
		if (actions_enabled &&
			(keyboard.pressed(this->bindings_.key(GalleryAction::restart_previews)) ||
				pads.pressed(0, GamepadButton::x)))
		{
			// Reset after the tick's step so the next draw shows frame zero.
			this->loop_preview_->restart();
			this->once_preview_->restart();
			this->set_status(L"Both animation previews restarted.");
		}
		if (actions_enabled &&
			keyboard.pressed(this->bindings_.key(GalleryAction::reset_difficulty)))
		{
			this->difficulty_->set_index(1);
			this->set_status(L"Difficulty reset to Normal.");
		}

		this->refresh_view();
		this->scene_->update(dt);
		this->scene_->end_tick();
	}

	void GalleryState::on_deactivated()
	{
		this->cancel_name_edit();
		if (this->binding_ != nullptr)
		{
			this->cancel_binding_capture(L"Binding capture cancelled: focus lost.");
		}
		this->pointer_.cancel();
		this->pressed_row_ = nullptr;
		this->dragging_ = nullptr;
	}

	void GalleryState::update_pointer()
	{
		const PointerSample sample =
			this->pointer_.update(*this->app_->mouse(), *this->app_->touch());

		if (sample.phase == PointerPhase::pressed)
		{
			const Vector2F point = this->world_point(sample.position);
			const std::optional<size_t> hit =
				hit_target(this->pointer_targets_, point);
			if (this->name_->editing() &&
				(!hit.has_value() || this->pointer_widgets_[*hit] != this->name_))
			{
				this->cancel_name_edit();
			}
			if (!hit.has_value())
			{
				return;
			}

			// Focus first, as LineSweeper's pause menu does, so whatever the
			// release does belongs to the row the cursor is now on.
			UiWidget* const row = this->pointer_widgets_[*hit];
			if (row != this->binding_)
			{
				this->cancel_binding_capture(L"Binding capture cancelled: another row.");
			}
			this->focus_.set_focused(0, row);
			this->pressed_row_ = row;

			for (Slider* slider : { this->music_, this->effects_ })
			{
				const RectangleF& track = slider->track().bounds();
				if (row == slider && point.x >= track.x - grab_margin &&
					point.x < track.x + track.width + grab_margin)
				{
					this->dragging_ = slider;
					slider->drag_to(point.x);
				}
			}
			return;
		}

		if (sample.phase == PointerPhase::held)
		{
			// Followed wherever it goes, so a drag past either end of the
			// track clamps there rather than letting go.
			if (this->dragging_ != nullptr)
			{
				this->dragging_->drag_to(this->world_point(sample.position).x);
			}
			return;
		}

		UiWidget* const pressed = this->pressed_row_;
		const bool dragged = this->dragging_ != nullptr;
		this->pressed_row_ = nullptr;
		this->dragging_ = nullptr;

		// A cancelled contact completes nothing, and a drag has already done
		// what it does. A slider keeps the value the drag left it at.
		if (sample.phase != PointerPhase::released || dragged ||
			pressed == nullptr)
		{
			return;
		}

		// A click or a tap completes only on the row it began on: letting go
		// anywhere else is the player changing their mind.
		const Vector2F point = this->world_point(sample.position);
		const std::optional<size_t> hit =
			hit_target(this->pointer_targets_, point);
		if (!hit.has_value() || this->pointer_widgets_[*hit] != pressed)
		{
			return;
		}

		// On an option row's value, the left half steps back and the right
		// half on, as the arrows do; on the label it only focused.
		for (OptionRow* option : { this->difficulty_, this->window_mode_ })
		{
			if (pressed == option && point.x >= value_x)
			{
				const RectangleF value = option->value().bounds();
				option->adjust(point.x < value.x + value.width * 0.5f ?
					Direction::left : Direction::right);
			}
		}

		// Quit, Name and Binding have actions; activation runs the selected one.
		if (pressed == this->quit_ || pressed == this->name_ || pressed == this->binding_)
		{
			this->focus_.activate(0);
		}
	}

	Vector2F GalleryState::world_point(const Vector2I& client) const
	{
		// The view refresh_view() gave the scene.
		const LayoutView view = layout_view(
			this->app_->resolution_manager()->resolution_vec());
		return view.camera.world_position(client, view.viewport);
	}

	void GalleryState::set_status(const std::wstring& text)
	{
		this->status_->set_text(text);
		if (this->on_status_)
		{
			this->on_status_(text);
		}
	}

	void GalleryState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer, [this](int, DrawList& list)
			{
				// The previews are pixel art, so they are point-sampled
				// whatever filter the view was left in before the overlay.
				list.set_filter(TextureFilter::point);
				this->loop_preview_->draw(list, loop_preview_bounds);
				this->once_preview_->draw(list, once_preview_bounds);
			});
	}

	void GalleryState::refresh_view()
	{
		const LayoutView view = layout_view(
			this->app_->resolution_manager()->resolution_vec());

		this->scene_->clear_views();
		this->scene_->add_view(view.viewport, view.camera);
	}
}
