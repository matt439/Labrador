#pragma once

#include "engine/render/border_thickness.h"
#include "engine/math/matt_math.h"

namespace artattack
{
    class CameraTools
    {
    public:
        mattmath::Camera calculate_camera(
            const mattmath::Vector2F& player_center,
            const mattmath::Vector2F& viewport_size,
            const mattmath::Camera& prev_camera,
            const mattmath::RectangleF& camera_bounds) const;
    private:
        static BorderThickness calculate_camera_scroll_border(
            const mattmath::Vector2F& viewport_size);
    };
}