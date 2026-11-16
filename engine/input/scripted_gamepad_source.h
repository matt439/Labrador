#pragma once

#include "engine/input/gamepad.h"

#include <cstddef>
#include <memory>
#include <span>

namespace labrador
{
	class GamepadReader;

	enum class ScriptedGamepadEvent
	{
		sample,
		suspend,
		resume,
		vibrate,
		stop,
	};

	struct ScriptedGamepadRecord
	{
		ScriptedGamepadEvent event = ScriptedGamepadEvent::sample;
		int slot = -1;
		Vibration vibration;
		bool accepted = false;
		double seconds = 0.0;
	};

	// An explicitly selected pad source for fixtures in the shipping library.
	// Owns copies of supplied snapshots and records; never calls a pad API.
	// Use on one thread. This immovable object must outlive its borrowing reader
	// (and Application, when selected there), including reader moves and teardown.
	// Only one reader may borrow a source at a time; distinct sources are isolated.
	class ScriptedGamepadSource
	{
	public:
		// Reserves a bounded record buffer once. A full buffer drops later records
		// and sets records_overflowed(); input and vibration lifetimes still run.
		// Recording never allocates during sampling, suspension or teardown.
		explicit ScriptedGamepadSource(std::size_t record_capacity = 1024);
		~ScriptedGamepadSource();

		ScriptedGamepadSource(const ScriptedGamepadSource&) = delete;
		ScriptedGamepadSource& operator=(const ScriptedGamepadSource&) = delete;
		ScriptedGamepadSource(ScriptedGamepadSource&&) = delete;
		ScriptedGamepadSource& operator=(ScriptedGamepadSource&&) = delete;

		// Copies the next sample's state. Slots are [0, 4); other values throw
		// std::out_of_range. Disconnected snapshots are made wholly neutral.
		// To replace a pad, sample its disconnection before reusing its slot.
		void set_state(int slot, const GamepadState& state);

		// Choose whether this slot's simulated output accepts a request; true
		// initially. Disconnected and suspended slots always refuse output.
		void set_vibration_available(int slot, bool available);

		// Seconds on the script's clock, initially zero. Must be finite and
		// nonnegative and never decrease; invalid values throw invalid_argument.
		// Expiry is checked on sample(), as it is for the configured source.
		void set_time(double seconds);

		// Borrowed until clear_records() or destruction. Appending does not
		// invalidate existing records. Clear resets the overflow flag too.
		std::span<const ScriptedGamepadRecord> records() const;
		bool records_overflowed() const;
		void clear_records();

	private:
		friend class GamepadReader;
		void attach();
		void detach() noexcept;
		void sample();
		GamepadState read(int slot) const;
		void suspend();
		void resume();
		bool vibrate(int slot, const Vibration& vibration);
		void stop_vibration(int slot);

		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
