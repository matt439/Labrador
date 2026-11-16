#include "samples/components/shape_outline.h"

#include "engine/math/circle.h"
#include "engine/math/quad.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/triangle.h"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace labrador;

namespace components
{
	namespace
	{
		bool finite(const Vector2F& point)
		{
			return std::isfinite(point.x) && std::isfinite(point.y);
		}

		template <typename T>
		const T& concrete(const Shape& shape)
		{
			const T* typed = dynamic_cast<const T*>(&shape);
			if (typed == nullptr)
			{
				throw std::invalid_argument("ShapeOutline: shape tag does not match its type");
			}
			return *typed;
		}

		Vector2F checked_point(double x, double y)
		{
			const double largest = std::numeric_limits<float>::max();
			if (!std::isfinite(x) || !std::isfinite(y) ||
				x < -largest || x > largest || y < -largest || y > largest)
			{
				throw std::invalid_argument("ShapeOutline: boundary is not finite in float coordinates");
			}
			return Vector2F(static_cast<float>(x), static_cast<float>(y));
		}

		std::size_t polygon_edges(std::span<const Vector2F> points,
			std::span<Segment> output)
		{
			if (output.size() < points.size())
			{
				throw std::length_error("ShapeOutline: output cannot hold the boundary");
			}
			for (const Vector2F& point : points)
			{
				if (!finite(point))
				{
					throw std::invalid_argument("ShapeOutline: boundary contains a nonfinite point");
				}
			}
			for (std::size_t index = 0; index < points.size(); ++index)
			{
				output[index] = Segment(points[index], points[(index + 1) % points.size()]);
			}
			return points.size();
		}

		struct Stroke
		{
			Vector2F start;
			float length = 0.0f;
			float rotation = 0.0f;
		};
	}

	std::size_t outline_edges(const Shape& shape, std::span<Segment> output,
		int circle_segments)
	{
		switch (shape.shape_type())
		{
		case ShapeType::rectangle:
		{
			const RectangleF& rectangle = concrete<RectangleF>(shape);
			const std::array<Vector2F, 4> points = { rectangle.top_left(),
				rectangle.top_right(), rectangle.bottom_right(), rectangle.bottom_left() };
			return polygon_edges(points, output);
		}
		case ShapeType::rectangle_rotated:
			return polygon_edges(concrete<RectangleRotated>(shape).points(), output);
		case ShapeType::quad:
			return polygon_edges(concrete<Quad>(shape).points(), output);
		case ShapeType::triangle:
		{
			const Triangle& triangle = concrete<Triangle>(shape);
			return polygon_edges(triangle.points, output);
		}
		case ShapeType::circle:
		{
			const Circle& circle = concrete<Circle>(shape);
			if (!circle.is_valid() || circle_segments < 3 || circle_segments > max_outline_segments)
			{
				throw std::invalid_argument("ShapeOutline: circle needs a valid radius/centre and 3..1024 segments");
			}
			if (circle.radius() == 0.0f)
			{
				return 0;
			}
			const std::size_t count = static_cast<std::size_t>(circle_segments);
			if (output.size() < count)
			{
				throw std::length_error("ShapeOutline: output cannot hold the circle boundary");
			}
			const Vector2F centre = circle.center();
			const double radius = circle.radius();
			// Check the entire circle's extrema, including between its chords.
			checked_point(centre.x - radius, centre.y - radius);
			checked_point(centre.x + radius, centre.y + radius);
			std::array<Vector2F, max_outline_segments> points;
			for (std::size_t index = 0; index < count; ++index)
			{
				const double angle = 2.0 * std::numbers::pi * static_cast<double>(index) /
					static_cast<double>(count);
				points[index] = checked_point(centre.x + radius * std::cos(angle),
					centre.y + radius * std::sin(angle));
			}
			return polygon_edges(std::span<const Vector2F>(points.data(), count), output);
		}
		default:
			throw std::invalid_argument("ShapeOutline: unsupported shape type");
		}
	}

	ShapeOutline::ShapeOutline(TextureHandle white) : white_(white)
	{
	}

	void ShapeOutline::draw(DrawList& list, const Shape& shape,
		float thickness, const Colour& colour, int circle_segments) const
	{
		if (!(thickness > 0.0f) || !std::isfinite(thickness))
		{
			throw std::invalid_argument("ShapeOutline: thickness must be finite and positive");
		}
		std::array<Segment, max_outline_segments> edges;
		const std::size_t edge_count = outline_edges(shape, edges, circle_segments);
		std::array<Stroke, max_outline_segments> strokes;
		std::size_t count = 0;
		for (std::size_t index = 0; index < edge_count; ++index)
		{
			const Segment& edge = edges[index];
			const double x = static_cast<double>(edge.point_1.x) - edge.point_0.x;
			const double y = static_cast<double>(edge.point_1.y) - edge.point_0.y;
			const double length = std::hypot(x, y);
			if (length == 0.0)
			{
				continue;
			}
			if (length > std::numeric_limits<float>::max())
			{
				throw std::invalid_argument("ShapeOutline: stroke length is not finite in float coordinates");
			}
			// Sprite quantization consumes this unrotated destination first.
			checked_point(edge.point_0.x + length,
				static_cast<double>(edge.point_0.y) + thickness);
			const double half_thickness = static_cast<double>(thickness) * 0.5;
			const double normal_x = -y / length * half_thickness;
			const double normal_y = x / length * half_thickness;
			for (const Vector2F& end : { edge.point_0, edge.point_1 })
			{
				checked_point(end.x + normal_x, end.y + normal_y);
				checked_point(end.x - normal_x, end.y - normal_y);
			}
			strokes[count++] = { edge.point_0, static_cast<float>(length),
				static_cast<float>(std::atan2(y, x)) };
		}
		for (std::size_t index = 0; index < count; ++index)
		{
			const Stroke& stroke = strokes[index];
			list.draw_sprite(this->white_, RectangleI(0, 0, 1, 1),
				RectangleF(stroke.start.x, stroke.start.y, stroke.length, thickness),
				colour, stroke.rotation, Vector2F(0.0f, 0.5f), SpriteFlip::none, 0.0f);
		}
	}
}
