#pragma once

#include "engine/core/game_object.h"
#include "engine/render/texture_object.h"
#include "engine/render/text_drop_shadow.h"
#include "engine/render/colour.h"
#include "engine/render/camera.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <string>
#include <vector>

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

		// Painting a thing that has no colour is nothing, not an error, which
		// is why this is a no-op here and pure one level down.
		//
		// It is on this class at all so that a container can forward it to
		// children it knows only as UiObjects. The two alternatives are both
		// worse: a container that asked at runtime which of its children were
		// colourable would be putting a cast on the path, and a container that
		// could only hold colourable children would be a narrower container
		// than anyone asked for.
		virtual void set_colour(const Colour& colour);
	private:
		std::string name_ = "error_name";
		bool hidden_ = false;
	};

	// A UiObject that has a colour, which is what the focus machinery
	// traffics in: FocusGroup::add takes one of these, because paint is how a
	// menu shows where the cursor is (focus.h).
	class UiWidget : public UiObject
	{
	public:
		UiWidget() = default;
		explicit UiWidget(const std::string& name, bool hidden = false);
		~UiWidget() override = default;

		void update(float dt) override = 0;
		void draw(DrawList& draw_list) const override = 0;
		mattmath::RectangleF bounds() const override = 0;

		// Pure again. UiObject's no-op is for a thing that is drawn and not
		// coloured; a widget is the other kind, and being the other kind is
		// the whole of what this class adds.
		void set_colour(const Colour& colour) override = 0;
	};

	// A widget that is its children.
	//
	// IT IS A UiWidget, AND USED TO BE A UiObject, which is the difference
	// between a compound widget being writable and not. FocusGroup::add takes
	// a UiWidget*, so a container could not be focused - and a row that is a
	// label and a value side by side, which is what every options screen is
	// made of, is exactly a container the cursor lands on. A client without
	// this recovers the label-to-value relationship by comparing the focused
	// pointer against each label in turn, and cannot write a slider at all.
	//
	// SO IT IS NOT final EITHER, and deriving from it is how a compound
	// widget is written: build the parts in the constructor, hand them to
	// add_child, and inherit update, draw, bounds and set_colour already
	// written. set_colour reaches every child, so a focus group holding one of
	// these paints the whole row.
	//
	// Children are loans, as they were, and the container outlives nothing. A
	// deriving class that owns its children holds them as members and calls
	// add_child from its constructor body: members are built after the base,
	// so there is nothing to add during the initialiser list, and destruction
	// runs the other way - the container is emptied before the members it was
	// pointing at go.
	class UiContainer : public UiWidget
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

		// Every child, in the order they were added, and whatever a child
		// makes of it - a nested container passes it down, and a child with no
		// colour ignores it.
		void set_colour(const Colour& colour) override;
	private:
		std::vector<std::pair<std::string, UiObject*>> children_;
	};

	// THE THREE LEAVES ARE NOT final, and it is worth saying because they
	// were, with no reason given anywhere - which reads as unconsidered rather
	// than decided. A client wanting a text object with one extra behaviour -
	// a line that builds itself from a list of parts, say - had to compose one
	// and forward update, draw, bounds and set_colour to it by hand: four
	// functions of boilerplate to add one.
	//
	// Deriving costs nothing structural. The destructor is virtual from
	// GameObject down, so these are safe to hold and delete as UiWidget*, and
	// each leaf is that plus a render-side base that already does the drawing.
	//
	// WHAT A DERIVING CLASS OWES: if it overrides draw(), it early-outs on
	// hidden() itself. That check is written into each leaf's draw rather than
	// into a wrapper around it, so an override replaces it rather than
	// running after it.
	class UiTexture : public UiWidget, public TextureObject
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

	class UiText : public UiWidget, public Text
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

	class UiTextDropShadow : public UiWidget, public TextDropShadow
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
