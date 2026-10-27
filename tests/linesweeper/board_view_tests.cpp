#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

// The board, drawn under the null renderer and read back as the sprites it
// submitted.
//
// A BOARDVIEW NEEDS A FONT, NOT A DEVICE. It measures and draws text, so its
// resource table has to hold a loaded font - and the null backend loads one
// through the same reader every other backend does, keeping the atlas as a
// size. So the board's colours are asserted here rather than only looked at:
// what a palette changes and what it leaves alone. How it looks is still the
// captures' (tools/linesweeper_capture/).
//
// The font is the sample's own, read from the source tree
// (LINESWEEPER_FONT_DIRECTORY, tests/linesweeper/CMakeLists.txt).
namespace
{
	using namespace labrador;
	using namespace linesweeper;

	constexpr std::array<Kind, 7> kinds = { Kind::i, Kind::j, Kind::l,
		Kind::o, Kind::s, Kind::t, Kind::z };

	// What a sprite's tint is in palette terms: a kind at full strength (a
	// cell, a preview or the shadow's edge), a kind's shadow fill, or
	// neither.
	struct Role
	{
		Kind kind = Kind::none;
		bool fill = false;
	};

	std::optional<Role> role_of(const Colour& tint, Palette palette)
	{
		for (const Kind kind : kinds)
		{
			if (tint == kind_colour(kind, palette))
			{
				return Role{ kind, false };
			}

			if (tint == shadow_fill_colour(kind, palette))
			{
				return Role{ kind, true };
			}
		}

		return std::nullopt;
	}

	class Harness
	{
	public:
		Harness()
		{
			this->renderer_.create_device(nullptr, 1280, 720, 1);
			this->renderer_.set_resources(&this->resources_);

			TextureData texture;
			texture.width = texture.height = 1;
			texture.levels.push_back(texture_level(texture.format, 1, 1, 0));
			texture.pixels.assign(4, 255);
			add_texture_asset(this->renderer_, this->resources_, "white",
				texture);
			this->white = this->resources_.resolve_texture("white");

			load_font_asset(this->renderer_, this->resources_,
				LINESWEEPER_FONT_DIRECTORY, "jetbrains_mono_bold_16");
		}

		// A copy, because the next begin_frame empties what recorded_sprites
		// refers to (tests/render/null_tests.cpp, Harness::end).
		std::vector<RecordedSprite> draw(const BoardView& board)
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(1);
			DrawList list = this->renderer_.view(0);
			board.draw(list);
			this->renderer_.submit();
			return recorded_sprites(this->renderer_);
		}

		RenderResources& resources() { return this->resources_; }

		TextureHandle white;

	private:
		RenderResources resources_;
		Renderer renderer_;
	};

	// One locked cell of every kind on the bottom row, a T falling with its
	// shadow below it, I in the hold slot and five more in the queue: every
	// kind drawn as a cell and as a preview, and one shadow.
	World every_kind()
	{
		World world;

		for (int index = 0; index < 7; ++index)
		{
			world.cells[static_cast<std::size_t>(
				cell_index(index, well_rows - 1))] =
				static_cast<std::uint8_t>(kinds[static_cast<std::size_t>(
					index)]);
		}

		world.current = Piece{ Kind::t, 0, 4,
			static_cast<std::int8_t>(well_buffer_rows + 2) };
		world.hold_kind = static_cast<std::uint8_t>(Kind::i);

		const std::array<Kind, 5> next = { Kind::o, Kind::s, Kind::z,
			Kind::j, Kind::l };
		for (std::size_t index = 0; index < next.size(); ++index)
		{
			world.queue[index] = static_cast<std::uint8_t>(next[index]);
		}
		world.queue_head = 0;
		world.queue_count = static_cast<std::uint8_t>(next.size());

		return world;
	}
}

TEST_CASE("a palette changes every piece the board draws and nothing else")
{
	Harness harness;
	const World world = every_kind();
	Palette palette = Palette::standard;
	const BoardView board(&world, &harness.resources(), &palette);

	const std::vector<RecordedSprite> standard = harness.draw(board);
	REQUIRE_FALSE(standard.empty());

	// What the standard frame drew, by role: each kind as a cell or preview,
	// and the falling T's shadow fill.
	std::array<int, 8> drawn = {};
	int fills = 0;
	for (const RecordedSprite& sprite : standard)
	{
		if (sprite.texture != harness.white)
		{
			continue;
		}

		const std::optional<Role> role =
			role_of(sprite.corners[0].colour, Palette::standard);
		if (role.has_value())
		{
			if (role->fill)
			{
				CHECK(role->kind == Kind::t);
				++fills;
			}
			else
			{
				++drawn[static_cast<std::size_t>(role->kind)];
			}
		}
	}

	// Each kind is in the stack and in a preview at least; the T is falling
	// and its shadow's four edges are drawn in its colour too.
	for (const Kind kind : kinds)
	{
		CAPTURE(static_cast<int>(kind));
		CHECK(drawn[static_cast<std::size_t>(kind)] >= 5);
	}
	CHECK(fills == 4);

	for (const Palette other : palettes)
	{
		if (other == Palette::standard)
		{
			continue;
		}

		CAPTURE(static_cast<int>(other));
		palette = other;
		const std::vector<RecordedSprite> recoloured = harness.draw(board);
		REQUIRE(recoloured.size() == standard.size());

		for (std::size_t index = 0; index < standard.size(); ++index)
		{
			CAPTURE(index);
			const RecordedSprite& before = standard[index];
			const RecordedSprite& after = recoloured[index];

			// The same quad from the same texture, in the same place.
			CHECK(after.texture == before.texture);
			for (std::size_t corner = 0; corner < std::size(before.corners);
				++corner)
			{
				CHECK(after.corners[corner].position ==
					before.corners[corner].position);
				CHECK(after.corners[corner].texcoord ==
					before.corners[corner].texcoord);
			}

			// A piece's colour becomes the same kind's in the new palette,
			// and everything else - the frame, the well, the grid, the
			// labels and the numbers - is exactly what it was.
			const Colour& was = before.corners[0].colour;
			const Colour& is = after.corners[0].colour;
			const std::optional<Role> role = before.texture == harness.white
				? role_of(was, Palette::standard)
				: std::nullopt;

			if (!role.has_value())
			{
				CHECK(is == was);
				CHECK_FALSE(role_of(is, other).has_value());
			}
			else if (role->fill)
			{
				CHECK(is == shadow_fill_colour(role->kind, other));
			}
			else
			{
				CHECK(is == kind_colour(role->kind, other));
				CHECK(is == shadow_edge_colour(role->kind, other));
			}
		}
	}
}
