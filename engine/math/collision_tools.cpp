#include "engine/math/collision_tools.h"

using namespace mattmath;

Vector2F CollisionTools::opposite_direction(
    const Vector2F& direction)
{
	return Vector2F(-direction.x, -direction.y);
}

void CollisionTools::move_object_by_direction_relative_to_size(Shape* obj,
    const Vector2F& movement_direction, float relative_amount)
{
    RectangleF obj_aabb = obj->get_bounding_box();

    float relative_width = obj_aabb.width * relative_amount;
    float relative_height = obj_aabb.height * relative_amount;

    if (movement_direction.x == 0.0f)
	{
		relative_width = 0.0f;
	}
	if (movement_direction.y == 0.0f)
    {
		relative_height = 0.0f;
	}

	obj->offset(movement_direction * Vector2F(relative_width, relative_height));
}

bool CollisionTools::bracket_object_collision(bool colliding, int i, Shape* collider,
    const Vector2F& collider_direction)
{
    RectangleF collider_aabb = collider->get_bounding_box();

    if (colliding)
    {
        move_object_by_direction_relative_to_size(collider, collider_direction,
            1.0f / pow(ITERATION_POWER, static_cast<float>(i)));
    }
    else if (i == 1) // no collision in the first iteration
    {
        return false;
    }
    else
    {
        move_object_by_direction_relative_to_size(collider,
            opposite_direction(collider_direction),
            1.0f / pow(ITERATION_POWER, static_cast<float>(i)));
    }
    return true;
}

bool CollisionTools::bracket_object_collision_generic(Shape* collider, const Shape* collidee,
    const Vector2F& collider_direction, int iterations)
{
    RectangleF collider_aabb = collider->get_bounding_box();

    for (int i = 1; i <= iterations; i++)
    {
        if (!bracket_object_collision(collider->intersects(collidee),
            i, collider, collider_direction))
        {
            return false;
        }
    }
    return true;
}

Vector2F CollisionTools::compare_point_collision_depth_horizontal(
    const Point2F& collider, const Point2F& collidee)
{
    if (collider.x < collidee.x)
    {
        return Vector2F::DIRECTION_RIGHT;
    }
	return Vector2F::DIRECTION_LEFT;
}

Vector2F CollisionTools::compare_point_collision_depth_vertical(
    const Point2F& collider, const Point2F& collidee)
{
    if (collider.y < collidee.y)
    {
		return Vector2F::DIRECTION_DOWN;
    }
	return Vector2F::DIRECTION_UP;
}

Vector2F CollisionTools::calculate_containing_collision_direction(
    const Shape* collider, const Shape* collidee)
{
    // calculate the direction of the greatest distance between the two shapes
	Point2F collider_center = collider->get_center();
	Point2F collidee_center = collidee->get_center();

    float x_distance = collider_center.x - collidee_center.x;

    float y_distance = collider_center.y - collidee_center.y;

    if (std::abs(x_distance) < std::abs(y_distance))
    {
        return compare_point_collision_depth_horizontal(collider_center, collidee_center);
    }
    return compare_point_collision_depth_vertical(collider_center, collidee_center);
}

Vector2F CollisionTools::shape_shape_collision_direction(
    const Shape* collider, const Shape* collidee)
{
	Point2F collider_center = collider->get_center();
	Point2F collidee_center = collidee->get_center();

	Vector2F direction = collidee_center - collider_center;

	if (direction == Vector2F::ZERO)
	{
		return Vector2F::ZERO;
	}

	return Vector2F::unit_vector(direction);
}

Vector2F CollisionTools::calculate_object_collision_direction(const Shape* collider,
    const Shape* collidee)
{
    if (!collider->intersects(collidee))
    {
		return Vector2F::ZERO;
    }

    return shape_shape_collision_direction(collider, collidee);
}

