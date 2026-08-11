#pragma once


namespace mattmath
{
	struct Vector2F;

	struct Vector2I
	{
		int x = 0;
		int y = 0;

		Vector2I() = default;
		Vector2I(const Vector2I&) = default;
		Vector2I(int x, int y);
		explicit Vector2I(const mattmath::Vector2F& vector);

		bool operator==(const Vector2I& other) const;
		bool operator!=(const Vector2I& other) const;

		Vector2I& operator+=(const Vector2I& other);
		Vector2I& operator-=(const Vector2I& other);
		Vector2I& operator*=(const Vector2I& other);
		Vector2I& operator/=(const Vector2I& other);
		Vector2I& operator*=(int other);
		Vector2I& operator/=(int other);

		void offset(int horizontal_amount, int vertical_amount);
		void scale(int horizontal_amount, int vertical_amount);
		void set(int x, int y);

		static const Vector2I ZERO;
	};

	Vector2I operator+ (const Vector2I& V1, const Vector2I& V2);
	Vector2I operator- (const Vector2I& V1, const Vector2I& V2);
	Vector2I operator* (const Vector2I& V1, const Vector2I& V2);	
	Vector2I operator* (const Vector2I& V, int S);
	Vector2I operator/ (const Vector2I& V1, const Vector2I& V2);
	Vector2I operator/ (const Vector2I& V, int S);
	Vector2I operator* (int S, const Vector2I& V);
}
