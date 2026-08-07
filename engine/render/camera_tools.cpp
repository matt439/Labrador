#include "engine/render/camera_tools.h"

using namespace DirectX;
using namespace camera_consts;
using namespace mattmath;

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
		if (camera_rectangle.get_left() < camera_bounds.get_left())
		{
			new_cam.translation.x += camera_bounds.get_left() - camera_rectangle.get_left();
		}
		else if (camera_rectangle.get_right() > camera_bounds.get_right())
		{
			new_cam.translation.x += camera_bounds.get_right() - camera_rectangle.get_right();
		}
		if (camera_rectangle.get_top() < camera_bounds.get_top())
		{
			new_cam.translation.y += camera_bounds.get_top() - camera_rectangle.get_top();
		}
		else if (camera_rectangle.get_bottom() > camera_bounds.get_bottom())
		{
			new_cam.translation.y += camera_bounds.get_bottom() - camera_rectangle.get_bottom();
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

	clamp_ref(result.left, MIN_BORDER_LEFT, viewport_size.x / 2.0f);
	clamp_ref(result.right, MIN_BORDER_RIGHT, viewport_size.x / 2.0f);
	clamp_ref(result.top, MIN_BORDER_TOP, viewport_size.y / 2.0f);
	clamp_ref(result.bottom, MIN_BORDER_BOTTOM, viewport_size.y / 2.0f);

	return result;
}
