#pragma once

#include "engine/core/game_object.h"
#include "engine/render/texture_object.h"
#include "engine/render/text_drop_shadow.h"
#include "engine/render/colour.h"

namespace artattack
{
	// A named, hideable thing in a user interface.
	//
	// NO RESCALE. This class used to require every widget to implement
	// scale_size_and_position(), and UiContainer shipped a
	// scale_objects_to_new_resolution() that walked the tree applying it. That
	// is a layout policy, and an engine base class is not where a game's
	// answer to "what happens at 1280x720" belongs (T1). It was also
	// destructive: the factor was recorded nowhere, so after the walk there was
	// no authoritative geometry left to write against, and any later absolute
	// setter wrote design-space units into a rescaled widget. The results
	// screen did exactly that every frame - its team bars were 1.5x too wide
	// for the box they sat in at the default resolution, because the box had
	// been shrunk and the bars had not.
	//
	// A widget now holds one geometry, in whatever space the game authored it
	// in, and nothing ever rewrites it. The mapping to the screen is a Camera,
	// and it is set on the draw list once for the whole menu rather than passed
	// to every widget in turn (renderer.h, DrawList::set_camera).
	class UiObject : public GameObject
	{
	public:
		UiObject() = default;
		explicit UiObject(const std::string& name, bool hidden = false);
		const std::string& name() const;

		void set_hidden(bool hidden);
		bool hidden() const;

		void update(float dt) override = 0;
		void draw(DrawList& draw_list) const override = 0;
		mattmath::RectangleF bounds() const override = 0;
	private:
		std::string name_ = "error_name";
		bool hidden_ = false;
	};

	class UiContainer final : public UiObject
	{
	public:
		UiContainer() = default;
		explicit UiContainer(const std::string& name);
		void add_child(UiObject* child);
		void remove_child(const std::string& name);
		void remove_child(const UiObject* child);
		void remove_all_children();
		size_t child_count() const;
		std::vector<std::pair<std::string, UiObject*>> children();

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;
		mattmath::RectangleF bounds() const override;
	private:
		std::vector<std::pair<std::string, UiObject*>> children_;
	};

	class UiWidget : public UiObject
	{
	public:
		UiWidget() = default;
		explicit UiWidget(const std::string& name, bool hidden = false);
		~UiWidget() override = default;

		void update(float dt) override = 0;
		void draw(DrawList& draw_list) const override = 0;
		mattmath::RectangleF bounds() const override = 0;

		virtual void set_colour(const Colour& colour) = 0;
	};

	class UiTexture final : public UiWidget, public TextureObject
	{
	public:
		UiTexture() = default;
		UiTexture(const std::string& name,
			const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleF& rectangle,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			bool hidden = false,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;
		mattmath::RectangleF bounds() const override;

		void set_texture(const std::string& sheet_name, const std::string& frame_name);
		void set_sprite_frame(const std::string& frame_name);
		void set_colour(const Colour& colour) override;
		void set_position(const mattmath::Vector2F& position);
		void set_position_at_center(const mattmath::Vector2F& position);
		void set_width(float width);
		void set_height(float height);
		void set_size(const mattmath::Vector2F& size);
		void set_position_from_top_right_origin(const mattmath::Vector2F& position);

		const mattmath::RectangleF& rectangle() const;
	private:
		mattmath::RectangleF rectangle_ = mattmath::RectangleF::ZERO;
	};

	class UiText final : public UiWidget, public Text
	{
	public:
		UiText() = default;
		UiText(const std::string& name,
			const std::wstring& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			bool hidden = false,
			float scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;
		mattmath::RectangleF bounds() const override;
		void set_colour(const Colour& colour) override;
	};

	class UiTextDropShadow final : public UiWidget, public TextDropShadow
	{
	public:
		UiTextDropShadow() = default;
		UiTextDropShadow(const std::string& name,
			const std::wstring& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			const Colour& shadow_color = Colour::black,
			const mattmath::Vector2F& shadow_offset = { 2.0f, 2.0f },
			bool hidden = false,
			float scale = 1.0f,
			float shadow_scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;
		mattmath::RectangleF bounds() const override;
		void set_colour(const Colour& colour) override;
	};
}
