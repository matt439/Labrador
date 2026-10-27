#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/viewport.h"

// Which way up the screen is, and how the board's coordinates reach it.
//
// EVERYTHING ELSE IN presentation/ DRAWS IN ONE SPACE: the 1280x720 that
// main.cpp asks for, with the well at layout.h's origin and the side columns
// at board_view.cpp's. Nothing below moves a panel. What changes with the
// buffer is the camera the state hands the scene, and that is the whole of
// both layouts:
//
// - LANDSCAPE frames that 1280x720 in the buffer, fitted and centred. At
//   exactly 1280x720 the camera is the identity - a scale of one and no
//   translation, both exact in floating point - so the window main.cpp opens
//   draws byte for byte what it drew before there was a camera here, and the
//   capture tool's images are the evidence.
// - PORTRAIT frames only the board block - HOLD and the score on the left,
//   the well, NEXT on the right - fitted to the buffer's WIDTH, and leaves
//   the height below it for the touch controls. The block is about as wide as
//   it is tall, so a tall buffer takes it with room underneath for two rows
//   of thumb-sized buttons; a second arrangement of five panels would have
//   bought a slightly larger well for twice the layout code (T3).
//
// The choice follows the buffer's aspect and nothing else: taller than wide is
// portrait. It is recomputed every tick from the size the shell reports, so a
// phone turned on its side, a resized browser and a dragged window edge all
// land in the arrangement that fits on the next frame.
namespace linesweeper
{
	enum class Orientation
	{
		landscape,
		portrait,
	};

	// The landscape screen, in the space everything is drawn in.
	inline constexpr float landscape_width = 1280.0f;
	inline constexpr float landscape_height = 720.0f;

	// The board block that portrait frames, in that same space. Centred on the
	// well (x 500 to 780) rather than on the block, which reaches 251 to the
	// right of the well's middle and 299 to the left, because the well is what
	// a player looks at; the HOLD column sits nearer the edge than NEXT.
	inline constexpr float portrait_left = 330.0f;
	inline constexpr float portrait_width = 620.0f;

	// From above the HOLD and NEXT labels to below the well's frame.
	inline constexpr float portrait_board_top = 36.0f;
	inline constexpr float portrait_board_bottom = 652.0f;

	// The space under the board the controls need: two rows of buttons and
	// the margins round them (touch_controls.h).
	inline constexpr float portrait_controls_height = 316.0f;

	// The least height portrait shows. A buffer squatter than this, at the
	// width above, is fitted to its height instead and the block is centred
	// across.
	inline constexpr float portrait_min_height =
		portrait_board_bottom - portrait_board_top + portrait_controls_height;

	struct Arrangement
	{
		Orientation orientation = Orientation::landscape;

		// The whole buffer, which is the one view this sample draws.
		labrador::Viewport viewport;

		// Board space to buffer pixels.
		labrador::Camera camera;

		// The board-space rectangle the whole buffer shows. Wider or taller
		// than what was framed whenever the aspects differ; anything anchored
		// to an edge of the screen - the scrim, the controls - is anchored to
		// this.
		labrador::RectangleF visible;
	};

	// The arrangement for a buffer of this size, in pixels. Throws
	// std::invalid_argument for a non-positive extent, as Camera::frame does.
	Arrangement arrange(const labrador::Vector2F& buffer);
}
