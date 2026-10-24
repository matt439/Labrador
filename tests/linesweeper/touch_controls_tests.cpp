#include <doctest/doctest.h>

#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/touch_controls.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

using labrador::TouchPoint;
using linesweeper::Arrangement;
using linesweeper::Control;
using linesweeper::ControlSet;
using linesweeper::Orientation;
using linesweeper::arrange;
using linesweeper::control_at;
using linesweeper::control_bit;
using linesweeper::control_count;
using linesweeper::control_rectangles;
using linesweeper::controls_held;
using mattmath::RectangleF;
using mattmath::Vector2F;
using mattmath::Vector2I;

// The screen's two arrangements and the touch controls' hit test, with no
// device.
//
// Both are arithmetic on a buffer size and a camera, and both decide where a
// finger lands, so they are pinned here rather than only looked at: a control
// that is drawn in one place and hit-tested in another is a phone that
// ignores the thumb on it, and nothing on screen says why. What the controls
// LOOK like is checked by looking at them; TouchControls itself measures its
// labels through a font a device filled and is not constructed here.
namespace
{
	// The board block portrait frames, in board space: from the HOLD column's
	// left edge to the NEXT column's right, and from above their labels to
	// below the well's frame (board_view.cpp).
	const RectangleF board_block(341.0f, 46.0f, 550.0f, 590.0f);

	// The pixel a board-space point is drawn at, which is what a finger on
	// it reports.
	Vector2I buffer_pixel(const Arrangement& arrangement, const Vector2F& board)
	{
		const Vector2F view =
			arrangement.camera.calculate_view_position(board);

		return Vector2I(static_cast<int>(std::floor(view.x)),
			static_cast<int>(std::floor(view.y)));
	}

	bool overlaps(const RectangleF& a, const RectangleF& b)
	{
		return a.x < b.x + b.width && b.x < a.x + a.width &&
			a.y < b.y + b.height && b.y < a.y + a.height;
	}

	bool inside(const RectangleF& inner, const RectangleF& outer)
	{
		return inner.x >= outer.x && inner.y >= outer.y &&
			inner.x + inner.width <= outer.x + outer.width &&
			inner.y + inner.height <= outer.y + outer.height;
	}

	TouchPoint point_at(std::uint32_t id, const Vector2I& position)
	{
		TouchPoint point;
		point.id = id;
		point.position = position;
		return point;
	}

	// Phone-shaped buffers, in device pixels, both ways up, and two that are
	// not phones.
	const std::array<Vector2F, 4> portrait_buffers = {
		Vector2F(1170.0f, 2532.0f),
		Vector2F(1080.0f, 2400.0f),
		Vector2F(720.0f, 1280.0f),
		Vector2F(768.0f, 1024.0f),
	};

	const std::array<Vector2F, 4> landscape_buffers = {
		Vector2F(1280.0f, 720.0f),
		Vector2F(2532.0f, 1170.0f),
		Vector2F(1920.0f, 1200.0f),
		Vector2F(1361.0f, 766.0f),
	};
}

