#include "engine/render/render_resources.h"

#include "engine/math/vector2f.h"

#include <memory>
#include <string>
#include <string_view>

using namespace mattmath;

namespace labrador
{
	// The half of RenderResources that names no backend type, compiled once.
	//
	// It was compiled three times until now - once per backend, from three
	// files that said so in their own comments and stayed word for word
	// identical anyway. What kept them there was that every one of these
	// methods reaches a table, and all three tables lived on Impl, which is a
	// complete type only inside a backend folder. Two of those tables hold
	// engine data and did not need to be there; render_resources.h says why at
	// length. They are members of RenderResources now, so this file can exist.
	//
	// WHAT IS STILL THREE TIMES, and it is the honest remainder rather than an
	// oversight: the constructor, the destructor, the moves, resolve_texture
	// and release_device_resources. Every one of them either touches the
	// texture table or needs Impl complete in order to make or destroy one -
	// which is the rule this class states, arriving at a file list instead of
	// at a paragraph.

	void RenderResources::add_sprite_sheet(const std::string& sprite_sheet_name,
		std::unique_ptr<SpriteSheet> sprite_sheet)
	{
		this->sprite_sheets_.add(sprite_sheet_name, std::move(sprite_sheet));
	}

	void RenderResources::add_font(const std::string& font_name,
		std::unique_ptr<Font> font)
	{
		this->fonts_.add(font_name, std::move(font));
	}

	FontHandle RenderResources::resolve_sprite_font(
		const std::string& font_name) const
	{
		return this->fonts_.resolve(font_name);
	}

	RenderResources::SpriteSheetHandle RenderResources::resolve_sprite_sheet(
		const std::string& sprite_sheet_name) const
	{
		return this->sprite_sheets_.resolve(sprite_sheet_name);
	}

	const SpriteSheet* RenderResources::sprite_sheet(
		SpriteSheetHandle sprite_sheet) const
	{
		return this->sprite_sheets_.get(sprite_sheet);
	}

	const SpriteSheet* RenderResources::sprite_sheet(
		const std::string& sprite_sheet_name) const
	{
		return this->sprite_sheets_.get(sprite_sheet_name);
	}

	const Font* RenderResources::font(FontHandle font) const
	{
		return this->fonts_.get(font);
	}

	// The three below forward to arithmetic in engine/render/font.cpp and
	// perform none themselves, which is what made having three copies of them
	// survivable and is not what makes one copy right. measure_text is a term
	// of the pixel contract; a backend that answered it differently would move
	// every glyph a client had laid out against it.

	Vector2F RenderResources::measure_text(FontHandle font,
		const std::wstring& text) const
	{
		return this->font(font)->measure(text);
	}

	bool RenderResources::can_render(FontHandle font,
		std::wstring_view text) const
	{
		return this->first_unrenderable(font, text) == std::wstring_view::npos;
	}

	size_t RenderResources::first_unrenderable(FontHandle font,
		std::wstring_view text) const
	{
		return this->font(font)->first_unrenderable(text);
	}
}
