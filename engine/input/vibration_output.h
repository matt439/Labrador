#pragma once

#include "engine/input/gamepad.h"
#include "engine/input/gamepads.h"

#include <chrono>
#include <utility>

namespace labrador
{
	// The lifetime of every vibration request, shared by both input backends,
	// which supply only the call that reaches their pad API.
	//
	// `Transport` has two members: send(slot, vibration), answering whether
	// the pad API took the request, and stop(slot), a best-effort stop that
	// must not throw. A backend's transport is a few lines over its platform
	// call; a test's records what it was asked, which is how InputTests sees a
	// cancellation reach the transport rather than only a flag change here.
	//
	// THE RULES THIS HOLDS, so that no backend has to keep them by itself:
	//   - a request lasts until its seconds pass, a stop, or a newer request
	//     on its slot, whichever is first, and its end stops the motors;
	//   - a slot that loses its pad forgets its request and is stopped, so
	//     nothing waits to vibrate whatever pad arrives there next;
	//   - suspension stops every slot and refuses requests until resume, and
	//     resume replays nothing;
	//   - destruction stops every slot that has a request.
	//
	// Not a public type: Gamepads::vibrate is the game's way in. A template in
	// a header because the transport is one of two structs known at build
	// time, and the tests need a third.
	template <typename Transport>
	class VibrationOutput
	{
	public:
		explicit VibrationOutput(Transport transport) :
			transport_(std::move(transport))
		{
		}

		~VibrationOutput()
		{
			this->stop_all();
		}

		VibrationOutput(const VibrationOutput&) = delete;
		VibrationOutput& operator=(const VibrationOutput&) = delete;

		// Sends `vibration` to `slot` at time `now`, in seconds on any one
		// clock, and answers whether the transport took it. A refused request
		// is not kept, so its slot has nothing to end later, and one refused
		// while another runs there stops that one.
		bool start(int slot, const Vibration& vibration, double now)
		{
			if (!in_range(slot) || this->suspended_)
			{
				return false;
			}
			if (!this->transport_.send(slot, vibration))
			{
				// A refused replacement still ends the request before it.
				if (this->active_[slot])
				{
					this->stop(slot);
				}
				return false;
			}
			this->active_[slot] = true;
			this->ends_[slot] = now + vibration.seconds;
			return true;
		}

		// Stops `slot` whether or not it has a request, because the game's
		// stop is a request for silence and costs one call.
		void stop(int slot)
		{
			if (!in_range(slot))
			{
				return;
			}
			this->active_[slot] = false;
			this->transport_.stop(slot);
		}

		// Called once per slot per sample, after the backend has read it.
		// `same_pad` is false when the slot is empty or holds a different pad
		// from the last sample. Ends a request whose time has passed, or whose
		// pad has gone.
		void sampled(int slot, bool same_pad, double now)
		{
			if (!in_range(slot) || !this->active_[slot])
			{
				return;
			}
			if (!same_pad || now >= this->ends_[slot])
			{
				this->stop(slot);
			}
		}

		void suspend()
		{
			this->stop_all();
			this->suspended_ = true;
		}

		void resume()
		{
			this->suspended_ = false;
		}

		bool active(int slot) const
		{
			return in_range(slot) && this->active_[slot];
		}

		Transport& transport()
		{
			return this->transport_;
		}

	private:
		static bool in_range(int slot)
		{
			return slot >= 0 && slot < Gamepads::max_count;
		}

		void stop_all()
		{
			for (int slot = 0; slot < Gamepads::max_count; ++slot)
			{
				if (this->active_[slot])
				{
					this->stop(slot);
				}
			}
		}

		Transport transport_;
		bool active_[Gamepads::max_count] = {};
		double ends_[Gamepads::max_count] = {};
		bool suspended_ = false;
	};

	// The clock both backends hand VibrationOutput: seconds, monotonic.
	inline double vibration_clock_seconds()
	{
		return std::chrono::duration<double>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}
}
