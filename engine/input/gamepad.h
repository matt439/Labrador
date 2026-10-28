#pragma once

#include "engine/math/vector2f.h"

#include <cstdint>

namespace labrador
{
	// Games own actions, bindings, rebinding and source-combination policy (T1).
	// These queries compare device snapshots independently of bindings. OR-ing
	// sources' pressed() values may fire again while another source remains
	// held; comparing their combined held levels fires once until all are up.

	// Every button a pad has, one bit each in GamepadState::buttons.
	enum class GamepadButton : uint16_t
	{
		a,
		b,
		x,
		y,
		left_shoulder,
		right_shoulder,
		left_stick,
		right_stick,
		back,
		start,
		dpad_up,
		dpad_down,
		dpad_left,
		dpad_right,
	};

	// Analogue triggers use caller-supplied thresholds for button-like edges.
	enum class GamepadTrigger
	{
		left,
		right,
	};

	// One frame's pad snapshot. Values may be constructed for edge queries.
	struct GamepadState
	{
		// False means the slot is empty, and every field below is then neutral
		// rather than whatever was last read out of it.
		bool connected = false;

		// Screen convention: +x right, +y down. No deadzone is applied; callers
		// choose one with apply_deadzone rather than applying a second deadzone
		// to a value the backend has already filtered.
		Vector2F left_stick = Vector2F::zero;
		Vector2F right_stick = Vector2F::zero;

		float left_trigger = 0.0f;
		float right_trigger = 0.0f;

		uint16_t buttons = 0;

		// Query stored values without consulting connected. Use named button
		// enumerators; occupancy gating belongs to the edge functions below.
		bool is_down(GamepadButton button) const;
		float trigger(GamepadTrigger trigger) const;
	};

	// One request for a pad's two rumble motors, held for a bounded time.
	//
	// low_frequency is the heavy motor, XInput's left and the browser's
	// strongMagnitude; high_frequency is the light motor, XInput's right and
	// weakMagnitude. Each strength is in [0, 1]. `seconds` is in
	// (0, max_vibration_seconds]; the engine stops an expired request during
	// polling. A new request on a slot replaces the previous one; renew before
	// expiry for a longer effect. Envelopes, mixing and player preferences are
	// game policy (T1).
	struct Vibration
	{
		float low_frequency = 0.0f;
		float high_frequency = 0.0f;
		float seconds = 0.0f;
	};

	// Longest single request, shared by all input backends.
	inline constexpr float max_vibration_seconds = 5.0f;

	// submitted means the pad API accepted the request, not that a motor was
	// observed turning. unavailable covers an empty slot, unsupported rumble,
	// suspension or API refusal; no request remains queued for later (T6).
	enum class VibrationResult
	{
		submitted,
		unavailable,
	};

	// Throws std::invalid_argument naming the field if a strength is not a
	// finite number in [0, 1] or `seconds` is not a finite number in
	// (0, max_vibration_seconds]. Out of range is a bug in the caller, so it
	// is refused rather than clamped into a request the caller did not make.
	void check_vibration(const Vibration& vibration);

	// pressed, released, trigger_pressed and trigger_released require connected
	// on both snapshots. This suppresses button/trigger edges on initial
	// connection, replug with an observed absent frame, and suspension/resume
	// when the reader reports an absent snapshot.
	// Hand-written snapshots must set connected to participate in edges.
	//
	// Occupancy does not establish device identity: a replacement between polls
	// without an absent snapshot cannot be detected by this rule or by the
	// connection-edge queries. Slots are the observable unit (gamepads.h).
	//
	// A disconnect does not produce released(). To stop an action when a pad
	// vanishes while holding its button, query the disconnect instead:
	//
	//     just_disconnected(slot) && previous_state(slot).is_down(button)
	//
	bool pressed(const GamepadState& now, const GamepadState& before,
		GamepadButton button);
	bool released(const GamepadState& now, const GamepadState& before,
		GamepadButton button);

	// Held means strictly greater than the caller's threshold; equality is up.
	// trigger_held queries the stored level without consulting connected;
	// trigger_pressed and trigger_released require both snapshots connected.
	bool trigger_held(const GamepadState& now, GamepadTrigger trigger,
		float threshold);
	bool trigger_pressed(const GamepadState& now, const GamepadState& before,
		GamepadTrigger trigger, float threshold);
	bool trigger_released(const GamepadState& now, const GamepadState& before,
		GamepadTrigger trigger, float threshold);

	bool just_connected(const GamepadState& now, const GamepadState& before);
	bool just_disconnected(const GamepadState& now, const GamepadState& before);

	// For deadzone in (0, 1), maps magnitudes at or below it to zero and
	// rescales the remaining range so full deflection stays 1, preserving sign.
	// A deadzone at or outside those bounds passes the value through. Inputs
	// are not clamped to a normalized range.
	float apply_deadzone(float value, float deadzone);

	// The same mapping on stick magnitude, preserving direction instead of
	// applying a separate threshold to each axis.
	Vector2F apply_deadzone(const Vector2F& stick,
		float deadzone);
}
