#include "engine/render/sprite_sheet_object.h"

using namespace DirectX;
using namespace MattMath;

SpriteSheetObject::SpriteSheetObject(const std::string& sheet_name,
	RenderResources* render_resources,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	DrawObject(render_resources, color, rotation, origin,
		effects, layer_depth),
	_sheet(render_resources->resolve_sprite_sheet(sheet_name))
{

}

SpriteSheet* SpriteSheetObject::get_sprite_sheet() const
{
	return this->get_render_resources()->get_sprite_sheet(this->_sheet);
}

void SpriteSheetObject::set_sprite_sheet(const std::string& sheet_name)
{
	this->_sheet = this->get_render_resources()->resolve_sprite_sheet(
		sheet_name);
}
