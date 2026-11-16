#include "engine/render/text_alignment.h"

#include <cmath>
#include <stdexcept>

namespace labrador
{
	namespace
	{
		bool finite_nonnegative(float value)
		{
			return value >= 0.0f && std::isfinite(value);
		}

		Vector2F alignment_fraction(TextAlignment alignment)
		{
			switch (alignment)
			{
			case TextAlignment::top_left: return Vector2F(0.0f, 0.0f);
			case TextAlignment::top_centre: return Vector2F(0.5f, 0.0f);
			case TextAlignment::top_right: return Vector2F(1.0f, 0.0f);
			case TextAlignment::centre_left: return Vector2F(0.0f, 0.5f);
			case TextAlignment::centre: return Vector2F(0.5f, 0.5f);
			case TextAlignment::centre_right: return Vector2F(1.0f, 0.5f);
			case TextAlignment::bottom_left: return Vector2F(0.0f, 1.0f);
			case TextAlignment::bottom_centre: return Vector2F(0.5f, 1.0f);
			case TextAlignment::bottom_right: return Vector2F(1.0f, 1.0f);
			default:
				throw std::invalid_argument("aligned_text_position: unknown alignment");
			}
		}
	}

	Vector2F aligned_text_position(const Vector2F& measured_size,
		const RectangleF& anchor, TextAlignment alignment, TextSnapping snapping,
		float scale)
	{
		if (!finite_nonnegative(measured_size.x) ||
			!finite_nonnegative(measured_size.y) ||
			!finite_nonnegative(anchor.width) || !finite_nonnegative(anchor.height) ||
			!std::isfinite(anchor.x) || !std::isfinite(anchor.y) ||
			!(scale > 0.0f) || !std::isfinite(scale))
		{
			throw std::invalid_argument(
				"aligned_text_position: sizes must be finite and nonnegative, "
				"positions finite, and scale finite and positive");
		}
		const Vector2F fraction = alignment_fraction(alignment);
		const Vector2F size = measured_size * scale;
		if (!std::isfinite(size.x) || !std::isfinite(size.y))
		{
			throw std::invalid_argument("aligned_text_position: scaled size is not finite");
		}
		Vector2F position(anchor.x + (anchor.width - size.x) * fraction.x,
			anchor.y + (anchor.height - size.y) * fraction.y);
		if (!std::isfinite(position.x) || !std::isfinite(position.y))
		{
			throw std::invalid_argument("aligned_text_position: position is not finite");
		}
		switch (snapping)
		{
		case TextSnapping::none:
			return position;
		case TextSnapping::floor:
			return Vector2F(std::floor(position.x), std::floor(position.y));
		default:
			throw std::invalid_argument("aligned_text_position: unknown snapping");
		}
	}

	Vector2F aligned_text_position(const Vector2F& measured_size,
		const Vector2F& anchor, TextAlignment alignment, TextSnapping snapping,
		float scale)
	{
		return aligned_text_position(measured_size,
			RectangleF(anchor, Vector2F::zero), alignment, snapping, scale);
	}
}
