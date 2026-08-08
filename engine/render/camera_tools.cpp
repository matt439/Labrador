#include "engine/render/camera_tools.h"

#include <algorithm>

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
	namespace
	{
		// The scroll border, as a fraction of the viewport and with a floor in
		// pixels. Private to the one function that reads them: these tune how the
		// camera chases a player and configure nothing else (CONVENTIONS,
		// Constants and enumerators).
		constexpr float DEFAULT_BORDER_RATIO_LEFT = 0.4f;
		constexpr float DEFAULT_BORDER_RATIO_TOP = 0.2f;
		constexpr float DEFAULT_BORDER_RATIO_RIGHT = 0.4f;
		constexpr float DEFAULT_BORDER_RATIO_BOTTOM = 0.4f;

		constexpr float MIN_BORDER_LEFT = 400.0f;
		constexpr float MIN_BORDER_TOP = 250.0f;
		constexpr float MIN_BORDER_RIGHT = 400.0f;
		constexpr float MIN_BORDER_BOTTOM = 250.0f;
	}

	Camera CameraTools::calculate_camera(
		const Vector2F& player_center,
		const Vector2F& viewport_size,
		const Camera& prev_camera,
		const RectangleF& camera_bounds) const
	{
		BorderThickness scroll_border =
			this->calculate_camera_scroll_border(viewport_size);

		float left_edge = prev_camera.translation.x + scroll_border.left;
		float right_edge = prev_camera.translation.x + viewport_size.x - scroll_border.right;
		float top_edge = prev_camera.translation.y + scroll_border.top;
		float bottom_edge = prev_camera.translation.y + viewport_size.y - scroll_border.bottom;


		Camera new_cam = prev_camera;
		// Test top/bottom of screen
		if (player_center.y < top_edge)
		{
			new_cam.translation.y += player_center.y - top_edge;
		}
		else if (player_center.y > bottom_edge)
		{
			new_cam.translation.y += player_center.y - bottom_edge;
		}
		// Test left/right of screen
		if (player_center.x < left_edge)
		{
			new_cam.translation.x += player_center.x - left_edge;
		}
		else if (player_center.x > right_edge)
		{
			new_cam.translation.x += player_center.x - right_edge;
		}

		// Test if camera is outside of camera bounds
		RectangleF camera_rectangle = RectangleF(new_cam.translation.x,
			new_cam.translation.y,
			viewport_size.x, viewport_size.y);

		if (camera_bounds.contains(camera_rectangle))
		{
			return new_cam;
		}
		else
		{
			if (camera_rectangle.left() < camera_bounds.left())
			{
				new_cam.translation.x += camera_bounds.left() - camera_rectangle.left();
			}
			else if (camera_rectangle.right() > camera_bounds.right())
			{
				new_cam.translation.x += camera_bounds.right() - camera_rectangle.right();
			}
			if (camera_rectangle.top() < camera_bounds.top())
			{
				new_cam.translation.y += camera_bounds.top() - camera_rectangle.top();
			}
			else if (camera_rectangle.bottom() > camera_bounds.bottom())
			{
				new_cam.translation.y += camera_bounds.bottom() - camera_rectangle.bottom();
			}
			return new_cam;
		}
	}

	BorderThickness CameraTools::calculate_camera_scroll_border(
		const Vector2F& viewport_size)
	{
		BorderThickness result =
		{
			viewport_size.x * DEFAULT_BORDER_RATIO_LEFT,
			viewport_size.y * DEFAULT_BORDER_RATIO_TOP,
			viewport_size.x * DEFAULT_BORDER_RATIO_RIGHT,
			viewport_size.y * DEFAULT_BORDER_RATIO_BOTTOM
		};

		// Half the viewport is a hard geometric limit and the pixel floor is a
		// preference, so the limit is applied last and wins. It has to: with
		// opposing borders each capped at half, left + right can never exceed
		// the width and top + bottom can never exceed the height, which is the
		// invariant calculate_camera() reads them under - it derives
		// left_edge/right_edge and top_edge/bottom_edge from them and assumes
		// the first of each pair is the smaller.
		//
		// clamp_ref() cannot express this. It takes the min branch first, so
		// with a floor above the ceiling the ceiling is never consulted and
		// the floor is returned unchanged. At 1280x720 two-player - the
		// default resolution, and what an unparseable save file coerces to -
		// each pane is 1280x360, both vertical floors are 250 against a
		// ceiling of 180, and the result was top_edge sitting 140 px *below*
		// bottom_edge. A motionless player then satisfied both branches in
		// turn, every frame, forever.
		result.left = std::min(std::max(result.left, MIN_BORDER_LEFT),
			viewport_size.x / 2.0f);
		result.right = std::min(std::max(result.right, MIN_BORDER_RIGHT),
			viewport_size.x / 2.0f);
		result.top = std::min(std::max(result.top, MIN_BORDER_TOP),
			viewport_size.y / 2.0f);
		result.bottom = std::min(std::max(result.bottom, MIN_BORDER_BOTTOM),
			viewport_size.y / 2.0f);

		return result;
	}
}