TEST_SUITE("linesweeper arrangement")
{
	TEST_CASE("1280x720 is the identity, so the window main.cpp opens draws as it always did")
	{
		const Arrangement arrangement = arrange(Vector2F(1280.0f, 720.0f));

		CHECK(arrangement.orientation == Orientation::landscape);
		CHECK(arrangement.camera.scale == 1.0f);
		CHECK(arrangement.camera.translation.x == 0.0f);
		CHECK(arrangement.camera.translation.y == 0.0f);
		CHECK(arrangement.visible == RectangleF(0.0f, 0.0f, 1280.0f, 720.0f));
	}

	TEST_CASE("the orientation follows the aspect, and a square is landscape")
	{
		CHECK(arrange(Vector2F(720.0f, 1280.0f)).orientation ==
			Orientation::portrait);
		CHECK(arrange(Vector2F(1000.0f, 1001.0f)).orientation ==
			Orientation::portrait);
		CHECK(arrange(Vector2F(1000.0f, 1000.0f)).orientation ==
			Orientation::landscape);
		CHECK(arrange(Vector2F(2532.0f, 1170.0f)).orientation ==
			Orientation::landscape);
	}

	TEST_CASE("landscape fits the whole 1280x720 screen and centres the slack")
	{
		for (const Vector2F& buffer : landscape_buffers)
		{
			CAPTURE(buffer.x);
			CAPTURE(buffer.y);

			const Arrangement arrangement = arrange(buffer);
			const RectangleF& visible = arrangement.visible;

			CHECK(visible.x <= 0.0f);
			CHECK(visible.y <= 0.0f);
			CHECK(visible.x + visible.width >= doctest::Approx(1280.0f));
			CHECK(visible.y + visible.height >= doctest::Approx(720.0f));

			// Centred: as much shown past the left edge as past the right.
			CHECK(-visible.x == doctest::Approx(
				visible.x + visible.width - 1280.0f).epsilon(0.001));
			CHECK(-visible.y == doctest::Approx(
				visible.y + visible.height - 720.0f).epsilon(0.001));
		}
	}

	TEST_CASE("portrait shows the whole board block, across the full width of a phone")
	{
		for (const Vector2F& buffer : portrait_buffers)
		{
			CAPTURE(buffer.x);
			CAPTURE(buffer.y);

			const Arrangement arrangement = arrange(buffer);

			CHECK(arrangement.orientation == Orientation::portrait);
			CHECK(inside(board_block, arrangement.visible));

			// The well's middle is the buffer's middle.
			CHECK(arrangement.visible.x + arrangement.visible.width * 0.5f ==
				doctest::Approx(640.0f));
		}

		// A phone is narrow enough that its width is what limits: the block's
		// 620 units are exactly the buffer's width.
		const Arrangement phone = arrange(Vector2F(1170.0f, 2532.0f));
		CHECK(phone.visible.width == doctest::Approx(620.0f));
		CHECK(phone.camera.scale == doctest::Approx(1170.0f / 620.0f));
	}

	TEST_CASE("a squat portrait buffer is fitted to its height and keeps room for the controls")
	{
		const Arrangement arrangement = arrange(Vector2F(768.0f, 1024.0f));

		CHECK(arrangement.orientation == Orientation::portrait);
		CHECK(arrangement.visible.height ==
			doctest::Approx(linesweeper::portrait_min_height));
		CHECK(arrangement.visible.width > 620.0f);
	}

	TEST_CASE("portrait centres the board in the height the controls leave")
	{
		const Arrangement arrangement = arrange(Vector2F(1170.0f, 2532.0f));
		const std::array<RectangleF, control_count> rectangles =
			control_rectangles(arrangement);

		float controls_top = rectangles[0].y;

		for (const RectangleF& rectangle : rectangles)
		{
			controls_top = rectangle.y < controls_top ? rectangle.y : controls_top;
		}

		const float above = linesweeper::portrait_board_top -
			arrangement.visible.y;
		const float below = controls_top - linesweeper::portrait_board_bottom;

		// The same slack either side, give or take the margin the controls
		// keep inside their own band.
		CHECK(above > 0.0f);
		CHECK(below - above ==
			doctest::Approx(linesweeper::control_margin).epsilon(0.01));
	}

	TEST_CASE("a buffer pixel and the board point drawn at it are each other's inverse")
	{
		for (const Vector2F& buffer : portrait_buffers)
		{
			const Arrangement arrangement = arrange(buffer);
			const Vector2F board(640.0f, 400.0f);
			const Vector2F view =
				arrangement.camera.calculate_view_position(board);
			const Vector2F back = arrangement.camera.world_position(
				Vector2I(static_cast<int>(view.x), static_cast<int>(view.y)),
				arrangement.viewport);

			// Within a pixel, because the buffer point is whole.
			CHECK(std::fabs(back.x - board.x) <= 1.0f / arrangement.camera.scale);
			CHECK(std::fabs(back.y - board.y) <= 1.0f / arrangement.camera.scale);
		}
	}
}

