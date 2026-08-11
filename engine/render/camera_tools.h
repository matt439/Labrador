#pragma once

#include "engine/render/border_thickness.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"

namespace artattack
{
    class CameraTools
    {
    public:
        // `viewport_size` is in WORLD units, not pixels, and that distinction
        // is the whole contract. Every line of the body adds it to
        // prev_camera.translation, which is a world position, and compares the
        // result against camera_bounds, which is a world rectangle. The
        // function never reads prev_camera.scale and must not: the scale is
        // already spent by the time the size gets here.
        //
        // So a caller with a zoomed camera passes
        // camera.visible_rectangle(viewport).size(), not viewport.size().
        // Passing pixels is correct only at scale 1, silently and everywhere -
        // the borders, the edges and the bounds test all drift together, so
        // the camera simply chases wrong without ever looking broken.
        //
        // Written down rather than fixed because there is nothing here to fix:
        // the arithmetic is right for its stated input. What was missing was
        // the sentence saying which input that is.
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