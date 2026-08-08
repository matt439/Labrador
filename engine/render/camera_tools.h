#pragma once

#include "engine/render/border_thickness.h"
#include "engine/math/matt_math.h"
#include "engine/render/camera.h"

namespace artattack
{
    class CameraTools
    {
    public:
        Camera calculate_camera(
            const mattmath::Vector2F& player_center,
            const mattmath::Vector2F& viewport_size,
            const Camera& prev_camera,
            const mattmath::RectangleF& camera_bounds) const;

        // The dead zone the player moves inside before the camera follows,
        // for a viewport this size. Public because it is the whole of the
        // camera model that can be checked without simulating one, and it
        // carries an invariant worth checking directly: opposing borders never
        // sum past the viewport, or calculate_camera()'s near edge ends up
        // beyond its far edge.
        static BorderThickness calculate_camera_scroll_border(
            const mattmath::Vector2F& viewport_size);
    };
}