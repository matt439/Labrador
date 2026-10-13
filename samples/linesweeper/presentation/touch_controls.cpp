#include "samples/linesweeper/presentation/touch_controls.h"

#include "engine/math/rectanglei.h"
#include "engine/render/colour.h"
#include "samples/linesweeper/presentation/palette.h"

#include <cstddef>
#include <string>

using namespace mattmath;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		const std::string block_texture_name = "white";
		const std::string font_name = "jetbrains_mono_bold_16";

		// The labels, indexed by Control. ASCII, because the font's atlas is.
		const std::array<const wchar_t*, control_count> labels = {
			L"<",
			L">",
			L"DOWN",
			L"DROP",
			L"CW",
			L"CCW",
			L"HOLD",
			L"PAUSE",
		};

		// Twice the font's size, so a label reads at the size a thumb is.
		constexpr float label_scale = 2.0f;

		// Premultiplied white, faint at rest and brighter under a finger, so
		// the board shows through a button and a press is seen.
		constexpr float resting_alpha = 0.10f;
		constexpr float held_alpha = 0.28f;
		constexpr Colour label_colour(200, 208, 222);

		// Which column and row of the grid each control takes, indexed by
		// Control. Columns 0 and 1 are the left pair, 2 and 3 the right;
		// row 0 is the upper row.
		struct Cell
		{
			int column;
			int row;
		};

		constexpr std::array<Cell, control_count> cells = { {
			{ 0, 0 },	// left
			{ 1, 0 },	// right
			{ 0, 1 },	// soft_drop
			{ 2, 1 },	// hard_drop
			{ 3, 0 },	// rotate_clockwise
			{ 2, 0 },	// rotate_anticlockwise
			{ 3, 1 },	// hold
			{ 1, 1 },	// pause
		} };

		bool holds(const RectangleF& rectangle, const Vector2F& point)
		{
			return point.x >= rectangle.x && point.x < rectangle.x + rectangle.width &&
				point.y >= rectangle.y && point.y < rectangle.y + rectangle.height;
		}
	}

	// The portrait space under the board has to hold the grid; a change to
	// either side of this that breaks it is a compile error, not an overlap
	// somebody has to notice on a phone.
	static_assert(portrait_controls_height >=
		control_size * 2.0f + control_gap + control_margin * 2.0f);

	std::array<RectangleF, control_count> control_rectangles(
		const Arrangement& arrangement)
	{
		const RectangleF& visible = arrangement.visible;
		const float step = control_size + control_gap;

		// The left pair from the left edge of the screen, the right pair from
		// the right edge, so a screen wider than what was framed keeps a pair
		// under each thumb rather than both in the middle.
		const std::array<float, 4> columns = {
			visible.x + control_margin,
			visible.x + control_margin + step,
			visible.x + visible.width - control_margin - control_size - step,
			visible.x + visible.width - control_margin - control_size,
		};

		const float lower = visible.y + visible.height - control_margin -
			control_size;
		const std::array<float, 2> rows = { lower - step, lower };

		std::array<RectangleF, control_count> rectangles;

		for (std::size_t index = 0; index < cells.size(); ++index)
		{
			rectangles[index] = RectangleF(
				columns[static_cast<std::size_t>(cells[index].column)],
				rows[static_cast<std::size_t>(cells[index].row)],
				control_size, control_size);
		}

		return rectangles;
	}

	std::optional<Control> control_at(const Arrangement& arrangement,
		const Vector2I& buffer_point)
	{
		const Vector2F point = to_board(arrangement, buffer_point);
		const std::array<RectangleF, control_count> rectangles =
			control_rectangles(arrangement);

		for (std::size_t index = 0; index < rectangles.size(); ++index)
		{
			if (holds(rectangles[index], point))
			{
				return static_cast<Control>(index);
			}
		}

		return std::nullopt;
	}

	ControlSet controls_held(const Arrangement& arrangement,
		std::span<const TouchPoint> points)
	{
		ControlSet held = 0;

		for (const TouchPoint& point : points)
		{
			const std::optional<Control> control =
				control_at(arrangement, point.position);

			if (control.has_value())
			{
				held |= control_bit(*control);
			}
		}

		return held;
	}

	TouchControls::TouchControls(RenderResources* render_resources) :
		block_(render_resources->resolve_texture(block_texture_name)),
		font_(render_resources->resolve_sprite_font(font_name))
	{
		for (std::size_t index = 0; index < labels.size(); ++index)
		{
			this->label_sizes_[index] =
				render_resources->measure_text(this->font_, labels[index]) *
				label_scale;
		}
	}

	void TouchControls::set(const Arrangement& arrangement, ControlSet held,
		bool shown)
	{
		this->rectangles_ = control_rectangles(arrangement);
		this->visible_ = arrangement.visible;
		this->held_ = held;
		this->shown_ = shown;
	}

	void TouchControls::update(float /*dt*/)
	{

	}

	void TouchControls::draw(DrawList& draw_list) const
	{
		if (!this->shown_)
		{
			return;
		}

		for (std::size_t index = 0; index < this->rectangles_.size(); ++index)
		{
			const RectangleF& rectangle = this->rectangles_[index];
			const bool held = (this->held_ &
				control_bit(static_cast<Control>(index))) != 0;

			draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1),
				rectangle,
				faded(Colour(1.0f, 1.0f, 1.0f),
					held ? held_alpha : resting_alpha),
				0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);

			const Vector2F& size = this->label_sizes_[index];

			draw_list.draw_text(this->font_, labels[index],
				Vector2F(rectangle.x + (rectangle.width - size.x) * 0.5f,
					rectangle.y + (rectangle.height - size.y) * 0.5f),
				label_colour, label_scale, 0.0f, Vector2F::ZERO, 0.0f);
		}
	}

	RectangleF TouchControls::bounds() const
	{
		return this->shown_ ? this->visible_ : RectangleF();
	}
}
