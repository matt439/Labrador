#include "samples/linesweeper/presentation/arrangement.h"

using namespace mattmath;
using namespace labrador;

namespace linesweeper
{
	Arrangement arrange(const Vector2F& buffer)
	{
		Arrangement arrangement;
		arrangement.orientation = buffer.y > buffer.x
			? Orientation::portrait
			: Orientation::landscape;
		arrangement.viewport = Viewport(0.0f, 0.0f, buffer.x, buffer.y);

		// ONE FRAMING IN BOTH CASES, and Camera::frame does all of it. It fits
		// whichever axis is tighter and splits the slack evenly on the other.
		// For portrait that is the right split without being asked: the
		// controls are anchored to the bottom of what is visible, so half the
		// slack lands above the board and half between the board and the
		// controls, which centres the board in the space they leave.
		const RectangleF framed = arrangement.orientation == Orientation::portrait
			? RectangleF(portrait_left, portrait_board_top, portrait_width,
				portrait_min_height)
			: RectangleF(0.0f, 0.0f, landscape_width, landscape_height);

		arrangement.camera = Camera::frame(framed, arrangement.viewport);
		arrangement.visible =
			arrangement.camera.visible_rectangle(arrangement.viewport);

		return arrangement;
	}

	Vector2F to_board(const Arrangement& arrangement,
		const Vector2I& buffer_point)
	{
		// The inverse of view = (world - translation) * scale, with the one
		// view's viewport at the buffer's origin.
		const Camera& camera = arrangement.camera;

		return Vector2F(
			static_cast<float>(buffer_point.x) / camera.scale +
				camera.translation.x,
			static_cast<float>(buffer_point.y) / camera.scale +
				camera.translation.y);
	}
}
