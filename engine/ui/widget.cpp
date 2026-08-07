#include "engine/ui/widget.h"

using namespace DirectX;
using namespace MattMath;

#pragma region MObject

MObject::MObject(const std::string& name, bool hidden) :
	_name(name), _hidden(hidden)
{

}
const std::string& MObject::get_name() const
{
	return this->_name;
}
void MObject::draw(SpriteBatch* sprite_batch, const Viewport& viewport)
{
	this->draw(sprite_batch, Camera(viewport));
}
void MObject::set_hidden(bool hidden)
{
	this->_hidden = hidden;
}
bool MObject::get_hidden() const
{
	return this->_hidden;
}

#pragma endregion MObject

#pragma region MContainer

MContainer::MContainer(const std::string& name) :
	MObject(name)
{

}
void MContainer::add_child(MObject* child)
{
	this->_children.push_back(std::make_pair(child->get_name(), child));
}
void MContainer::remove_child(const std::string& name)
{
	for (auto it = this->_children.begin(); it != this->_children.end(); ++it)
	{
		if (it->first == name)
		{
			this->_children.erase(it);
			return;
		}
	}
}
void MContainer::remove_child(const MObject* child)
{
	for (auto it = this->_children.begin(); it != this->_children.end(); ++it)
	{
		if (it->second == child)
		{
			this->_children.erase(it);
			return;
		}
	}
}
void MContainer::remove_all_children()
{
	this->_children.clear();
}
size_t MContainer::get_child_count() const
{
	return this->_children.size();
}
std::vector<std::pair<std::string, MObject*>> MContainer::get_children()
{
	return this->_children;
}
void MContainer::scale_objects_to_new_resolution(
	const Vector2F& prev_resolution,
	const Vector2F& new_resolution)
{
	Vector2F scale_factor = new_resolution / prev_resolution;
	this->scale_size_and_position(scale_factor);
}
void MContainer::scale_size_and_position(const Vector2F& scale)
{
	for (auto const& child : this->_children)
	{
		child.second->scale_size_and_position(scale);
	}
}
void MContainer::update()
{
	for (auto const& child : this->_children)
	{
		child.second->update();
	}
}
void MContainer::draw(SpriteBatch* sprite_batch, const MattMath::Camera& camera)
{
	for (auto const& child : this->_children)
	{
		child.second->draw(sprite_batch, camera);
	}
}
void MContainer::draw(SpriteBatch* sprite_batch)
{
	for (auto const& child : this->_children)
	{
		child.second->draw(sprite_batch);
	}
}
bool MContainer::is_visible_in_viewport(const MattMath::RectangleF& view) const
{
	for (auto const& child : this->_children)
	{
		if (child.second->is_visible_in_viewport(view))
		{
			return true;
		}
	}
	return false;
}

#pragma endregion MContainer

#pragma region MWidget


MWidget::MWidget(const std::string& name, bool hidden) :
	MObject(name, hidden)
{

}

#pragma endregion MWidget

#pragma region MTexture

