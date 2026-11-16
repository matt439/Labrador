#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/viewport.h"
#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/assets.h"
#include "samples/linesweeper/presentation/touch_controls.h"
#include "tests/linesweeper/null_render_harness.h"

#include <array>
#include <cstddef>
#include <vector>

using namespace labrador;
using namespace linesweeper;

TEST_CASE("shared alignment preserves the touch labels' fractional draw geometry")
{
	linesweeper_tests::NullHarness harness;
	TouchControls controls(&harness.resources());
	const FontHandle font = harness.resources().resolve_sprite_font(font_name);
	const TextureHandle atlas = harness.resources().font(font)->atlas();
	const std::array<const wchar_t*, control_count> labels = {
		L"<", L">", L"DOWN", L"DROP", L"CW", L"CCW", L"HOLD", L"PAUSE",
	};
	const std::array<Vector2F, 10> buffers = {
		Vector2F(1280, 720), Vector2F(1170, 2532), Vector2F(1080, 2400),
		Vector2F(720, 1280), Vector2F(768, 1024), Vector2F(2532, 1170),
		Vector2F(1920, 1200), Vector2F(1361, 766),
		Vector2F(389.75f, 843.5f), Vector2F(843.5f, 389.75f),
	};
	for (const Vector2F& buffer : buffers)
	{
		CAPTURE(buffer.x);
		CAPTURE(buffer.y);
		const Arrangement arrangement = arrange(buffer);
		controls.set(arrangement, 0, true);
		const std::vector<RecordedSprite> drawn = harness.draw(controls, arrangement.camera);
		std::vector<RecordedSprite> actual;
		for (const RecordedSprite& sprite : drawn)
		{
			if (sprite.texture == atlas)
			{
				actual.push_back(sprite);
			}
		}

		Renderer& renderer = harness.renderer();
		renderer.begin_frame();
		renderer.set_view_count(1);
		DrawList list = renderer.view(0);
		list.set_camera(arrangement.camera);
		list.set_viewport(Viewport(0, 0, 1280, 720));
		const std::array<RectangleF, control_count> rectangles = control_rectangles(arrangement);
		for (std::size_t index = 0; index < labels.size(); ++index)
		{
			const RectangleF& rectangle = rectangles[index];
			const Vector2F size = harness.resources().measure_text(font, labels[index]) * 2.0f;
			// The layout contract: centre the scaled size without snapping.
			const Vector2F position(rectangle.x + (rectangle.width - size.x) * 0.5f,
				rectangle.y + (rectangle.height - size.y) * 0.5f);
			list.draw_text(font, labels[index], position, Colour(200, 208, 222),
				2.0f, 0.0f, Vector2F::zero, 0.0f);
		}
		renderer.submit();
		const std::vector<RecordedSprite>& expected = recorded_sprites(renderer);
		REQUIRE(actual.size() == expected.size());
		for (std::size_t index = 0; index < actual.size(); ++index)
		{
			for (std::size_t corner = 0; corner < 4; ++corner)
			{
				CHECK(actual[index].corners[corner].position == expected[index].corners[corner].position);
				CHECK(actual[index].corners[corner].texcoord == expected[index].corners[corner].texcoord);
			}
		}
	}
}
