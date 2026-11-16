#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/render_resources.h"
#include "engine/render/resolution_manager.h"
#include "engine/ui/widget.h"
#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/assets.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/states/pause_state.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

using namespace labrador;
using namespace linesweeper;

namespace
{
	ApplicationOptions pause_options(const Vector2I& size)
	{
		ApplicationOptions options;
		options.window_class_name = L"LineSweeperTouchPauseTests";
		options.visible = false;
		options.view_capacity = 1;
		options.max_threads = 1;
		options.client_size = size;
		return options;
	}

	// The real pause state and delivered font, with a hidden null-backend
	// Application. Input still crosses client pixels and the actual camera.
	class Menu
	{
	public:
		explicit Menu(const Vector2I& size = Vector2I(1280, 720)) : app_(pause_options(size))
		{
			this->app_.initialize();
			this->app_.load_manifest(std::string(LINESWEEPER_CONTENT_DIRECTORY) + "manifest.json");
			this->app_.keyboard()->set_focused(true);
			this->app_.touch()->set_focused(true);
			this->app_.keyboard()->poll();
			this->app_.keyboard()->poll();
			this->app_.touch()->poll();
			this->app_.touch()->poll();
			this->context_.push<PauseChoice>(std::make_unique<PauseState>(&this->app_, &this->palette_),
				[this](const PauseChoice& choice) { this->choice_ = choice; });
		}

		void step()
		{
			this->app_.keyboard()->poll();
			this->app_.touch()->poll();
			this->context_.update(1.0f / 60.0f);
		}

		RectangleF text_bounds(int row)
		{
			const std::wstring text = row == 0 ? L"RESUME" : row == 1 ? L"RESTART" :
				row == 2 ? std::wstring(L"COLOURS: ") + palette_name(this->palette_) : L"QUIT";
			RenderResources* resources = this->app_.render_resources();
			const FontHandle font = resources->resolve_sprite_font(font_name);
			const Vector2F size = resources->measure_text(font, text);
			const Vector2F position(std::floor((landscape_width - size.x) * 0.5f),
				312.0f + static_cast<float>(row) * 44.0f);
			const UiText label("probe", text, font_name, position, resources);
			return label.bounds();
		}

		RectangleF band(int row)
		{
			const RectangleF text = this->text_bounds(row);
			return RectangleF(text.center(), std::max(160.0f, text.width * 0.5f + 16.0f), 22.0f);
		}

		Vector2I client_at(const Vector2F& point)
		{
			const Arrangement arrangement = arrange(this->app_.resolution_manager()->resolution_vec());
			const Vector2F client = arrangement.camera.calculate_view_position(point);
			return Vector2I(static_cast<int>(std::lround(client.x)), static_cast<int>(std::lround(client.y)));
		}

		void down(uint32_t id, const Vector2F& point)
		{
			this->app_.touch()->on_down(id, this->client_at(point));
		}

		void up(uint32_t id, const Vector2F& point)
		{
			this->app_.touch()->on_up(id, this->client_at(point));
		}

		Keyboard& keyboard() { return *this->app_.keyboard(); }
		Palette palette() const { return this->palette_; }
		std::optional<PauseChoice> choice() const { return this->choice_; }
		int depth() const { return this->context_.depth(); }

	private:
		Application app_;
		Palette palette_ = Palette::standard;
		std::optional<PauseChoice> choice_;
		StateContext context_;
	};
}

TEST_CASE("pause touch uses the expanded band on press and never repeats while held")
{
	for (const Vector2I& size : { Vector2I(1280, 720), Vector2I(480, 960) })
	{
		Menu menu(size);
		const RectangleF target = menu.band(2);
		const Vector2F point(target.right() - 8.0f, target.center().y);
		REQUIRE(point.x > menu.text_bounds(2).right());
		menu.down(10, point);
		menu.step();
		CHECK(menu.palette() == next_palette(Palette::standard));
		CHECK(menu.depth() == 1);
		menu.step();
		menu.step();
		CHECK(menu.palette() == next_palette(Palette::standard));
		menu.up(10, point);
		menu.step();
		CHECK(menu.palette() == next_palette(Palette::standard));
		CHECK_FALSE(menu.choice());
	}
}

TEST_CASE("pause touch preserves left-inclusive and right-exclusive row bands")
{
	{
		Menu menu;
		const RectangleF resume = menu.band(0);
		const Vector2F outside(resume.right(), resume.center().y);
		menu.down(1, outside);
		menu.step();
		CHECK_FALSE(menu.choice());
		menu.up(1, outside);
		menu.step();
		menu.down(2, Vector2F(resume.left(), resume.center().y));
		menu.step();
		CHECK(menu.choice() == std::optional<PauseChoice>(PauseChoice::resume));
		CHECK(menu.depth() == 0);
	}

	Menu adjacent;
	const RectangleF top = adjacent.band(0);
	// Touch samples integral client pixels; the delivered font's shared edge
	// is fractional, so use the first whole pixel on the next row's side.
	adjacent.down(3, Vector2F(top.center().x, std::ceil(top.bottom())));
	adjacent.step();
	CHECK(adjacent.choice() == std::optional<PauseChoice>(PauseChoice::restart));
}

TEST_CASE("pause touch preserves contact order and performs at most one choice")
{
	{
		Menu colours_first;
		colours_first.down(20, colours_first.band(2).center());
		colours_first.down(10, colours_first.band(1).center());
		colours_first.step();
		CHECK_FALSE(colours_first.choice());
		CHECK(colours_first.palette() == next_palette(Palette::standard));
	}

	Menu restart_first;
	restart_first.down(20, restart_first.band(1).center());
	restart_first.down(10, restart_first.band(2).center());
	restart_first.step();
	CHECK(restart_first.choice() == std::optional<PauseChoice>(PauseChoice::restart));
	CHECK(restart_first.palette() == Palette::standard);
}

TEST_CASE("pause cancel and confirm retain priority over a newly pressed touch")
{
	for (const Key key : { Key::escape, Key::enter })
	{
		Menu menu;
		menu.down(1, menu.band(2).center());
		menu.keyboard().on_key_down(key);
		menu.step();
		CHECK(menu.choice() == std::optional<PauseChoice>(PauseChoice::resume));
		CHECK(menu.palette() == Palette::standard);
	}
}
