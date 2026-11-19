#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <cstdint>

namespace components
{
	// One held touch rectangle. The page supplies a TouchPoint's stable id
	// and its position converted into the region's coordinate space. The page
	// chooses fresh presses, overlap precedence, focus and action bindings.
	class TouchButton
	{
	public:
		TouchButton() = default;
		explicit TouchButton(const labrador::RectangleF& region);

		// Finite, nonnegative extents and finite endpoints are required;
		// invalid geometry throws invalid_argument without changing state.
		// An empty rectangle is disabled. Changed geometry cancels a gesture;
		// setting the same rectangle preserves it.
		void set_region(const labrador::RectangleF& region);
		labrador::RectangleF region() const;

		// Call only for a fresh press the page accepts. Zero ids, a busy
		// control and points outside the half-open rectangle return false.
		// A candidate press requires a finite position or throws invalid_argument.
		bool begin(std::uint32_t contact, const labrador::Vector2F& position);

		// Only the owned contact moves the button. Sliding outside clears held
		// but keeps ownership; re-entering restores held. Its position must be
		// finite or invalid_argument leaves state unchanged. Foreign ids do nothing.
		void move(std::uint32_t contact, const labrador::Vector2F& position);
		void end(std::uint32_t contact);
		// The page calls this for cancellation, focus loss or layout changes.
		// No other held contact is adopted; only a later begin can capture.
		void cancel();

		std::uint32_t contact() const;
		bool held() const;

	private:
		labrador::RectangleF region_;
		std::uint32_t contact_ = 0;
		bool held_ = false;
	};
}
