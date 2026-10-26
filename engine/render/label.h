#pragma once

#include "engine/core/game_object.h"
#include "engine/render/text.h"
#include "engine/render/colour.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <string>

namespace labrador
{
	// A string a scene can hold, which is what Text on its own is not.
	//
	// Text is the drawing half - a wide string, a font handle, a measurement
	// and a draw(). What it does not have is the three things
	// engine/core/game_object.h asks for: something to step, an extent to cull
	// against, and the GameObject base that lets a container hold it next to a
	// sprite.
	//
	// It is a separate class rather than a base added to Text, and that is not
	// a style choice: engine/ui pairs a GameObject-derived UiWidget with the
	// drawable (UiText is UiWidget plus Text), so a GameObject base on Text
	// would give UiText two of them. This is the same pairing Visual already
	// is for sprites - GameObject plus TextureObject - and for the same reason.
	//
	// IT HAS NO HIDDEN FLAG, AND SCENE TEXT THAT HIDES IS A UiText. A string a
	// scene holds that must stop drawing while keeping its words, its box and
	// its colour is a UiText (engine/ui/widget.h), whose set_hidden skips the
	// draw and leaves bounds() alone. For a label whose box may go empty,
	// set_text(L"") is the supported way to hide it; it re-measures on every
	// call, so a caller that would do it every frame gates it on a change, as
	// LineSweeper's play state does. Naming UiText here is a pointer and not a
	// dependency: engine/render does not include engine/ui.
	class Label final : public GameObject, public Text
	{
	public:
		Label() = default;

		// `font_name` names a loaded font in `render_resources`, normally
		// Application::render_resources(). That table is borrowed and must
		// outlive this object. An unknown font name throws std::out_of_range.
		//
		// `position` locates the text's origin in world coordinates. `origin`
		// is the offset from the layout's top-left in unscaled font texels;
		// its default zero anchors that corner at `position`. Glyph bearings
		// can offset the visible ink. `scale` scales the font's pixels, and
		// `rotation` is in radians, clockwise on screen.
		Label(const std::wstring& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const Colour& colour = Colour::white,
			float scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float layer_depth = 0.0f);

		// Nothing to step. A label that animates is a label something else
		// moves, which is what set_position is for.
		void update(float dt) override;

		// One declaration overriding two virtuals, because GameObject::draw and
		// TextObject::draw have the same signature in unrelated bases - without
		// it, this class is abstract and the call is ambiguous.
		void draw(DrawList& draw_list) const override;

		// The drawn box: TextObject::text_bounds, which scales and turns the
		// stored measurement at the current scale and rotation. The measurement
		// is taken when the string or the font changes and not here: this is
		// the culling path, and measuring walks the string. A scale or rotation
		// change takes no measurement and needs none.
		mattmath::RectangleF bounds() const override;
	};
}
