#pragma once

#include "engine/core/handle.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_sheet.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>
#include <string_view>

namespace artattack
{
	// Everything drawing reads from, cached by name: the textures the GPU holds,
	// the fonts built from them, and the sheets that index them. Named resources
	// in, handles out - the loader that fills it decides what a name means and
	// where the bytes came from.
	//
	// BEHIND THE SEAM, and this is why the pimpl. The public API here was
	// already handles, but the storage was ComPtr<ID3D11ShaderResourceView> and
	// SpriteFont, so every translation unit that resolved a name compiled
	// <SpriteFont.h> - which meant a headless test could not construct a
	// TextObject even after draw() was clean, because TextObject's *constructor*
	// resolves a font and measures a string. Renderer holds an Impl the backend
	// defines; so does this, for the same reason and in the same folder.
	//
	// The load-time half - add_texture, add_sprite_font, the device-loss resets -
	// is not here at all. It is on Impl, in engine/render/<backend>/backend.h,
	// because every one of those calls names a backend type in its signature.
	// The resource factory includes that header deliberately; nothing else does.
	//
	// add_sprite_sheet is the exception the rule produces rather than one made
	// for it: a sheet is engine data, so its signature names nothing a backend
	// owns, so by the sentence above it does not belong on Impl. It is here, and
	// the loader that builds sheets out of JSON - which lives in assets/, where
	// no backend header may go - installs them through it.
	class RenderResources
	{
	public:
		// A sheet is engine data - frame rectangles and strip timings over a
		// texture handle - so unlike Texture and Font it is a real type here.
		using SpriteSheetHandle = Handle<SpriteSheet>;

		RenderResources();
		~RenderResources();

		RenderResources(RenderResources&&) noexcept;
		RenderResources& operator=(RenderResources&&) noexcept;
		RenderResources(const RenderResources&) = delete;
		RenderResources& operator=(const RenderResources&) = delete;

		// Load-time. Adding one twice under the same name replaces it, which is
		// what a device restore does to the texture underneath a sheet.
		void add_sprite_sheet(const std::string& sprite_sheet_name,
			std::unique_ptr<SpriteSheet> sprite_sheet);

		// Load-time. Each throws std::out_of_range naming the resource if nothing
		// has loaded it, so a misspelt name in a definition file fails while the
		// level is loading rather than on the frame that first drew it.
		//
		// A handle outlives a device loss. It names a slot, and reload_device_
		// resources() refills slots rather than making new ones, so a drawable
		// resolved before the loss draws the right thing after it - which a cached
		// texture or font pointer would not, both being remade.
		TextureHandle resolve_texture(const std::string& texture_name) const;
		FontHandle resolve_sprite_font(const std::string& font_name) const;
		SpriteSheetHandle resolve_sprite_sheet(
			const std::string& sprite_sheet_name) const;

		// Per-draw, and const because they are called from render workers. Throws
		// std::out_of_range if the handle is unresolved.
		//
		// A sheet is not a device resource - a device loss takes its texture, and
		// the texture is a handle - so this one never reports "released".
		const SpriteSheet* sprite_sheet(SpriteSheetHandle sprite_sheet) const;

		// The by-name read, for load-time callers that have a name and want the
		// sheet now. Nothing on the draw path may use it.
		const SpriteSheet* sprite_sheet(const std::string& sprite_sheet_name) const;

		// The unscaled extent of `text` in `font`.
		//
		// MEASUREMENT IS EAGER, and it lives here rather than on Renderer
		// because measuring needs the font table and this is the font table.
		// The alternative - a cache inside TextObject filled on first read -
		// is unsound rather than merely slower: text_bounds() is const and
		// every view worker enters it on the same object at the same time, so
		// a mutable cache filled on first read is a data race by construction.
		//
		// So a TextObject measures in its constructor, on one thread, through
		// exactly the pointer it already holds.
		mattmath::Vector2F measure_text(FontHandle font,
			const std::wstring& text) const;

		// Whether `font` has a glyph for every character in `text`, and where
		// the first character it has none for is - wstring_view::npos when
		// there is none.
		//
		// ASKED OF THE ATLAS, NOT OF WHAT WOULD HAPPEN. The loader installs a
		// stand-in glyph on every font it builds, so drawing an unrenderable
		// string no longer fails; it draws question marks. "Will this throw"
		// has therefore stopped being a question worth answering, and "will
		// the player read what I wrote" has not.
		//
		// IT IS HERE BECAUSE THERE IS NO OTHER WAY TO ASK. SpriteFont's own
		// ContainsCharacter is behind the backend header, and the two files
		// outside engine/render/<backend>/ allowed to include that are named
		// in ARCHITECTURE - a third would be a mistake. Without this a client
		// can only guess, or convert every content string through a
		// hand-written table of what it believes the font holds. One did, and
		// only after measuring the alternative: a curly apostrophe in a weapon
		// description let the game start, load and reach the menu, and would
		// have thrown four screens later, during play.
		//
		// Per UTF-16 unit, so a character outside the basic plane is two and
		// reports at the first of them - which is where a caller pointing at
		// the problem wants to point anyway, and is how the font will treat it
		// when it draws.
		bool can_render(FontHandle font, std::wstring_view text) const;
		size_t first_unrenderable(FontHandle font,
			std::wstring_view text) const;

		// The tables, for the resource factory that fills them. Declared in the
		// backend's own header rather than here - see the class comment.
		class Impl;
		Impl* impl() const { return this->impl_.get(); }

	private:
		std::unique_ptr<Impl> impl_;
	};
}
