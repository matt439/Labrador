#pragma once

#include "engine/math/matt_math.h"
#include "engine/math/shape_type.h"

constexpr int BRACKET_ITERATIONS = 40;
constexpr float ITERATION_POWER = 1.5f;

class CollisionTools
{
public:
	CollisionTools() = default;

	/*
	* Determines the direction of the collision between two objects,
	* relative to the collider. If the objects are not colliding,
    * a zero vector is returned.
    * 
	* @param collider The object that is colliding with the collidee.
	* @param collidee The object that is being collided with.
	* @return The direction of the collision.
    */
    static MattMath::Vector2F calculate_object_collision_direction(
        const MattMath::Shape* collider,
        const MattMath::Shape* collidee);

	/*
	* Determines the direction of the collision between two objects,
	* relative to the collider. The edges of the shape's bounding boxes
    * are used to assess the collision. This function is less accurate
    * with non-rectangle shapes. If the objects are not colliding,
	* a zero vector is returned.
    * 
	* @param collider The object that is colliding with the collidee.
	* @param collidee The object that is being collided with.
	* @return The direction of the collision.
    */
    static MattMath::Vector2F calculate_object_collision_direction_by_edge(
		const MattMath::Shape* collider,
		const MattMath::Shape* collidee);

    /*
	* Resolves the collision between two objects by moving the collider
	* in the opposite direction of the collision. The amount moved is
	* stored in the amount parameter.
    * 
	* @param collider The object that is colliding with the collidee.
	* @param collidee The object that is being collided with.
	* @param collision_direction The direction of the collision.
	* @param amount The amount which the collider has been moved.
	* @return True if the objects are colliding, false otherwise.
    */
    static bool resolve_object_collision(MattMath::Shape* collider,
        const MattMath::Shape* collidee,
        const MattMath::Vector2F& collision_direction, MattMath::Vector2F& amount);

    static bool resolve_object_collision(MattMath::Shape* collider,
        const MattMath::Shape* collidee,
        const MattMath::Vector2F& collision_direction);

	static MattMath::Vector2F calculate_object_collision_depth(
		const MattMath::Shape* collider,
		const MattMath::Shape* collidee,
        const MattMath::Vector2F& collision_direction);

    static MattMath::Vector2F opposite_direction(const MattMath::Vector2F& direction);

private:
    static void move_object_by_direction_relative_to_size(MattMath::Shape* obj,
        const MattMath::Vector2F& movement_direction, float relative_amount = 1.0f);

    /*
    * Moves the object back and forth with decreasing step size for
    * the given number of iterations.
    */
    static bool bracket_object_collision(bool colliding, int i, MattMath::Shape* collider,
        const MattMath::Vector2F& collider_direction);

    static bool bracket_object_collision_generic(MattMath::Shape* collider,
        const MattMath::Shape* collidee,
        const MattMath::Vector2F& collider_direction, int iterations);

    static MattMath::Vector2F compare_point_collision_depth_horizontal(
        const MattMath::Point2F& collider, const MattMath::Point2F& collidee);

    static MattMath::Vector2F compare_point_collision_depth_vertical(
        const MattMath::Point2F& collider, const MattMath::Point2F& collidee);

    static MattMath::Vector2F calculate_containing_collision_direction(
        const MattMath::Shape* collider, const MattMath::Shape* collidee);

    static MattMath::Vector2F shape_shape_collision_direction(
        const MattMath::Shape* collider, const MattMath::Shape* collidee);

    static void resolve_object_AABB_collision(MattMath::Shape* collider,
        const MattMath::Shape* collidee,
        const MattMath::Vector2F& collision_direction);
};
