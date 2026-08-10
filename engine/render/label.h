#pragma once

#include "engine/core/game_object.h"
#include "engine/render/text.h"
#include "engine/render/colour.h"

#include <string>

namespace artattack
{
	// A string a scene can hold, which is what Text on its own is not.
	//
	// Text is the drawing half - a wide string, a font handle, a measurement
	// and a draw(). What it has never had is the three things
	// engine/core/game_object.h asks for: something to step, an extent to cull
	// against, and the GameObject base that lets a container hold it next to a
	// sprite. Every screen that wanted text therefore kept its own
	// unique_ptr<Text> and its own loop over it, which is what the sample did
	// and what every menu page did before engine/ui.
	//
	// It is a separate class rather than a base added to Text, and that is not
	// a style choice: engine/ui pairs a GameObject-derived UiWidget with the
	// drawable (UiText is UiWidget plus Text), so a GameObject base on Text
	// would give UiText two of them. This is the same pairing Visual already
	// is for sprites - GameObject plus TextureObject - and for the same reason.
	class Label final : public GameObject, public Text
	{
	public:
		Label() = default;
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
		// it, this class is abstract and the call is ambiguous. Visual does the
		// same thing for the same reason.
		void draw(DrawList& draw_list) const override;

		// The measured box, taken when the string or the scale changed and not
		// here: this is the culling path, and measuring walks the string.
		mattmath::RectangleF bounds() const override;
	};
}
