#pragma once

#include "engine/core/handle.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_sheet.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

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

		// The tables, for the resource factory that fills them. Declared in the
		// backend's own header rather than here - see the class comment.
		class Impl;
		Impl* impl() const { return this->impl_.get(); }

	private:
		std::unique_ptr<Impl> impl_;
	};
}
