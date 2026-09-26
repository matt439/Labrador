#include "engine/collision/narrow_phase.h"
#include "engine/collision/resolve.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/intersects.h"
#include "engine/math/circle.h"
#include <cstdio>
#include <cmath>
#include <limits>

int main() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const mattmath::RectangleF good(0.0f, 0.0f, 20.0f, 20.0f);
    const mattmath::RectangleF poison(5.0f, 5.0f, nan, 10.0f);
    const std::optional<labrador::Manifold> contact = labrador::narrow_phase(poison, good);
    std::printf("NaN width rectangle has contact=%d; penetration=%g\n",
        contact.has_value() ? 1 : 0, contact ? static_cast<double>(contact->penetration) : 0.0);
    const mattmath::Vector2F movement = labrador::separation_along(
        mattmath::Vector2F(nan, 1.0f), 1.0f, mattmath::Vector2F(0.0f, 1.0f));
    std::printf("NaN normal accepted; movement finite=%d\n",
        std::isfinite(movement.x) && std::isfinite(movement.y) ? 1 : 0);
    const mattmath::Vector2F x_axis(1.0f, 0.0f);
    const mattmath::Vector2F y_axis(0.0f, 1.0f);
    const mattmath::RectangleRotated first(mattmath::Vector2F(0.0f, 0.0f), x_axis, y_axis,
        mattmath::Vector2F(0.0001f, 1.0f));
    const mattmath::RectangleRotated second(mattmath::Vector2F(0.0f, 100.0f), x_axis, y_axis,
        mattmath::Vector2F(0.0001f, 1.0f));
    std::printf("Separated thin rectangles: boxes overlap=%d; narrow contact=%d\n",
        first.bounding_box().intersects(second.bounding_box()) ? 1 : 0,
        labrador::narrow_phase(first, second).has_value() ? 1 : 0);
    mattmath::Point2F closest;
    const mattmath::Circle circle(nan, 10.0f, 1.0f);
    std::printf("NaN circle/AABB boolean=%d versus contact overload=%d\n",
        mattmath::rectangle_circle_intersect(good, circle) ? 1 : 0,
        mattmath::rectangle_circle_intersect(good, circle, closest) ? 1 : 0);
}
