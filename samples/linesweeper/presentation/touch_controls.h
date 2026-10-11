#pragma once

#include "engine/core/game_object.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "samples/linesweeper/presentation/arrangement.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

// The buttons a touchscreen plays with, drawn by the game over the board.
//
// A THIRD DEVICE IS A THIRD TABLE, which is what README, Still open, said the
// pad's pricing had settled: the keyboard has {Key, button}, the pad has
// {GamepadButton, button}, and the glass has these eight rectangles and the
// {Control, button} table in play_state.cpp. The engine gained a touch device
// and no binding layer (docs/port/web.md, P2), so what a finger on a rectangle
// means is decided here, in the sample, the same way a key's is.
//
// THIS FILE DOES NOT KNOW THE RULES' BUTTONS. presentation/ may not include
// rules/tick.h, where the button bits live (README, Three layers), so a
// control is named for what it is on the screen and states/ maps it onto the
// byte tick() takes. Pause is a control and not a rules button, for the reason
// play_state.cpp gives: it suspends the match rather than stepping it.
//
// HELD, NOT TAPPED, for the reason the keyboard is read as held: the byte
// handed to tick() is what is down, and tick() derives every press and every
// auto-repeat from it. A finger resting on LEFT is a held left arrow, with the
// same delay and the same repeat. A finger that slides from one button to the
// next lets go of the first and presses the second, because each frame asks
// which rectangle every point is in now.
namespace linesweeper
{
	enum class Control : std::uint8_t
	{
		left,
		right,
		soft_drop,
		hard_drop,
		rotate_clockwise,
		rotate_anticlockwise,
		hold,
		pause,
	};

	inline constexpr int control_count = 8;

	// One bit per control, as tick.h has one bit per button.
	using ControlSet = std::uint8_t;

	constexpr ControlSet control_bit(Control control)
	{
		return static_cast<ControlSet>(1u << static_cast<unsigned int>(control));
	}

	// The grid, in board space. Two rows of square buttons anchored to the
	// bottom of what is visible, a pair at each side: the left thumb moves
	// and the right thumb turns and drops. Square and this size because it is
	// a thumb's width on a phone held upright - 136 of the 620 board units
	// portrait shows across is about a fifth of the screen's width.
	inline constexpr float control_size = 136.0f;
	inline constexpr float control_gap = 12.0f;
	inline constexpr float control_margin = 16.0f;

	// Where each control is for this arrangement, indexed by Control.
	std::array<mattmath::RectangleF, control_count> control_rectangles(
		const Arrangement& arrangement);

	// The control under a buffer pixel, if any. A rectangle holds its top and
	// left edges and not its bottom and right, so two that share an edge
	// cannot both answer.
	std::optional<Control> control_at(const Arrangement& arrangement,
		const mattmath::Vector2I& buffer_point);

	// Every control a point is on this frame.
	ControlSet controls_held(const Arrangement& arrangement,
		std::span<const labrador::TouchPoint> points);

	// The controls, drawn. One object for the eight, for the reason
	// board_view.h is one object for five panels: they are one read of one
	// value, and that value is handed in by the state each tick.
	class TouchControls final : public labrador::GameObject
	{
	public:
		// Borrowed for the constructor only: the labels are measured here,
		// once, and the handles resolved once (T7, T8).
		explicit TouchControls(labrador::RenderResources* render_resources);

		// What to draw: where, which buttons a finger is on, and whether the
		// controls are shown at all. A keyboard player never sees them; they
		// appear the first frame a finger touches the screen and stay.
		//
		// A setter the state calls from its update(), because draw() is const
		// and update() is where a value is allowed to change.
		void set(const Arrangement& arrangement, ControlSet held, bool shown);

		void update(float dt) override;
		void draw(labrador::DrawList& draw_list) const override;

		// What is visible while the controls are shown, and nothing while
		// they are not.
		mattmath::RectangleF bounds() const override;

	private:
		labrador::TextureHandle block_;
		labrador::FontHandle font_;

		std::array<mattmath::Vector2F, control_count> label_sizes_ = {};
		std::array<mattmath::RectangleF, control_count> rectangles_ = {};
		mattmath::RectangleF visible_;
		ControlSet held_ = 0;
		bool shown_ = false;
	};
}