TEST_SUITE("linesweeper touch controls")
{
	TEST_CASE("the controls are on screen, apart, and clear of the board, both ways up")
	{
		std::array<Vector2F, 8> buffers = {};

		for (std::size_t index = 0; index < 4; ++index)
		{
			buffers[index] = portrait_buffers[index];
			buffers[index + 4] = landscape_buffers[index];
		}

		for (const Vector2F& buffer : buffers)
		{
			CAPTURE(buffer.x);
			CAPTURE(buffer.y);

			const Arrangement arrangement = arrange(buffer);
			const std::array<RectangleF, control_count> rectangles =
				control_rectangles(arrangement);

			for (std::size_t first = 0; first < rectangles.size(); ++first)
			{
				CHECK(inside(rectangles[first], arrangement.visible));
				CHECK_FALSE(overlaps(rectangles[first], board_block));

				for (std::size_t second = first + 1; second < rectangles.size();
					++second)
				{
					CHECK_FALSE(overlaps(rectangles[first], rectangles[second]));
				}
			}
		}
	}

	TEST_CASE("a finger in the middle of a control is on that control")
	{
		for (const Vector2F& buffer : { portrait_buffers[0], landscape_buffers[0],
			landscape_buffers[1] })
		{
			const Arrangement arrangement = arrange(buffer);
			const std::array<RectangleF, control_count> rectangles =
				control_rectangles(arrangement);

			for (std::size_t index = 0; index < rectangles.size(); ++index)
			{
				const std::optional<Control> found = control_at(arrangement,
					buffer_pixel(arrangement, rectangles[index].center()));

				REQUIRE(found.has_value());
				CHECK(*found == static_cast<Control>(index));
			}
		}
	}

	TEST_CASE("the left thumb moves and the right thumb turns and drops")
	{
		const Arrangement arrangement = arrange(Vector2F(1170.0f, 2532.0f));
		const std::array<RectangleF, control_count> rectangles =
			control_rectangles(arrangement);
		const float middle =
			arrangement.visible.x + arrangement.visible.width * 0.5f;

		const auto left_of_middle = [&](Control control)
		{
			const RectangleF& rectangle =
				rectangles[static_cast<std::size_t>(control)];
			return rectangle.x + rectangle.width <= middle;
		};

		CHECK(left_of_middle(Control::left));
		CHECK(left_of_middle(Control::right));
		CHECK(left_of_middle(Control::soft_drop));
		CHECK_FALSE(left_of_middle(Control::hard_drop));
		CHECK_FALSE(left_of_middle(Control::rotate_clockwise));
		CHECK_FALSE(left_of_middle(Control::rotate_anticlockwise));
		CHECK_FALSE(left_of_middle(Control::hold));
	}

	TEST_CASE("the gap between two controls and the board itself are on nothing")
	{
		const Arrangement arrangement = arrange(Vector2F(1170.0f, 2532.0f));
		const std::array<RectangleF, control_count> rectangles =
			control_rectangles(arrangement);

		const RectangleF& left =
			rectangles[static_cast<std::size_t>(Control::left)];
		const Vector2F in_gap(left.x + left.width +
			linesweeper::control_gap * 0.5f, left.y + left.height * 0.5f);

		CHECK_FALSE(control_at(arrangement,
			buffer_pixel(arrangement, in_gap)).has_value());
		CHECK_FALSE(control_at(arrangement,
			buffer_pixel(arrangement, Vector2F(640.0f, 300.0f))).has_value());
	}

	TEST_CASE("every finger counts, and one off the controls adds nothing")
	{
		const Arrangement arrangement = arrange(Vector2F(1170.0f, 2532.0f));
		const std::array<RectangleF, control_count> rectangles =
			control_rectangles(arrangement);

		const auto on = [&](Control control)
		{
			return buffer_pixel(arrangement,
				rectangles[static_cast<std::size_t>(control)].center());
		};

		const std::array<TouchPoint, 3> points = {
			point_at(1, on(Control::left)),
			point_at(2, on(Control::rotate_clockwise)),
			point_at(3, buffer_pixel(arrangement, Vector2F(640.0f, 300.0f))),
		};

		CHECK(controls_held(arrangement, points) ==
			static_cast<ControlSet>(control_bit(Control::left) |
				control_bit(Control::rotate_clockwise)));

		CHECK(controls_held(arrangement,
			std::span<const TouchPoint>(points.data() + 2, 1)) == 0);
		CHECK(controls_held(arrangement, std::span<const TouchPoint>()) == 0);
	}

	TEST_CASE("a finger that slides from one control to the next lets go of the first")
	{
		const Arrangement arrangement = arrange(Vector2F(1170.0f, 2532.0f));
		const std::array<RectangleF, control_count> rectangles =
			control_rectangles(arrangement);

		const std::array<TouchPoint, 1> before = { point_at(7, buffer_pixel(
			arrangement, rectangles[static_cast<std::size_t>(
				Control::left)].center())) };
		const std::array<TouchPoint, 1> after = { point_at(7, buffer_pixel(
			arrangement, rectangles[static_cast<std::size_t>(
				Control::right)].center())) };

		CHECK(controls_held(arrangement, before) == control_bit(Control::left));
		CHECK(controls_held(arrangement, after) == control_bit(Control::right));
	}

	TEST_CASE("each control has its own bit, and the eight fill one byte")
	{
		ControlSet all = 0;

		for (int index = 0; index < control_count; ++index)
		{
			const ControlSet bit = control_bit(static_cast<Control>(index));

			CHECK((all & bit) == 0);
			all = static_cast<ControlSet>(all | bit);
		}

		CHECK(all == 0xFF);
	}
}
