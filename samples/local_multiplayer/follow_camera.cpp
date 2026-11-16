#include "samples/local_multiplayer/follow_camera.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace labrador;

namespace multiplayer
{
	namespace
	{
		bool finite_rectangle(const RectangleF& rectangle)
		{
			return std::isfinite(rectangle.x) && std::isfinite(rectangle.y) &&
				std::isfinite(rectangle.width) && std::isfinite(rectangle.height) &&
				std::isfinite(rectangle.right()) && std::isfinite(rectangle.bottom());
		}

		float follow_axis(float translation, float previous_size, bool initialized,
			float target, float size, float zone_start, float zone_end,
			float world_start, float world_size)
		{
			// Wider temporaries keep a finite target/extent subtraction from
			// overflowing before the world clamp can bound the answer.
			double next = initialized ? static_cast<double>(translation) +
				(static_cast<double>(previous_size) - size) * 0.5 :
				static_cast<double>(target) - static_cast<double>(size) * 0.5;
			const double local_target = static_cast<double>(target) - next;
			if (local_target < zone_start)
			{
				next = static_cast<double>(target) - zone_start;
			}
			else if (local_target > zone_end)
			{
				next = static_cast<double>(target) - zone_end;
			}

			if (world_size <= size)
			{
				next = static_cast<double>(world_start) +
					(static_cast<double>(world_size) - size) * 0.5;
			}
			else
			{
				const double last = static_cast<double>(world_start) +
					(static_cast<double>(world_size) - size);
				next = std::clamp(next, static_cast<double>(world_start), last);
			}
			const double maximum = std::numeric_limits<float>::max();
			if (!std::isfinite(next) || next < -maximum || next > maximum)
			{
				throw std::invalid_argument("FollowCamera::update: translation is not representable");
			}
			return static_cast<float>(next);
		}
	}

	void FollowCamera::update(const Vector2F& target, const Vector2F& viewport_size,
		const RectangleF& dead_zone, const RectangleF& world)
	{
		if (!std::isfinite(target.x) || !std::isfinite(target.y) ||
			!std::isfinite(viewport_size.x) || !std::isfinite(viewport_size.y) ||
			viewport_size.x <= 0.0f || viewport_size.y <= 0.0f ||
			!finite_rectangle(dead_zone) || !finite_rectangle(world) ||
			world.width < 0.0f || world.height < 0.0f ||
			dead_zone.width < 0.0f || dead_zone.height < 0.0f ||
			dead_zone.left() < 0.0f || dead_zone.top() < 0.0f ||
			dead_zone.right() > viewport_size.x || dead_zone.bottom() > viewport_size.y)
		{
			throw std::invalid_argument("FollowCamera::update: invalid target, viewport, dead zone or world");
		}
		const Vector2F next(
			follow_axis(this->translation_.x, this->viewport_size_.x, this->initialized_,
				target.x, viewport_size.x, dead_zone.left(), dead_zone.right(), world.x, world.width),
			follow_axis(this->translation_.y, this->viewport_size_.y, this->initialized_,
				target.y, viewport_size.y, dead_zone.top(), dead_zone.bottom(), world.y, world.height));
		this->translation_ = next;
		this->viewport_size_ = viewport_size;
		this->initialized_ = true;
	}

	Camera FollowCamera::camera() const
	{
		return Camera(this->translation_, 1.0f);
	}
}
