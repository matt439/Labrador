#include "engine/render/visual.h"

using namespace MattMath;
using namespace DirectX;

Visual::Visual(const std::string& sheet_name,
	const std::string& frame_name,
	const RectangleF& rectangle,
	ResourceManager* resource_manager,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	TextureObject(sheet_name, frame_name, resource_manager,
		color, rotation, origin, effects, layer_depth),
	_rectangle(rectangle)
{
}

Visual::Visual(const std::string& sheet_name,
	const std::string& frame_name,
	const RectangleRotated& rect_rotated,
	ResourceManager* resource_manager,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	TextureObject(sheet_name, frame_name, resource_manager,
		color, rotation, origin, effects, layer_depth)
{
	this->_rectangle = rect_rotated.get_rectangle_rotated_to_axis();
}


void Visual::update()
{
	// do nothing
}
void Visual::draw(SpriteBatch* sprite_batch, const Camera& camera)
{
	this->TextureObject::draw(sprite_batch, this->_rectangle, camera);
}
void Visual::draw(SpriteBatch* sprite_batch)
{
	this->TextureObject::draw(sprite_batch, this->_rectangle);
}
bool Visual::is_visible_in_viewport(const RectangleF& view) const
{
	return this->_rectangle.intersects(view);
}