Vector2F CollisionTools::calculate_object_collision_direction_by_edge(
    const Shape* collider, const Shape* collidee)
{
    RectangleF collider_aabb = collider->get_bounding_box();
    RectangleF collidee_aabb = collidee->get_bounding_box();
    
    Segment collider_top_edge = collider_aabb.get_top_edge();
    Segment collider_left_edge = collider_aabb.get_left_edge();
    Segment collider_right_edge = collider_aabb.get_right_edge();
    Segment collider_bottom_edge = collider_aabb.get_bottom_edge();

    bool left_edge = collidee_aabb.intersects(collider_left_edge);
    bool right_edge = collidee_aabb.intersects(collider_right_edge);
    bool top_edge = collidee_aabb.intersects(collider_top_edge);
    bool bottom_edge = collidee_aabb.intersects(collider_bottom_edge);

    if (left_edge && right_edge && top_edge && bottom_edge) // collidee is equal size of collider
    {
        return calculate_containing_collision_direction(collider, collidee);
    }
    if ((left_edge && right_edge && top_edge) || (top_edge &&
        !(left_edge || right_edge || bottom_edge)))
    {
    	return Vector2F::DIRECTION_UP;
    }
    if ((left_edge && right_edge && bottom_edge) || (bottom_edge &&
        !(left_edge || right_edge || top_edge)))
    {
    	return Vector2F::DIRECTION_DOWN;
    }
    if ((top_edge && bottom_edge && right_edge) || (right_edge &&
        !(left_edge || top_edge || bottom_edge)))
    {
    	return Vector2F::DIRECTION_RIGHT;
    }
    if ((top_edge && bottom_edge && left_edge) || (left_edge &&
        !(right_edge || top_edge || bottom_edge)))
    {
    	return Vector2F::DIRECTION_LEFT;
    }
    if (left_edge && right_edge)
    {
        // check if the collider is more to the left or right of the collidee
        return compare_point_collision_depth_horizontal(collider->get_center(),
            collidee->get_center());
    }
    if (top_edge && bottom_edge)
    {
        // check if the collider is more to the top or bottom of the collidee
        return compare_point_collision_depth_vertical(collider->get_center(),
            collidee->get_center());
    }
    if (left_edge && top_edge)
    {
    	return Vector2F::DIRECTION_UP_LEFT;
    }
    if (left_edge && bottom_edge)
    {
    	return Vector2F::DIRECTION_DOWN_LEFT;
    }
    if (right_edge && top_edge)
    {
    	return Vector2F::DIRECTION_UP_RIGHT;
    }
    if (right_edge && bottom_edge)
    {
    	return Vector2F::DIRECTION_DOWN_RIGHT;
    }

    // collider contains collidee or collidee contains collider
    return calculate_containing_collision_direction(collider, collidee);
}

void CollisionTools::resolve_object_AABB_collision(Shape* collider,
    const Shape* collidee,
    const Vector2F& collision_direction)
{
    // get the intersection rectangle
    RectangleF inter = RectangleF::intersection(collider->get_bounding_box(),
        collidee->get_bounding_box());

    Vector2F amount = { inter.width, inter.height };

	Vector2F direction = collision_direction;

	if (collision_direction.x != 0.0f && collision_direction.y != 0.0f)
	{
		float larger = std::max(collision_direction.x, collision_direction.y);

        direction = collision_direction / larger;
	}

	collider->offset(opposite_direction(direction) * amount);
}

bool CollisionTools::resolve_object_collision(Shape* collider, const Shape* collidee,
    const Vector2F& collision_direction, Vector2F& amount)
{
    // check if the sprites are colliding
    if (collision_direction == Vector2F::ZERO || !collider->intersects(collidee))
    {
		amount = Vector2F::ZERO;
        return false;
    }

    Vector2F unit_direction = Vector2F::unit_vector(collision_direction);
	Vector2F collider_center = collider->get_bounding_box().get_center();

    if (collider->get_shape_type() == ShapeType::rectangle &&
        collidee->get_shape_type() == ShapeType::rectangle)
    {
        resolve_object_AABB_collision(collider, collidee, unit_direction);
    }
    else // one or both of the sprites are using pixel collision
    {
        bracket_object_collision_generic(collider, collidee,
                                            opposite_direction(unit_direction),
                                                                BRACKET_ITERATIONS);
    }

	// calculate the amount moved
	Vector2F new_center = collider->get_bounding_box().get_center();
	amount = new_center - collider_center;

    return true;
}

bool CollisionTools::resolve_object_collision(Shape* collider, const Shape* collidee,
    const Vector2F& collision_direction)
{
    Vector2F amount;
	return resolve_object_collision(collider, collidee, collision_direction, amount);
}

Vector2F CollisionTools::calculate_object_collision_depth(
    const Shape* collider,
    const Shape* collidee,
    const Vector2F& collision_direction)
{
	std::unique_ptr<Shape> collider_copy = collider->clone();
    Vector2F amount;

	resolve_object_collision(collider_copy.get(), collidee, collision_direction, amount);

	return amount;
}