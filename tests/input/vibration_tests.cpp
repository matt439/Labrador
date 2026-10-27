#include <doctest/doctest.h>

#include "engine/input/gamepad.h"
#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/vibration_output.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace labrador;

// Vibration's checks, its lifetime rules against a transport that records
// what reached it, and Gamepads' answers on a machine with no pad. No case
// here turns a motor: what a pad API does with a request is outside every
// test in this tree.

namespace
{
	struct Call
	{
		enum class Kind
		{
			send,
			stop,
		};

		Kind kind;
		int slot;
		float low_frequency = 0.0f;
		float high_frequency = 0.0f;
	};

	// Records every call, and takes or refuses sends as `accepts` says.
	struct RecordingTransport
	{
		std::vector<Call>* calls = nullptr;
		bool accepts = true;

		bool send(int slot, const Vibration& vibration)
		{
			this->calls->push_back({ Call::Kind::send, slot,
				vibration.low_frequency, vibration.high_frequency });
			return this->accepts;
		}

		void stop(int slot)
		{
			this->calls->push_back({ Call::Kind::stop, slot });
		}
	};

	using Output = VibrationOutput<RecordingTransport>;

	Vibration rumble(float seconds)
	{
		return Vibration{ 0.75f, 0.25f, seconds };
	}

	int count(const std::vector<Call>& calls, Call::Kind kind, int slot)
	{
		int total = 0;
		for (const Call& call : calls)
		{
			if (call.kind == kind && call.slot == slot)
			{
				++total;
			}
		}
		return total;
	}
}

TEST_CASE("a vibration in range is accepted at both ends of every range")
{
	CHECK_NOTHROW(check_vibration({ 0.0f, 0.0f, 0.001f }));
	CHECK_NOTHROW(check_vibration({ 1.0f, 1.0f, max_vibration_seconds }));
	CHECK_NOTHROW(check_vibration({ 0.5f, 0.0f, 1.0f }));
}

TEST_CASE("a strength outside [0, 1] or not a number is refused, not clamped")
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();

	CHECK_THROWS_AS(check_vibration({ -0.1f, 0.0f, 1.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 1.1f, 0.0f, 1.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 0.0f, -0.1f, 1.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 0.0f, 1.1f, 1.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ nan, 0.0f, 1.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 0.0f, infinity, 1.0f }),
		std::invalid_argument);
}

TEST_CASE("a duration must be positive, finite and no longer than the bound")
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();

	CHECK_THROWS_AS(check_vibration({ 0.5f, 0.5f, 0.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 0.5f, 0.5f, -1.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 0.5f, 0.5f,
		std::nextafter(max_vibration_seconds, 10.0f) }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 0.5f, 0.5f, nan }),
		std::invalid_argument);
	CHECK_THROWS_AS(check_vibration({ 0.5f, 0.5f, infinity }),
		std::invalid_argument);
}

TEST_CASE("a request reaches the transport for its own slot, with both motors")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });

	CHECK(output.start(2, rumble(1.0f), 0.0));
	REQUIRE(calls.size() == 1);
	CHECK(calls[0].kind == Call::Kind::send);
	CHECK(calls[0].slot == 2);
	CHECK(calls[0].low_frequency == doctest::Approx(0.75f));
	CHECK(calls[0].high_frequency == doctest::Approx(0.25f));
	CHECK(output.active(2));
	CHECK_FALSE(output.active(0));
}

TEST_CASE("a request ends when its seconds pass, and not before")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });
	output.start(1, rumble(0.5f), 10.0);

	output.sampled(1, true, 10.25);
	CHECK(output.active(1));
	CHECK(count(calls, Call::Kind::stop, 1) == 0);

	output.sampled(1, true, 10.5);
	CHECK_FALSE(output.active(1));
	CHECK(count(calls, Call::Kind::stop, 1) == 1);

	// Ended once: later samples send nothing more.
	output.sampled(1, true, 11.0);
	CHECK(count(calls, Call::Kind::stop, 1) == 1);
}

TEST_CASE("a newer request on a slot replaces the older one's end")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });
	output.start(0, rumble(0.5f), 0.0);
	output.start(0, rumble(2.0f), 0.25);

	output.sampled(0, true, 1.0);
	CHECK(output.active(0));
	output.sampled(0, true, 2.25);
	CHECK_FALSE(output.active(0));
	CHECK(count(calls, Call::Kind::send, 0) == 2);
	CHECK(count(calls, Call::Kind::stop, 0) == 1);
}

TEST_CASE("an explicit stop reaches the transport, with or without a request")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });
	output.start(3, rumble(1.0f), 0.0);

	output.stop(3);
	CHECK_FALSE(output.active(3));
	CHECK(count(calls, Call::Kind::stop, 3) == 1);

	output.stop(3);
	CHECK(count(calls, Call::Kind::stop, 3) == 2);

	// And its time passing later sends nothing for a request already gone.
	output.sampled(3, true, 5.0);
	CHECK(count(calls, Call::Kind::stop, 3) == 2);
}

