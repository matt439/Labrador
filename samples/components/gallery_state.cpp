#include "samples/components/gallery_state.h"
#include "samples/components/nine_slice.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/viewport.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace labrador;

namespace components
{
	namespace
	{
		const std::string font_name = "jetbrains_mono_bold_16";

		// Manual layout, as PHILOSOPHY (UI) asks: a column of labels at one
		// x, the values at another, one row every fifty units.
		constexpr float label_x = 160.0f;
		constexpr float value_x = 480.0f;
		constexpr float first_row_y = 240.0f;
		constexpr float row_spacing = 50.0f;

		// The slider's track: as wide as the values' column allows, and a
		// little shorter than a line of text, centred on it.
		constexpr float track_width = 300.0f;
		constexpr float track_height = 10.0f;
		constexpr float track_inset = 4.0f;

		constexpr Colour heading_colour(200, 208, 222);
		constexpr Colour help_colour(150, 160, 180);

		const RectangleF loop_preview_bounds(960.0f, 260.0f, 64.0f, 64.0f);
		const RectangleF once_preview_bounds(960.0f, 410.0f, 64.0f, 64.0f);

		float row_y(int row)
		{
			return first_row_y + static_cast<float>(row) * row_spacing;
		}

		RectangleF track_for_row(int row)
		{
			return RectangleF(value_x, row_y(row) + track_inset, track_width,
				track_height);
		}
	}

	GalleryState::GalleryState(Application* app) : app_(app)
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
		// the order things were added. One 16-texel frame, its 6-texel
		// corners drawn two units a texel and the rest stretched to fit.
		this->scene_->add(std::make_unique<NineSlice>("panel",
			resources->resolve_texture("panel"), RectangleI(0, 0, 16, 16),
			SliceBorder{ 6, 6, 6, 6 }, RectangleF(120.0f, 90.0f, 1040.0f, 490.0f),
			Colour::white, 2.0f));

		this->scene_->add(std::make_unique<UiText>("heading",
			L"COMPONENTS / copy one into your game", font_name,
			Vector2F(label_x, 120.0f), resources, heading_colour));
		this->scene_->add(std::make_unique<UiText>("help",
			L"Up/Down: choose   Left/Right: change   Enter/A: select   Esc/B: quit",
			font_name, Vector2F(label_x, 150.0f), resources, help_colour));
		this->scene_->add(std::make_unique<UiText>("animation_help",
			L"R / gamepad X: restart both animations", font_name,
			Vector2F(label_x, 180.0f), resources, help_colour));
		this->scene_->add(std::make_unique<UiText>("loop_label",
			L"Loop", font_name, Vector2F(900.0f, 230.0f), resources,
			help_colour));
		this->scene_->add(std::make_unique<UiText>("once_label",
			L"Once / holds last", font_name, Vector2F(900.0f, 380.0f), resources,
			help_colour));

		// Built before the rows, because every row's callback writes to it.
		this->status_ = this->scene_->add(std::make_unique<UiText>("status",
			L"Window mode applies; difficulty and volume sliders are examples.", font_name,
			Vector2F(label_x, row_y(6)), resources, help_colour));

		// An ordered list does not wrap; an unordered one does
		// (option_row.h).
		const std::vector<std::wstring> difficulties = {
			L"Easy", L"Normal", L"Hard" };
		this->difficulty_ = this->scene_->add(std::make_unique<OptionRow>(
			"difficulty", L"Difficulty", difficulties, 1,
			Vector2F(label_x, row_y(0)), value_x, font_name, resources,
			[this, difficulties](size_t index)
			{
				this->status_->set_text(L"Difficulty is now " +
					difficulties[index]);
			},
			false));

		const std::vector<std::wstring> window_modes = {
			L"Windowed", L"Full screen" };
		this->window_mode_ = this->scene_->add(std::make_unique<OptionRow>(
			"window", L"Window", window_modes, 0,
			Vector2F(label_x, row_y(1)), value_x, font_name, resources,
			[this, window_modes](size_t index)
			{
				this->app_->set_fullscreen(index == 1);
				this->status_->set_text(L"Window mode requested: " +
					window_modes[index]);
			}));
		this->window_mode_->set_index(
			this->app_->fullscreen() ? 1 : 0);

		this->music_ = this->scene_->add(std::make_unique<Slider>(
			"music", L"Music volume", 0, 100, 10, 70,
			Vector2F(label_x, row_y(2)), track_for_row(2), font_name, block,
			resources,
			[this](int value)
			{
				this->status_->set_text(L"Music slider example: " +
					std::to_wstring(value) + L"% (no audio loaded)");
			}));

		this->effects_ = this->scene_->add(std::make_unique<Slider>(
			"effects", L"Effects volume", 0, 100, 10, 100,
			Vector2F(label_x, row_y(3)), track_for_row(3), font_name, block,
			resources,
			[this](int value)
			{
				this->status_->set_text(L"Effects slider example: " +
					std::to_wstring(value) + L"% (no audio loaded)");
			}));

		UiText* quit = this->scene_->add(std::make_unique<UiText>("quit",
			L"Quit", font_name, Vector2F(label_x, row_y(4)), resources));

		// The rows are focus targets with no action: A on one means nothing,
		// and left and right are what change it. Quit is the one with an
		// action.
		this->focus_.add(this->difficulty_);
		this->focus_.add(this->window_mode_);
		this->focus_.add(this->music_);
		this->focus_.add(this->effects_);
		this->focus_.add(quit, [this]() { this->app_->quit(); });

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

		return false;
	}

	void GalleryState::update(float dt)
	{
		// A browser can refuse a request or leave full screen externally.
		// set_index follows that report without calling the request callback.
		this->window_mode_->set_index(
			this->app_->fullscreen() ? 1 : 0);

		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();

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
			this->focus_.move(0, direction);
		}
		else if (direction == Direction::left || direction == Direction::right)
		{
			this->adjust_focused(direction);
		}

		if (keyboard.pressed(Key::enter) || keyboard.pressed(Key::space) ||
			pads.pressed(0, GamepadButton::a))
		{
			this->focus_.activate(0);
		}

		this->loop_preview_->update(dt);
		this->once_preview_->update(dt);
		if (keyboard.pressed(Key::r) || pads.pressed(0, GamepadButton::x))
		{
			// Reset after the tick's step so the next draw shows frame zero.
			this->loop_preview_->restart();
			this->once_preview_->restart();
			this->status_->set_text(L"Both animation previews restarted.");
		}

		this->refresh_view();
		this->scene_->update(dt);
		this->scene_->end_tick();
	}

	void GalleryState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer, [this](int, DrawList& list)
			{
				list.set_filter(TextureFilter::point);
				this->loop_preview_->draw(list, loop_preview_bounds);
				this->once_preview_->draw(list, once_preview_bounds);
			});
	}

	void GalleryState::refresh_view()
	{
		// The screen is laid out at 1280x720 and fitted into whatever the
		// window is, so a resize scales it rather than moving the rows.
		const Vector2F resolution =
			this->app_->resolution_manager()->resolution_vec();
		const Viewport viewport(RectangleF(Vector2F::zero, resolution));

		this->scene_->clear_views();
		this->scene_->add_view(viewport,
			Camera::frame(RectangleF(0.0f, 0.0f, 1280.0f, 720.0f), viewport));
	}
}
