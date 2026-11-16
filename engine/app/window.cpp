#include "engine/app/window.h"

#include <stdexcept>

namespace labrador
{
	void Window::validate_cursor_shape(std::optional<CursorShape> shape)
	{
		if (!shape.has_value())
		{
			return;
		}
		switch (*shape)
		{
		case CursorShape::arrow:
		case CursorShape::hand:
		case CursorShape::text:
		case CursorShape::crosshair:
		case CursorShape::move:
		case CursorShape::not_allowed:
			return;
		}
		throw std::invalid_argument("cursor_shape must be a supported CursorShape or null.");
	}
}
