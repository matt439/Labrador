#include "engine/ui/widget.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <string>
#include <vector>

using namespace mattmath;

#pragma region UiObject

namespace artattack
{
	UiObject::UiObject(const std::string& name, bool hidden) :
		name_(name), hidden_(hidden)
	{

	}
	const std::string& UiObject::name() const
	{
		return this->name_;
	}
	void UiObject::set_hidden(bool hidden)
	{
		this->hidden_ = hidden;
	}
	bool UiObject::hidden() const
	{
		return this->hidden_;
	}
	void UiObject::set_colour(const Colour& /*colour*/)
	{
		// Nothing, and deliberately not pure: see the header. A container
		// forwards to children it knows as UiObjects, and one of those having
		// no colour is an answer rather than a case to detect.
	}

#pragma endregion UiObject

#pragma region UiWidget

	UiWidget::UiWidget(const std::string& name, bool hidden) :
		UiObject(name, hidden)
	{

	}

#pragma endregion UiWidget

#pragma region UiContainer

	UiContainer::UiContainer(const std::string& name) :
		UiWidget(name)
	{

	}
	void UiContainer::add_child(UiObject* child)
	{
		this->children_.push_back(std::make_pair(child->name(), child));
	}
	void UiContainer::remove_child(const std::string& name)
	{
		for (auto it = this->children_.begin(); it != this->children_.end(); ++it)
		{
			if (it->first == name)
			{
				this->children_.erase(it);
				return;
			}
		}
	}
	void UiContainer::remove_child(const UiObject* child)
	{
		for (auto it = this->children_.begin(); it != this->children_.end(); ++it)
		{
			if (it->second == child)
			{
				this->children_.erase(it);
				return;
			}
		}
	}
	void UiContainer::remove_all_children()
	{
		this->children_.clear();
	}
	size_t UiContainer::child_count() const
	{
		return this->children_.size();
	}
	std::vector<std::pair<std::string, UiObject*>> UiContainer::children()
	{
		return this->children_;
	}
	void UiContainer::update(float dt)
	{
		for (auto const& child : this->children_)
		{
			child.second->update(dt);
		}
	}
	void UiContainer::draw(DrawList& draw_list) const
	{
		// Every other widget in this header early-outs on hidden(); this one
		// did not, so set_hidden(true) on a container did nothing at all and
		// the caller had no way to tell. Hiding a subtree is the only reason
		// a container would carry the flag.
		if (this->hidden())
		{
			return;
		}
		for (auto const& child : this->children_)
		{
			child.second->draw(draw_list);
		}
	}
	RectangleF UiContainer::bounds() const
	{
		// The union of the children's, which is the recursive definition the
		// predicate form could only approximate by asking every child in turn.
		// An empty container occupies nothing.
		//
		// Empty-boxed children are skipped rather than unioned. RectangleF::ZERO
		// is a point at the world origin, so an empty nested container used to
		// drag its parent's box all the way back to (0,0) - a parent of one
		// label at (900,400) and one empty container reported a box from the
		// origin to the label. Culling reads this, and so does the navigation
		// walk, which is why the harmless-looking union is not harmless.
		RectangleF result = RectangleF::ZERO;
		bool any = false;
		for (auto const& child : this->children_)
		{
			const RectangleF child_bounds = child.second->bounds();
			if (child_bounds.width <= 0.0f || child_bounds.height <= 0.0f)
			{
				continue;
			}
			result = any ? RectangleF::union_of(result, child_bounds)
				: child_bounds;
			any = true;
		}
		return result;
	}
	void UiContainer::set_colour(const Colour& colour)
	{
		// No hidden() check, unlike draw(). A hidden subtree that is about to
		// be shown should already be the colour it was told to be, and a focus
		// group has no idea which of the widgets it holds are visible.
		for (auto const& child : this->children_)
		{
			child.second->set_colour(colour);
		}
	}

#pragma endregion UiContainer

#pragma region UiTexture

