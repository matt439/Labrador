#pragma once

#include "engine/core/game_object.h"
#include "engine/render/text.h"
#include "engine/render/texture_object.h"
#include "engine/render/colour.h"
#include "engine/render/camera.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <string>
#include <vector>

namespace labrador
{
	// A named, hideable thing in a user interface.
	//
	// Geometry stays in the game's authored space. The game owns layout (T1)
	// and maps the menu to screen space through DrawList::set_camera in
	// engine/render/renderer.h; widgets do not recursively rescale themselves.
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
		RectangleF bounds() const override = 0;

		// No-op for an object without colour. Containers can forward colour to
		// all their children without testing each child's type.
		virtual void set_colour(const Colour& colour);
	private:
		std::string name_ = "error_name";
		bool hidden_ = false;
	};

	// A colourable UiObject accepted by FocusGroup in engine/ui/focus.h.
	class UiWidget : public UiObject
	{
	public:
		UiWidget() = default;
		explicit UiWidget(const std::string& name, bool hidden = false);
		~UiWidget() override = default;

		void update(float dt) override = 0;
		void draw(DrawList& draw_list) const override = 0;
		RectangleF bounds() const override = 0;

		// Widgets implement colouring rather than inheriting UiObject's no-op.
		void set_colour(const Colour& colour) override = 0;
	};

	// A compound widget that updates, draws and colours its children in insertion
	// order. It can be focused as one row and derived from to own its parts
	// (docs/design/PHILOSOPHY.md, UI).
	//
	// Child pointers are borrowed and must remain alive while operations can
	// reach them. A derived class can own child members and add them from its
	// constructor body; container destruction drops pointers without reading
	// children, so those members may be destroyed before the base container.
	class UiContainer : public UiWidget
	{
	public:
		UiContainer() = default;
		explicit UiContainer(const std::string& name);
		// child must be non-null. Stores its name and borrows the pointer.
		void add_child(UiObject* child);
		// Remove the first matching child, without destroying it. No-op if absent.
		void remove_child(const std::string& name);
		void remove_child(const UiObject* child);
		size_t child_count() const;
		// A copy of the list; its pointers remain borrowed.
		std::vector<std::pair<std::string, UiObject*>> children();

		// Hidden children still update; hiding a container suppresses its draw.
		void update(float dt) override;
		void draw(DrawList& draw_list) const override;
		// Union of child bounds, skipping width or height <= 0. Returns
		// RectangleF::zero when no child contributes.
		RectangleF bounds() const override;
		RectangleF cull_bounds(float units_per_pixel) const override;

		// Forwarded to every child, including hidden ones, in insertion order.
		// Nested containers forward it; children without colour ignore it.
		void set_colour(const Colour& colour) override;
	private:
		std::vector<std::pair<std::string, UiObject*>> children_;
	};

	// Leaf widgets are open to derivation (docs/design/PHILOSOPHY.md, UI) and
	// safe to delete through UiWidget*. An overriding draw() must check hidden()
	// itself: it replaces the leaf's check rather than running after it.
	class UiTexture : public UiWidget, public TextureObject
	{
	public:
		UiTexture() = default;
		UiTexture(const std::string& name,
			const std::string& sheet_name,
			const std::string& frame_name,
			const RectangleF& rectangle,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			bool hidden = false,
			float rotation = 0.0f,
			const Vector2F& origin = Vector2F::zero,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;
		RectangleF bounds() const override;
		RectangleF cull_bounds(float units_per_pixel) const override;

		void set_sprite_frame(const std::string& frame_name);
		void set_colour(const Colour& colour) override;
		void set_position(const Vector2F& position);
		void set_position_at_center(const Vector2F& position);
		void set_width(float width);
		void set_height(float height);
		void set_size(const Vector2F& size);

		const RectangleF& rectangle() const;
	private:
		RectangleF rectangle_ = RectangleF::zero;

		// Hide TextureObject's positioned draws: widgets draw at rectangle().
		// A deriving class may call them with TextureObject:: qualification.
		using TextureObject::draw;
	};

	class UiText : public UiWidget, public Text
	{
	public:
		UiText() = default;
		UiText(const std::string& name,
			const std::wstring& text,
			const std::string& font_name,
			const Vector2F& position,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			bool hidden = false,
			float scale = 1.0f,
			float rotation = 0.0f,
			const Vector2F& origin = Vector2F::zero,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;
		RectangleF bounds() const override;
		void set_colour(const Colour& colour) override;
	};
}
