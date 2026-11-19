#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <cstdint>

namespace components
{
	// A fixed-centre virtual stick, driven by ids and converted positions from
	// page-supplied TouchPoints. It reads no device and chooses no action binding.
	class TouchStick
	{
	public:
		TouchStick() = default;
		explicit TouchStick(const labrador::RectangleF& region);

		// The half-open rectangle accepts new presses. Centre is its midpoint;
		// radius is half its shorter side. Empty geometry disables the control.
		// Finite nonnegative geometry/endpoints and a representable positive
		// radius for a nonempty region and representable centre are required,
		// else invalid_argument leaves
		// state unchanged. Different geometry cancels; identical geometry does not.
		void set_region(const labrador::RectangleF& region);
		labrador::RectangleF region() const;
		labrador::Vector2F centre() const;
		float radius() const;

		// The page calls begin only for a fresh accepted press. Zero ids, a busy
		// stick and points outside the region return false. A candidate position
		// must be finite or invalid_argument leaves state unchanged.
		bool begin(std::uint32_t contact, const labrador::Vector2F& position);
		// Follow the same contact outside the region, radially clamped to unit
		// length. +y points down. Foreign ids are ignored. A matching position
		// must be finite or invalid_argument leaves state unchanged.
		void move(std::uint32_t contact, const labrador::Vector2F& position);
		void end(std::uint32_t contact);
		// Cancellation/focus/resize policy belongs to the page. Returning to
		// neutral never adopts another held finger; capture needs a new begin.
		void cancel();
		std::uint32_t contact() const;
		labrador::Vector2F value() const;

	private:
		labrador::Vector2F value_at(const labrador::Vector2F& position) const;
		labrador::RectangleF region_;
		labrador::Vector2F centre_ = labrador::Vector2F::zero;
		float radius_ = 0.0f;
		std::uint32_t contact_ = 0;
		labrador::Vector2F value_ = labrador::Vector2F::zero;
	};
}
