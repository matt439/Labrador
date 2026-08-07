#include "engine/render/sprite_sheet_object.h"

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
	SpriteSheetObject::SpriteSheetObject(const std::string& sheet_name,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteEffects effects,
		float layer_depth) :
		DrawObject(render_resources, color, rotation, origin,
			effects, layer_depth),
		sheet_(render_resources->resolve_sprite_sheet(sheet_name))
	{

	}

	SpriteSheet* SpriteSheetObject::sprite_sheet() const
	{
		return this->render_resources()->sprite_sheet(this->sheet_);
	}

	void SpriteSheetObject::set_sprite_sheet(const std::string& sheet_name)
	{
		this->sheet_ = this->render_resources()->resolve_sprite_sheet(
			sheet_name);
	}
}
