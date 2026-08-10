#include "engine/render/sprite_sheet_object.h"

#include <string>

using namespace mattmath;

namespace artattack
{
	SpriteSheetObject::SpriteSheetObject(const std::string& sheet_name,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		DrawObject(render_resources, color, rotation, origin,
			flip, layer_depth),
		sheet_(render_resources->resolve_sprite_sheet(sheet_name))
	{

	}

	const SpriteSheet* SpriteSheetObject::sprite_sheet() const
	{
		return this->render_resources()->sprite_sheet(this->sheet_);
	}

	void SpriteSheetObject::set_sprite_sheet(const std::string& sheet_name)
	{
		this->sheet_ = this->render_resources()->resolve_sprite_sheet(
			sheet_name);
	}
}
