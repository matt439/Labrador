#ifndef CAMERATOOLS_H
#define CAMERATOOLS_H

#include "engine/render/border_thickness.h"
#include "engine/math/matt_math.h"

namespace camera_consts
{
    constexpr float DEFAULT_BORDER_RATIO_LEFT = 0.4f;
    constexpr float DEFAULT_BORDER_RATIO_TOP = 0.2f;
    constexpr float DEFAULT_BORDER_RATIO_RIGHT = 0.4f;
    constexpr float DEFAULT_BORDER_RATIO_BOTTOM = 0.4f;

    constexpr float MIN_BORDER_LEFT = 400.0f;
    constexpr float MIN_BORDER_TOP = 250.0f;
    constexpr float MIN_BORDER_RIGHT = 400.0f;
    constexpr float MIN_BORDER_BOTTOM = 250.0f;

}

class CameraTools
{
public:
    MattMath::Camera calculate_camera(
        const MattMath::Vector2F& player_center,
        const MattMath::Vector2F& viewport_size,
        const MattMath::Camera& prev_camera,
        const MattMath::RectangleF& camera_bounds) const;
private:
    static BorderThickness calculate_camera_scroll_border(
        const MattMath::Vector2F& viewport_size);
};
#endif // !CAMERATOOLS_H