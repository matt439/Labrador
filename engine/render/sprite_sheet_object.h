#ifndef SPRITESHEETOBJECT_H
#define SPRITESHEETOBJECT_H

#include "engine/render/draw_object.h"

class SpriteSheetObject : public DrawObject
{
public:
	SpriteSheetObject() = default;
	SpriteSheetObject(const std::string& sheet_name,
		const std::string& element_name,
		RenderResources* render_resources,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);
protected:
	virtual const std::string& get_sprite_sheet_name() const;
	virtual const std::string& get_element_name() const;
	virtual SpriteSheet* get_sprite_sheet() const;
	virtual void set_sprite_sheet_name(const std::string& sheet_name);
	virtual void set_element_name(const std::string& element_name);
private:
	std::string _sheet_name = "";
	std::string _element_name = "";
};
#endif // !SPRITESHEETOBJECT_H
