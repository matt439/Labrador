#include "engine/input/scripted_gamepad_source.h"
#include "engine/input/gamepads.h"
#include "engine/input/vibration_output.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace labrador
{
	struct ScriptedGamepadSource::Impl
	{
		void record(ScriptedGamepadEvent event, int slot = -1,
			const Vibration& request = {}, bool accepted = false) noexcept
		{
			if (this->records.size() == this->record_capacity)
			{
				this->overflowed = true;
				return;
			}
			this->records.push_back({ event, slot, request, accepted, this->seconds });
		}

		struct Transport
		{
			Impl* source = nullptr;

			bool send(int slot, const Vibration& request)
			{
				const bool accepted = this->source->current[slot].connected &&
					this->source->available[slot];
				this->source->record(ScriptedGamepadEvent::vibrate, slot,
					request, accepted);
				return accepted;
			}

			void stop(int slot) noexcept
			{
				this->source->record(ScriptedGamepadEvent::stop, slot);
			}
		};

		std::array<GamepadState, Gamepads::max_count> supplied;
		std::array<GamepadState, Gamepads::max_count> current;
		std::array<bool, Gamepads::max_count> available{ true, true, true, true };
		std::vector<ScriptedGamepadRecord> records;
		std::size_t record_capacity = 0;
		double seconds = 0.0;
		bool overflowed = false;
		bool attached = false;
		bool suspended = false;
		// Last, so teardown stops can still append to the owned record buffer.
		VibrationOutput<Transport> vibration{ Transport{ this } };
	};

	namespace
	{
		void check_slot(int slot)
		{
			if (slot < 0 || slot >= Gamepads::max_count)
			{
				throw std::out_of_range("ScriptedGamepadSource: invalid slot " +
					std::to_string(slot));
			}
		}
	}

	ScriptedGamepadSource::ScriptedGamepadSource(std::size_t record_capacity) :
		impl_(std::make_unique<Impl>())
	{
		this->impl_->records.reserve(record_capacity);
		this->impl_->record_capacity = record_capacity;
	}

	ScriptedGamepadSource::~ScriptedGamepadSource() = default;

	void ScriptedGamepadSource::set_state(int slot, const GamepadState& state)
	{
		check_slot(slot);
		this->impl_->supplied[slot] = state.connected ? state : GamepadState();
	}

	void ScriptedGamepadSource::set_vibration_available(int slot, bool available)
	{
		check_slot(slot);
		this->impl_->available[slot] = available;
	}

	void ScriptedGamepadSource::set_time(double seconds)
	{
		if (!std::isfinite(seconds) || seconds < this->impl_->seconds)
		{
			throw std::invalid_argument(
				"ScriptedGamepadSource: time must be finite and nondecreasing");
		}
		this->impl_->seconds = seconds;
	}

	std::span<const ScriptedGamepadRecord> ScriptedGamepadSource::records() const
	{
		return this->impl_->records;
	}

	bool ScriptedGamepadSource::records_overflowed() const
	{
		return this->impl_->overflowed;
	}

	void ScriptedGamepadSource::clear_records()
	{
		this->impl_->records.clear();
		this->impl_->overflowed = false;
	}

	void ScriptedGamepadSource::attach()
	{
		if (this->impl_->attached)
		{
			throw std::logic_error("ScriptedGamepadSource already has a reader");
		}
		this->impl_->attached = true;
		this->impl_->suspended = false;
		this->impl_->current.fill(GamepadState());
		this->impl_->vibration.resume();
	}

	void ScriptedGamepadSource::detach() noexcept
	{
		this->impl_->vibration.suspend();
		this->impl_->attached = false;
	}

	void ScriptedGamepadSource::sample()
	{
		Impl& impl = *this->impl_;
		impl.record(ScriptedGamepadEvent::sample);
		for (int slot = 0; slot < Gamepads::max_count; ++slot)
		{
			impl.current[slot] = impl.suspended ? GamepadState() : impl.supplied[slot];
			impl.vibration.sampled(slot, impl.current[slot].connected, impl.seconds);
		}
	}

	GamepadState ScriptedGamepadSource::read(int slot) const
	{
		return slot < 0 || slot >= Gamepads::max_count ? GamepadState() :
			this->impl_->current[slot];
	}

	void ScriptedGamepadSource::suspend()
	{
		this->impl_->vibration.suspend();
		this->impl_->suspended = true;
		this->impl_->record(ScriptedGamepadEvent::suspend);
	}

	void ScriptedGamepadSource::resume()
	{
		this->impl_->suspended = false;
		this->impl_->vibration.resume();
		this->impl_->record(ScriptedGamepadEvent::resume);
	}

	bool ScriptedGamepadSource::vibrate(int slot, const Vibration& vibration)
	{
		check_vibration(vibration);
		if (this->impl_->suspended || slot < 0 || slot >= Gamepads::max_count)
		{
			this->impl_->record(ScriptedGamepadEvent::vibrate, slot, vibration, false);
		}
		return this->impl_->vibration.start(slot, vibration, this->impl_->seconds);
	}

	void ScriptedGamepadSource::stop_vibration(int slot)
	{
		this->impl_->vibration.stop(slot);
	}
}
