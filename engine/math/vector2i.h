#pragma once


namespace labrador
{
	struct Vector2F;

	struct Vector2I
	{
		int x = 0;
		int y = 0;

		Vector2I() = default;
		Vector2I(const Vector2I&) = default;
		Vector2I& operator=(const Vector2I&) = default;
		// constexpr and defined here so zero is constant-initialised. See
		// Vector2F's constants for what the dynamic alternative risks.
		constexpr Vector2I(int x, int y) : x(x), y(y) {}
		// Each component truncates toward zero, as static_cast<int> does:
		// (2.9, -2.9) becomes (2, -2).
		explicit Vector2I(const Vector2F& vector);

		bool operator==(const Vector2I& other) const;
		bool operator!=(const Vector2I& other) const;

		Vector2I& operator+=(const Vector2I& other);
		Vector2I& operator-=(const Vector2I& other);
		Vector2I& operator*=(const Vector2I& other);
		Vector2I& operator*=(int other);
		// Division is int division per component, truncating toward zero:
		// (7, -7) / 2 is (3, -3).
		Vector2I& operator/=(const Vector2I& other);
		Vector2I& operator/=(int other);

		void offset(int horizontal_amount, int vertical_amount);
		void scale(int horizontal_amount, int vertical_amount);
		void set(int x, int y);

		static const Vector2I zero;
	};

	Vector2I operator+ (const Vector2I& a, const Vector2I& b);
	Vector2I operator- (const Vector2I& a, const Vector2I& b);
	Vector2I operator* (const Vector2I& a, const Vector2I& b);
	Vector2I operator* (const Vector2I& a, int scale);
	Vector2I operator* (int scale, const Vector2I& a);
	// Per component, truncating toward zero like operator/=.
	Vector2I operator/ (const Vector2I& a, const Vector2I& b);
	Vector2I operator/ (const Vector2I& a, int scale);
}
