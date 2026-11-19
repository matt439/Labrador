#include "samples/components/touch_button.h"

#include <cmath>
#include <stdexcept>

using namespace labrador;

namespace components
{
	namespace
	{
		bool inside_region(const RectangleF& region, const Vector2F& position)
		{
			return position.x >= region.left() && position.x < region.right() &&
				position.y >= region.top() && position.y < region.bottom();
		}

		void require_position(const Vector2F& position)
		{
			if (!std::isfinite(position.x) || !std::isfinite(position.y))
			{
				throw std::invalid_argument("TouchButton: position must be finite");
			}
		}
	}

	TouchButton::TouchButton(const RectangleF& region)
	{
		this->set_region(region);
	}

	void TouchButton::set_region(const RectangleF& region)
	{
		if (!std::isfinite(region.x) || !std::isfinite(region.y) ||
			!std::isfinite(region.width) || !std::isfinite(region.height) ||
			region.width < 0.0f || region.height < 0.0f ||
			!std::isfinite(region.right()) || !std::isfinite(region.bottom()))
		{
			throw std::invalid_argument("TouchButton: region must be finite with nonnegative extents");
		}
		if (region != this->region_)
		{
			this->region_ = region;
			this->cancel();
		}
	}

	RectangleF TouchButton::region() const { return this->region_; }
	std::uint32_t TouchButton::contact() const { return this->contact_; }
	bool TouchButton::held() const { return this->held_; }

	bool TouchButton::begin(std::uint32_t contact, const Vector2F& position)
	{
		if (contact == 0 || this->contact_ != 0 ||
			this->region_.width == 0.0f || this->region_.height == 0.0f)
		{
			return false;
		}
		require_position(position);
		if (!inside_region(this->region_, position)) return false;
		this->contact_ = contact;
		this->held_ = true;
		return true;
	}

	void TouchButton::move(std::uint32_t contact, const Vector2F& position)
	{
		if (contact == 0 || contact != this->contact_) return;
		require_position(position);
		this->held_ = inside_region(this->region_, position);
	}

	void TouchButton::end(std::uint32_t contact)
	{
		if (contact != 0 && contact == this->contact_) this->cancel();
	}

	void TouchButton::cancel()
	{
		this->contact_ = 0;
		this->held_ = false;
	}
}
