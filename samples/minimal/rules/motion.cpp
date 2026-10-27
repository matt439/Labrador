#include "samples/minimal/rules/motion.h"

#include "engine/math/matrix3x2f.h"

using namespace labrador;

Vector2F combined_direction(const Vector2F& stick, const Vector2F& keys)
{
	Vector2F direction = stick + keys;
	if (direction.length_squared() > 1.0f)
	{
		direction.normalize();
	}
	return direction;
}

Motion step_motion(const Motion& motion, const Vector2F& stick,
	const Vector2F& keys, float dt)
{
	Motion next = motion;
	next.centre += combined_direction(stick, keys) * (move_speed * dt);
	next.spin += spin_speed * dt;
	return next;
}

Vector2F greeting_position(const Motion& motion)
{
	// The two factories compose left to right, in the order the two things
	// happen to the text: turn it about the origin, then move it out to the
	// centre. One transform_point then takes the offset through both, which
	// is what the type is for - build the transform once, push points through
	// it. Writing it as a pair of sines instead works today and stops working
	// the moment a third step joins them.
	const Matrix3x2F to_screen = Matrix3x2F::rotation(motion.spin) *
		Matrix3x2F::translation(motion.centre);
	return to_screen.transform_point(Point2F(0.0f, -orbit_radius));
}