MTexture::MTexture(const std::string& name,
	const std::string& sheet_name,
	const std::string& frame_name,
	const RectangleF& rectangle,
	ResourceManager* resource_manager,
	const Colour& color,
	bool hidden,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	MWidget(name, hidden),
	TextureObject(sheet_name, frame_name,
		resource_manager, color, rotation, origin, effects, layer_depth),
	_rectangle(rectangle)
{

}
void MTexture::update()
{
	return;
}
bool MTexture::is_visible_in_viewport(const RectangleF& view) const
{
	// This called itself - infinite recursion, and /W4 has been reporting it
	// as C4717 the whole time. Test the widget's own rectangle, as Visual does.
	return this->_rectangle.intersects(view);
}
void MTexture::set_texture(const std::string& sheet_name,
	const std::string& frame_name)
{
	this->set_sprite_sheet_name(sheet_name);
	this->set_element_name(frame_name);
}
void MTexture::set_sprite_frame(const std::string& frame_name)
{
	this->set_element_name(frame_name);
}
void MTexture::set_colour(const Colour& colour)
{
	this->SpriteSheetObject::set_colour(colour);
}
void MTexture::scale_size_and_position(const Vector2F& scale)
{
	this->_rectangle.scale_size_and_position(scale);
}
void MTexture::draw(SpriteBatch* sprite_batch, const Camera& camera)
{
	if (this->get_hidden())
	{
		return;
	}
	this->TextureObject::draw(sprite_batch, this->_rectangle, camera);
}
void MTexture::draw(SpriteBatch* sprite_batch)
{
	if (this->get_hidden())
	{
		return;
	}
	this->TextureObject::draw(sprite_batch, this->_rectangle);
}
const RectangleF& MTexture::get_rectangle() const
{
	return this->_rectangle;
}
void MTexture::set_position(const Vector2F& position)
{
	this->_rectangle.set_position(position);
}
void MTexture::set_position_at_center(const Vector2F& position)
{
	this->_rectangle.set_position_at_center(position);
}
void MTexture::set_width(float width)
{
	this->_rectangle.set_width(width);
}
void MTexture::set_height(float height)
{
	this->_rectangle.set_height(height);
}
void MTexture::set_size(const Vector2F& size)
{
	this->_rectangle.set_size(size);
}
void MTexture::set_position_from_top_right_origin(const Vector2F& position)
{
	this->_rectangle.set_position_from_top_right(position);
}

#pragma endregion MTexture

#pragma region MText

MText::MText(const std::string& name,
	const std::string& text,
	const std::string& font_name,
	const Vector2F& position,
	ResourceManager* resource_manager,
	const Colour& color,
	bool hidden,
	float scale,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	MWidget(name, hidden),
	Text(text, font_name, position,
		resource_manager, color,
		scale, rotation, origin, effects, layer_depth)
{

}
void MText::scale_size_and_position(const Vector2F& scale)
{
	this->set_position(this->get_position() * scale);
	this->set_scale(this->get_scale() * scale.x);
}
void MText::draw(SpriteBatch* sprite_batch, const Camera& camera)
{
	if (this->get_hidden())
	{
		return;
	}
	this->Text::draw(sprite_batch, camera);
}
void MText::draw(SpriteBatch* sprite_batch)
{
	if (this->get_hidden())
	{
		return;
	}
	this->Text::draw(sprite_batch);

}
void MText::update()
{
	// do nothing
}
bool MText::is_visible_in_viewport(const RectangleF& /*view*/) const
{
	// TODO: implement text bounding box
	return true;
}
void MText::set_colour(const Colour& colour)
{
	this->Text::set_colour(colour);
}

#pragma endregion MText

#pragma region MTextDropShadow

MTextDropShadow::MTextDropShadow(const std::string& name,
	const std::string& text,
	const std::string& font_name,
	const Vector2F& position,
	ResourceManager* resource_manager,
	const Colour& color,
	const Colour& shadow_color,
	const Vector2F& shadow_offset,
	bool hidden,
	float scale,
	float shadow_scale,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	MWidget(name, hidden),
	TextDropShadow(text, font_name, position,
		resource_manager, color, shadow_color, shadow_offset,
		scale, shadow_scale, rotation, origin, effects, layer_depth)
{

}

void MTextDropShadow::scale_size_and_position(const Vector2F& scale)
{
	this->set_position(this->get_position() * scale);
	this->set_scale(this->get_scale() * scale.x);
	this->set_shadow_offset(this->get_shadow_offset() * scale);
	this->set_shadow_scale(this->get_shadow_scale() * scale.x);
}
void MTextDropShadow::draw(SpriteBatch* sprite_batch, const Camera& camera)
{
	if (this->get_hidden())
	{
		return;
	}
	this->TextDropShadow::draw(sprite_batch, camera);
}
void MTextDropShadow::draw(SpriteBatch* sprite_batch)
{
	if (this->get_hidden())
	{
		return;
	}
	this->TextDropShadow::draw(sprite_batch);

}
void MTextDropShadow::update()
{
	return;
}
bool MTextDropShadow::is_visible_in_viewport(const RectangleF& /*view*/) const
{
	// TODO: implement text bounding box
	return true;
}
void MTextDropShadow::set_colour(const Colour& colour)
{
	this->TextDropShadow::set_colour(colour);
}

#pragma endregion MTextDropShadow
