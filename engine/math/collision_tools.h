#pragma once

#include "engine/math/matt_math.h"
#include "engine/math/shape_type.h"

namespace mattmath
{
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
        static mattmath::Vector2F calculate_object_collision_direction(
            const mattmath::Shape* collider,
            const mattmath::Shape* collidee);

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
        static mattmath::Vector2F calculate_object_collision_direction_by_edge(
    		const mattmath::Shape* collider,
    		const mattmath::Shape* collidee);

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
        static bool resolve_object_collision(mattmath::Shape* collider,
            const mattmath::Shape* collidee,
            const mattmath::Vector2F& collision_direction, mattmath::Vector2F& amount);

        static bool resolve_object_collision(mattmath::Shape* collider,
            const mattmath::Shape* collidee,
            const mattmath::Vector2F& collision_direction);

    	static mattmath::Vector2F calculate_object_collision_depth(
    		const mattmath::Shape* collider,
    		const mattmath::Shape* collidee,
            const mattmath::Vector2F& collision_direction);

        static mattmath::Vector2F opposite_direction(const mattmath::Vector2F& direction);

    private:
        static void move_object_by_direction_relative_to_size(mattmath::Shape* obj,
            const mattmath::Vector2F& movement_direction, float relative_amount = 1.0f);

        /*
        * Moves the object back and forth with decreasing step size for
        * the given number of iterations.
        */
        static bool bracket_object_collision(bool colliding, int i, mattmath::Shape* collider,
            const mattmath::Vector2F& collider_direction);

        static bool bracket_object_collision_generic(mattmath::Shape* collider,
            const mattmath::Shape* collidee,
            const mattmath::Vector2F& collider_direction, int iterations);

        static mattmath::Vector2F compare_point_collision_depth_horizontal(
            const mattmath::Point2F& collider, const mattmath::Point2F& collidee);

        static mattmath::Vector2F compare_point_collision_depth_vertical(
            const mattmath::Point2F& collider, const mattmath::Point2F& collidee);

        static mattmath::Vector2F calculate_containing_collision_direction(
            const mattmath::Shape* collider, const mattmath::Shape* collidee);

        static mattmath::Vector2F shape_shape_collision_direction(
            const mattmath::Shape* collider, const mattmath::Shape* collidee);

        static void resolve_object_AABB_collision(mattmath::Shape* collider,
            const mattmath::Shape* collidee,
            const mattmath::Vector2F& collision_direction);
    };
}
