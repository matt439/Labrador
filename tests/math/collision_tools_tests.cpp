#include <doctest/doctest.h>
#include "engine/math/collision_tools.h"
#include <cmath>

using namespace mattmath;

namespace
{
	void check_equal(float actual, float expected, float epsilon)
	{
		CHECK(std::abs(actual - expected) <= epsilon);
	}
}

namespace CollisionToolsTests
{
	constexpr float EPSILON_F = 0.000001f;

	TEST_SUITE("CollisionToolsTests")
	{
		TEST_CASE("test_rectangle_rectangle_collision")
		{
			// 1st test. Collision to the bottom right
			RectangleF rect1 = RectangleF(0.0f, 0.0f, 10.0f, 10.0f);
			RectangleF rect2 = RectangleF(5.0f, 5.0f, 10.0f, 10.0f);
			
			Vector2F direction =
				CollisionTools::calculate_object_collision_direction(&rect1, &rect2);

			Vector2F direction_expected = Vector2F::unit_vector(
				rect2.center() - rect1.center());

			check_equal(direction.x, direction_expected.x, EPSILON_F);
			check_equal(direction.y, direction_expected.y, EPSILON_F);

			Vector2F amount = Vector2F(0.0f, 0.0f);
			bool resolved = CollisionTools::resolve_object_collision(&rect1, &rect2, direction, amount);

			CHECK(resolved);
			check_equal(amount.x, -5.0f, EPSILON_F);
			check_equal(amount.y, -5.0f, EPSILON_F);

			// 2nd test. Collision to the left

			RectangleF rect3 = RectangleF(0.0f, 0.0f, 10.0f, 10.0f);
			RectangleF rect4 = RectangleF(-5.0f, 0.0f, 10.0f, 10.0f);

			direction = CollisionTools::calculate_object_collision_direction(&rect3, &rect4);

			direction_expected = Vector2F::unit_vector(
				rect4.center() - rect3.center());

			check_equal(direction.x, direction_expected.x, EPSILON_F);
			check_equal(direction.y, direction_expected.y, EPSILON_F);

			amount = Vector2F(0.0f, 0.0f);
			resolved = CollisionTools::resolve_object_collision(&rect3, &rect4, direction, amount);

			CHECK(resolved);
			check_equal(amount.x, 5.0f, EPSILON_F);
			check_equal(amount.y, 0.0f, EPSILON_F);
		}

		TEST_CASE("test_rectagle_triangle_collision")
		{
			RectangleF rect1 = RectangleF(0.0f, 0.0f, 10.0f, 10.0f);
			Triangle tri1 = Triangle(Vector2F(5.0f, 5.0f),
				Vector2F(15.0f, 5.0f),
				Vector2F(5.0f, 15.0f));

			Vector2F direction = CollisionTools::calculate_object_collision_direction(&rect1, &tri1);

			Vector2F direction_expected = Vector2F::unit_vector(
				tri1.center() - rect1.center());

			check_equal(direction.x, direction_expected.x, EPSILON_F);
			check_equal(direction.y, direction_expected.y, EPSILON_F);

			Vector2F amount = Vector2F(0.0f, 0.0f);
			bool resolved = CollisionTools::resolve_object_collision(&rect1, &tri1, direction, amount);

			CHECK(resolved);
			check_equal(amount.x, -5.0f, EPSILON_F);
			check_equal(amount.y, -5.0f, EPSILON_F);
			
			
			
			//RectangleF rect = RectangleF(220.0f, 5878.0f, 52.0f, 120.0f);
			//Triangle tri = Triangle(Vector2F(200.0f, 6000.0f),
			//						Vector2F(400.0f, 6000.0f),
			//						Vector2F(400.0f, 5800.0f));

			//Vector2F direction =
			//	CollisionTools::calculate_object_collision_direction(&rect, &tri);

			//check_equal(direction.x, Vector2F::DIRECTION_DOWN_RIGHT.x, EPSILON_F);
			//check_equal(direction.y, Vector2F::DIRECTION_DOWN_RIGHT.y, EPSILON_F);

			//Vector2F amount = Vector2F(0.0f, 0.0f);
			//bool resolved = CollisionTools::resolve_object_collision(&rect, &tri, direction, amount);

			//CHECK(resolved);
			//check_equal(-1.0f, amount.x, EPSILON_F);
			//check_equal(-1.0f, amount.y, EPSILON_F);
		}
	};
}