	UiTexture::UiTexture(const std::string& name,
		const std::string& sheet_name,
		const std::string& frame_name,
		const RectangleF& rectangle,
		RenderResources* render_resources,
		const Colour& color,
		bool hidden,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		UiWidget(name, hidden),
		TextureObject(sheet_name, frame_name,
			render_resources, color, rotation, origin, flip, layer_depth),
		rectangle_(rectangle)
	{

	}
	void UiTexture::update(float /*dt*/)
	{
		return;
	}
	RectangleF UiTexture::bounds() const
	{
		// This used to call itself - infinite recursion, and /W4 had been
		// reporting it as C4717 the whole time.
		return this->rectangle_;
	}
	void UiTexture::set_texture(const std::string& sheet_name,
		const std::string& frame_name)
	{
		this->set_frame(sheet_name, frame_name);
	}
	void UiTexture::set_sprite_frame(const std::string& frame_name)
	{
		this->set_frame(frame_name);
	}
	void UiTexture::set_colour(const Colour& colour)
	{
		this->SpriteSheetObject::set_colour(colour);
	}
	void UiTexture::draw(DrawList& draw_list) const
	{
		if (this->hidden())
		{
			return;
		}
		this->TextureObject::draw(draw_list, this->rectangle_);
	}
	const RectangleF& UiTexture::rectangle() const
	{
		return this->rectangle_;
	}
	void UiTexture::set_position(const Vector2F& position)
	{
		this->rectangle_.set_position(position);
	}
	void UiTexture::set_position_at_center(const Vector2F& position)
	{
		this->rectangle_.set_position_at_center(position);
	}
	void UiTexture::set_width(float width)
	{
		this->rectangle_.set_width(width);
	}
	void UiTexture::set_height(float height)
	{
		this->rectangle_.set_height(height);
	}
	void UiTexture::set_size(const Vector2F& size)
	{
		this->rectangle_.set_size(size);
	}
	void UiTexture::set_position_from_top_right_origin(const Vector2F& position)
	{
		this->rectangle_.set_position_from_top_right(position);
	}

#pragma endregion UiTexture

#pragma region UiText

	UiText::UiText(const std::string& name,
		const std::wstring& text,
		const std::string& font_name,
		const Vector2F& position,
		RenderResources* render_resources,
		const Colour& color,
		bool hidden,
		float scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth) :
		UiWidget(name, hidden),
		Text(text, font_name, position,
			render_resources, color,
			scale, rotation, origin, layer_depth)
	{

	}
	void UiText::draw(DrawList& draw_list) const
	{
		if (this->hidden())
		{
			return;
		}
		this->Text::draw(draw_list);
	}
	void UiText::update(float /*dt*/)
	{
		// do nothing
	}
	RectangleF UiText::bounds() const
	{
		// Was "TODO: implement text bounding box; return true". The TODO was
		// only ever answerable once the interface asked for an extent instead
		// of a yes/no - there is no honest "true" to return, but there is an
		// honest box.
		return this->text_bounds();
	}
	void UiText::set_colour(const Colour& colour)
	{
		this->Text::set_colour(colour);
	}

#pragma endregion UiText

#pragma region UiTextDropShadow

	UiTextDropShadow::UiTextDropShadow(const std::string& name,
		const std::wstring& text,
		const std::string& font_name,
		const Vector2F& position,
		RenderResources* render_resources,
		const Colour& color,
		const Colour& shadow_color,
		const Vector2F& shadow_offset,
		bool hidden,
		float scale,
		float shadow_scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth) :
		UiWidget(name, hidden),
		TextDropShadow(text, font_name, position,
			render_resources, color, shadow_color, shadow_offset,
			scale, shadow_scale, rotation, origin, layer_depth)
	{

	}

	void UiTextDropShadow::draw(DrawList& draw_list) const
	{
		if (this->hidden())
		{
			return;
		}
		this->TextDropShadow::draw(draw_list);
	}
	void UiTextDropShadow::update(float /*dt*/)
	{
		return;
	}
	RectangleF UiTextDropShadow::bounds() const
	{
		// Both draws, not just the text: the shadow is offset and can carry its
		// own scale, so it is measured at its own position rather than assumed
		// to sit inside the text's box.
		return RectangleF::union_of(
			this->text_bounds(),
			this->text_bounds_at(this->position() + this->shadow_offset(),
				this->shadow_scale()));
	}
	void UiTextDropShadow::set_colour(const Colour& colour)
	{
		this->TextDropShadow::set_colour(colour);
	}

#pragma endregion UiTextDropShadow
}