TEST_CASE("a slot that loses its pad is stopped and keeps nothing for the next")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });
	output.start(1, rumble(4.0f), 0.0);

	output.sampled(1, false, 0.1);
	CHECK_FALSE(output.active(1));
	CHECK(count(calls, Call::Kind::stop, 1) == 1);

	// A pad arriving in the slot is not sent the old request.
	output.sampled(1, true, 0.2);
	CHECK(count(calls, Call::Kind::send, 1) == 1);
	CHECK_FALSE(output.active(1));
}

TEST_CASE("a refused request is not kept, and stops the one it replaced")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });
	output.start(0, rumble(4.0f), 0.0);

	output.transport().accepts = false;
	CHECK_FALSE(output.start(0, rumble(1.0f), 0.5));
	CHECK_FALSE(output.active(0));
	CHECK(count(calls, Call::Kind::stop, 0) == 1);

	CHECK_FALSE(output.start(1, rumble(1.0f), 0.5));
	CHECK_FALSE(output.active(1));
	CHECK(count(calls, Call::Kind::stop, 1) == 0);
}

TEST_CASE("suspension stops every request, refuses new ones, and resume replays none")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });
	output.start(0, rumble(4.0f), 0.0);
	output.start(2, rumble(4.0f), 0.0);

	output.suspend();
	CHECK(count(calls, Call::Kind::stop, 0) == 1);
	CHECK(count(calls, Call::Kind::stop, 2) == 1);
	CHECK_FALSE(output.active(0));
	CHECK_FALSE(output.active(2));

	const std::size_t before = calls.size();
	CHECK_FALSE(output.start(1, rumble(1.0f), 0.5));
	CHECK(calls.size() == before);

	output.resume();
	output.sampled(0, true, 0.6);
	output.sampled(2, true, 0.6);
	CHECK(calls.size() == before);

	// And after resume a new request goes through.
	CHECK(output.start(1, rumble(1.0f), 0.7));
	CHECK(count(calls, Call::Kind::send, 1) == 1);
}

TEST_CASE("destruction stops every slot with a request, and only those")
{
	std::vector<Call> calls;
	{
		Output output(RecordingTransport{ &calls });
		output.start(1, rumble(4.0f), 0.0);
		output.start(3, rumble(4.0f), 0.0);
		output.stop(3);
		calls.clear();
	}
	REQUIRE(calls.size() == 1);
	CHECK(calls[0].kind == Call::Kind::stop);
	CHECK(calls[0].slot == 1);
}

TEST_CASE("a slot outside the four reaches nothing")
{
	std::vector<Call> calls;
	Output output(RecordingTransport{ &calls });

	CHECK_FALSE(output.start(-1, rumble(1.0f), 0.0));
	CHECK_FALSE(output.start(Gamepads::max_count, rumble(1.0f), 0.0));
	output.stop(Gamepads::max_count);
	output.sampled(-1, false, 1.0);
	CHECK(calls.empty());
	CHECK_FALSE(output.active(Gamepads::max_count));
}

TEST_CASE("Gamepads answers unavailable for an empty slot and checks before that")
{
	// The build's own reader, on a machine running the tests: no pad is
	// plugged in on CI, and Node has no Gamepad API, so every slot is empty.
	GamepadReader reader;
	Gamepads pads(&reader);
	pads.poll();
	REQUIRE_FALSE(pads.connected(0));

	CHECK(pads.vibrate(0, rumble(1.0f)) == VibrationResult::unavailable);
	CHECK_NOTHROW(pads.stop_vibration(0));

	// A broken call is refused whether or not a pad is there.
	CHECK_THROWS_AS(pads.vibrate(Gamepads::max_count, rumble(1.0f)),
		std::out_of_range);
	CHECK_THROWS_AS(pads.vibrate(-1, rumble(1.0f)), std::out_of_range);
	CHECK_THROWS_AS(pads.stop_vibration(Gamepads::max_count),
		std::out_of_range);
	CHECK_THROWS_AS(pads.vibrate(0, Vibration{ 2.0f, 0.0f, 1.0f }),
		std::invalid_argument);
	CHECK_THROWS_AS(pads.vibrate(0, Vibration{ 0.5f, 0.5f, 0.0f }),
		std::invalid_argument);
}

TEST_CASE("a suspended reader refuses a request and resumes taking them")
{
	GamepadReader reader;
	reader.suspend();
	CHECK_FALSE(reader.vibrate(0, rumble(1.0f)));
	reader.resume();

	// Resumed, it still has no pad to reach, and says so.
	CHECK_FALSE(reader.vibrate(0, rumble(1.0f)));
	CHECK_FALSE(reader.vibrate(Gamepads::max_count, rumble(1.0f)));
	CHECK_NOTHROW(reader.stop_vibration(0));
}
