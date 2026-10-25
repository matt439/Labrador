#pragma once

#include "engine/math/vector2f.h"

// The template's rules: where the greeting is, and how it moves. HelloState
// reads the devices and draws the result; everything between the two is here.
//
// A LIBRARY OF ITS OWN THAT LINKS NO ENGINE, so a test can run it with no
// window, no device and no input device behind it (CMakeLists.txt,
// MinimalRules, and tests/motion_tests.cpp beside this). It takes what the
// devices said as plain directions rather than reading a Keyboard or a Gamepads,
// which is the whole trick: a rule that asks the platform a question cannot be
// tested without one. samples/linesweeper/rules/ is the same wall around a
// whole game.
//
// A game grows its own rules here - a score, a timer, what a collision means -
// and the states stay the part that talks to the engine.

// The two numbers the greeting's placement is worked out from: the point it
// circles, which the stick and the keys move, and how far round the circle it
// is, in radians.
struct Motion
{
	mattmath::Vector2F centre = { 0.0f, 0.0f };
	float spin = 0.0f;
};

// Pixels a second at full deflection, radians a second, and how far out the
// greeting circles. Tuning, next to the rules it configures (CONVENTIONS,
// Constants).
inline constexpr float move_speed = 400.0f;
inline constexpr float spin_speed = 1.2f;
inline constexpr float orbit_radius = 24.0f;

// The direction a step moves in: the stick and the keys summed rather than
// chosen between, so a desk with both can use both, and clamped to one unit.
// The clamp is what makes the sum safe, and it matters alone too: the keys give
// a whole unit per axis, so up-and-left is 1.41 units long and would otherwise
// outrun any push of the stick by half.
//
// Both are in the engine's convention, +y down, with the stick's deadzone
// already applied.
mattmath::Vector2F combined_direction(const mattmath::Vector2F& stick,
	const mattmath::Vector2F& keys);

// One step of dt seconds: the centre moved along combined_direction at
// move_speed, and the spin advanced.
Motion step_motion(const Motion& motion, const mattmath::Vector2F& stick,
	const mattmath::Vector2F& keys, float dt);

// Where the greeting is drawn: orbit_radius above the centre, turned about it
// by the spin.
mattmath::Vector2F greeting_position(const Motion& motion);
