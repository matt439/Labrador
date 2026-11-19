#include "samples/components/touch_stick.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace labrador;

namespace components
{
	TouchStick::TouchStick(const RectangleF& region)
	{
		this->set_region(region);
	}

	void TouchStick::set_region(const RectangleF& region)
	{
		if (!std::isfinite(region.x) || !std::isfinite(region.y) ||
			!std::isfinite(region.width) || !std::isfinite(region.height) ||
			region.width < 0.0f || region.height < 0.0f ||
			!std::isfinite(region.right()) || !std::isfinite(region.bottom()))
		{
			throw std::invalid_argument("TouchStick: region must be finite with nonnegative extents");
		}
		const float radius = std::min(region.width, region.height) * 0.5f;
		if (region.width > 0.0f && region.height > 0.0f && radius == 0.0f)
		{
			throw std::invalid_argument("TouchStick: radius is not representable");
		}
		const double centre_x = static_cast<double>(region.x) + region.width * 0.5;
		const double centre_y = static_cast<double>(region.y) + region.height * 0.5;
		const double maximum = std::numeric_limits<float>::max();
		if (centre_x < -maximum || centre_x > maximum || centre_y < -maximum || centre_y > maximum)
		{
			throw std::invalid_argument("TouchStick: centre is not representable");
		}
		if (region != this->region_)
		{
			this->region_ = region;
			this->centre_ = Vector2F(static_cast<float>(centre_x), static_cast<float>(centre_y));
			this->radius_ = radius;
			this->cancel();
		}
	}

	RectangleF TouchStick::region() const { return this->region_; }
	Vector2F TouchStick::centre() const { return this->centre_; }
	float TouchStick::radius() const { return this->radius_; }
	std::uint32_t TouchStick::contact() const { return this->contact_; }
	Vector2F TouchStick::value() const { return this->value_; }

	Vector2F TouchStick::value_at(const Vector2F& position) const
	{
		if (!std::isfinite(position.x) || !std::isfinite(position.y))
		{
			throw std::invalid_argument("TouchStick: position must be finite");
		}
		const double x = static_cast<double>(position.x) - this->centre_.x;
		const double y = static_cast<double>(position.y) - this->centre_.y;
		const double scale = std::max(static_cast<double>(this->radius_), std::hypot(x, y));
		Vector2F value(static_cast<float>(x / scale), static_cast<float>(y / scale));
		// Narrowing a unit vector can round its length just above one.
		if (std::hypot(static_cast<double>(value.x), static_cast<double>(value.y)) > 1.0)
		{
			value.x = std::nextafter(value.x, 0.0f);
			value.y = std::nextafter(value.y, 0.0f);
		}
		return value;
	}

	bool TouchStick::begin(std::uint32_t contact, const Vector2F& position)
	{
		if (contact == 0 || this->contact_ != 0 || this->radius_ == 0.0f) return false;
		const Vector2F value = this->value_at(position);
		if (position.x < this->region_.left() || position.x >= this->region_.right() ||
			position.y < this->region_.top() || position.y >= this->region_.bottom()) return false;
		this->contact_ = contact;
		this->value_ = value;
		return true;
	}

	void TouchStick::move(std::uint32_t contact, const Vector2F& position)
	{
		if (contact == 0 || contact != this->contact_) return;
		this->value_ = this->value_at(position);
	}

	void TouchStick::end(std::uint32_t contact)
	{
		if (contact != 0 && contact == this->contact_) this->cancel();
	}

	void TouchStick::cancel()
	{
		this->contact_ = 0;
		this->value_ = Vector2F::zero;
	}
}
