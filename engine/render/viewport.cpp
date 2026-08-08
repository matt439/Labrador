#include "engine/render/viewport.h"

using namespace mattmath;

namespace artattack
{
	Viewport::Viewport(float x, float y, float width, float height,
		float minDepth, float maxDepth) :
		x(x), y(y), width(width), height(height),
		minDepth(minDepth), maxDepth(maxDepth)
	{

	}
	Viewport::Viewport(const RectangleF& rectangle,
		float minDepth, float maxDepth)
	{
		this->x = rectangle.x;
		this->y = rectangle.y;
		this->width = rectangle.width;
		this->height = rectangle.height;
		this->minDepth = minDepth;
		this->maxDepth = maxDepth;
	}
	Viewport::Viewport(const RectangleI& rectangle,
		float minDepth, float maxDepth)
	{
		this->x = static_cast<float>(rectangle.x);
		this->y = static_cast<float>(rectangle.y);
		this->width = static_cast<float>(rectangle.width);
		this->height = static_cast<float>(rectangle.height);
		this->minDepth = minDepth;
		this->maxDepth = maxDepth;
	}
	RectangleF Viewport::rectangle() const
	{
		return RectangleF(this->x, this->y, this->width, this->height);
	}
	//RectangleF Viewport::rectangle(float minDepth, float maxDepth) const
	//{
	//	return RectangleF(this->x, this->y, this->width, this->height);
	//}
	Vector2F Viewport::position() const
	{
		return Vector2F(this->x, this->y);
	}
	Vector2F Viewport::size() const
	{
		return Vector2F(this->width, this->height);
	}
	Viewport& Viewport::operator=(const mattmath::RectangleF& rectangle)
	{
		this->x = rectangle.x;
		this->y = rectangle.y;
		this->width = rectangle.width;
		this->height = rectangle.height;
		return *this;
	}
	Viewport& Viewport::operator=(const mattmath::RectangleI& rectangle)
	{
		this->x = static_cast<float>(rectangle.x);
		this->y = static_cast<float>(rectangle.y);
		this->width = static_cast<float>(rectangle.width);
		this->height = static_cast<float>(rectangle.height);
		return *this;
	}
	bool Viewport::operator==(const Viewport& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->width == other.width &&
			this->height == other.height &&
			this->minDepth == other.minDepth &&
			this->maxDepth == other.maxDepth;
	}
	bool Viewport::operator!=(const Viewport& other) const
	{
		return !(*this == other);
	}
}
