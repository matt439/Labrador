#pragma once

#include "engine/core/i_game_object.h"
#include "engine/render/texture_object.h"
#include "engine/render/text_drop_shadow.h"

namespace artattack
{
	class MObject : public IGameObject
	{
	public:
		MObject() = default;
		explicit MObject(const std::string& name, bool hidden = false);
		const std::string& name() const;

		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Viewport& viewport) const;

		void set_hidden(bool hidden);
		bool hidden() const;

		virtual void scale_size_and_position(const mattmath::Vector2F& scale) = 0;

		void update(float dt) override = 0;
		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const override = 0;
		bool is_visible_in_viewport(const mattmath::RectangleF& view) const override = 0;
	private:
		std::string name_ = "error_name";
		bool hidden_ = false;
	};

	class MContainer final : public MObject
	{
	public:
		MContainer() = default;
		explicit MContainer(const std::string& name);
		void add_child(MObject* child);
		void remove_child(const std::string& name);
		void remove_child(const MObject* child);
		void remove_all_children();
		size_t child_count() const;
		std::vector<std::pair<std::string, MObject*>> children();

		void scale_objects_to_new_resolution(
			const mattmath::Vector2F& prev_resolution,
			const mattmath::Vector2F& new_resolution);

		void scale_size_and_position(const mattmath::Vector2F& scale) override;

		void update(float dt) override;
		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const override;
		bool is_visible_in_viewport(const mattmath::RectangleF& view) const override;
	private:
		std::vector<std::pair<std::string, MObject*>> children_;
	};

	class MWidget : public MObject
	{
	public:
		MWidget() = default;
		explicit MWidget(const std::string& name, bool hidden = false);
		~MWidget() override = default;

		void scale_size_and_position(const mattmath::Vector2F& scale) override = 0;

		void update(float dt) override = 0;
		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const override = 0;
		bool is_visible_in_viewport(const mattmath::RectangleF& view) const override = 0;

		virtual void set_colour(const mattmath::Colour& colour) = 0;
	};

	class MTexture final : public MWidget, public TextureObject
	{
	public:
		MTexture() = default;
		MTexture(const std::string& name,
			const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleF& rectangle,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			bool hidden = false,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f);

		void scale_size_and_position(const mattmath::Vector2F& scale) override;

		void update(float dt) override;
		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const override;
		bool is_visible_in_viewport(const mattmath::RectangleF& view) const override;

		void set_texture(const std::string& sheet_name, const std::string& frame_name);
		void set_sprite_frame(const std::string& frame_name);
		void set_colour(const mattmath::Colour& colour) override;
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

	class MText final : public MWidget, public Text
	{
	public:
		MText() = default;
		MText(const std::string& name,
			const std::string& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			bool hidden = false,
			float scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f);

		void scale_size_and_position(const mattmath::Vector2F& scale) override;

		void update(float dt) override;
		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const override;
		bool is_visible_in_viewport(const mattmath::RectangleF& view) const override;
		void set_colour(const mattmath::Colour& colour) override;
	};

	class MTextDropShadow final : public MWidget, public TextDropShadow
	{
	public:
		MTextDropShadow() = default;
		MTextDropShadow(const std::string& name,
			const std::string& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			const mattmath::Colour& shadow_color = colour_consts::BLACK,
			const mattmath::Vector2F& shadow_offset = { 2.0f, 2.0f },
			bool hidden = false,
			float scale = 1.0f,
			float shadow_scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f);

		void scale_size_and_position(const mattmath::Vector2F& scale) override;

		void update(float dt) override;
		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const override;
		bool is_visible_in_viewport(const mattmath::RectangleF& view) const override;
		void set_colour(const mattmath::Colour& colour) override;
	};
}
