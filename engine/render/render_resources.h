#pragma once

#include "engine/core/handle.h"
#include "engine/core/registry.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_sheet.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>
#include <string_view>

namespace labrador
{
	// Everything drawing reads from, cached by name: the textures the GPU holds,
	// the fonts built from them, and the sheets that index them. Named resources
	// in, handles out - the loader that fills it decides what a name means and
	// where the bytes came from.
	//
	// BEHIND THE SEAM, and this is why the pimpl. The public API here was
	// already handles, but the storage was ComPtr<ID3D11ShaderResourceView> and
	// DirectX::SpriteFont, so every translation unit that resolved a name
	// compiled <SpriteFont.h> - which meant a headless test could not construct
	// a TextObject even after draw() was clean, because TextObject's
	// *constructor* resolves a font and measures a string. Renderer holds an
	// Impl the backend defines; so does this, for the same reason and in the
	// same folder.
	//
	// ONE OF THE TWO STORAGE TYPES HAS SINCE STOPPED BEING A BACKEND'S. A Font
	// is engine data over a TextureHandle (font.h), so measuring a string is
	// arithmetic a headless test can now run - the pimpl is still here for the
	// textures, which nothing will ever make portable.
	//
	// THE RULE ABOUT WHICH HALF LIVES WHERE: a call whose signature names a
	// backend type is on Impl, in engine/render/<backend>/backend.h, and the
	// resource factory includes that header deliberately; nothing else does.
	// A call whose signature names only engine types is here.
	//
	// add_texture is on Impl and add_sprite_sheet and add_font are here, which
	// is the rule and not three decisions. A sheet is engine data, a font
	// became engine data, and a texture never will be. add_font moved out of
	// backend.h on the day it stopped taking a DirectX::SpriteFont, without
	// anybody having to decide anything.
	//
	// AND THE STORAGE FOLLOWS THE SAME RULE NOW, which it did not until the
	// three copies of it became legible. The font and sheet tables are members
	// of this class; only the texture table is on Impl. Each backend used to
	// carry all three, so every method below - measure_text and
	// first_unrenderable included, which are the two that carry a contract
	// rather than a forward - was defined once per backend, because Impl was a
	// complete type only inside a backend folder. The three copies agreed, and
	// nothing but proofreading held them to it.
	//
	// The trade-off those files stated was between that duplication and a
	// virtual table on the resource store, which would cost a call per lookup
	// on the frame path (T8). It was a false pair. A table of engine data
	// behind a pimpl is hidden from nobody: the pimpl is there so that a
	// caller resolving a name does not compile a graphics API, and Font and
	// SpriteSheet are not one. Moving them out costs no indirection, and what
	// stays behind is exactly the calls that touch a texture.
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

		// The same, for a font, and it is here beside the sheets rather than on
		// Impl beside the textures for the reason stated above: a Font is
		// engine data now (font.h), so its signature names nothing a backend
		// owns. It arrived here from Impl in the commit that made that true.
		//
		// The atlas underneath it is a texture like any other and goes in
		// through Impl, which is the one part of loading a font that does need
		// a device.
		void add_font(const std::string& font_name,
			std::unique_ptr<Font> font);

		// Drops everything the device owns, leaving the names - and therefore
		// every handle resolved from them - in place, so that a drawable holding
		// one before the loss draws the right thing after the reload refills its
		// slot.
		//
		// TEXTURES, AND THAT IS NOW THE WHOLE LIST. The sheets are not device
		// resources and never were: a sheet's device half is its texture, which
		// is a handle. A font's is too, since it became engine data - so a
		// device loss takes a font's atlas out of the texture table and leaves
		// the glyphs, the spacing and the handle that names the atlas exactly
		// where they were.
		//
		// IT IS ON THE SEAM RATHER THAN ON Impl, unlike the adds beside it,
		// because its signature names nothing a backend owns and its one caller
		// is the shell - which learns from DeviceNotify that a device it has
		// never heard of has gone away. Making the shell say which *kinds* of
		// resource a loss takes would be the seam telling it what a texture is.
		void release_device_resources();

		// CONSTRAINT: A RenderResources OUTLIVES THE Renderer IT WAS FILLED
		// AGAINST. This class holds whatever a backend calls a texture, and on
		// TWO of the five that is a resource the GPU may still be reading - an
		// ID3D12Resource on one and a VkImage on the other - whose only wait is
		// ~Renderer::Impl, so a shell that declares its table before its
		// renderer releases every texture ahead of the one wait on the whole
		// shutdown path. The D3D12 debug layer calls that
		// OBJECT_DELETED_WHILE_STILL_IN_USE and
		// engine/render/d3d12/device_resources.cpp asks it to break on one;
		// the Vulkan validation layers report the same thing about a VkImage,
		// which is the reason to say TWO here rather than to leave the number
		// where the fifth backend found it.
		//
		// It is stated here rather than only kept, because it costs nothing on
		// three of the five backends and is therefore invisible in three
		// configurations out of five. Members destruct in reverse declaration
		// order, so keeping it means declaring the table BEFORE the renderer -
		// which is the opposite of the order they are CREATED in, and the
		// paragraph on set_resources in renderer.h says why that order is
		// forced the other way. Everything in this repository that holds both
		// keeps it: engine/app/application.h, and the three test files whose
		// harnesses stand in for a shell.
		//
		// Nothing here needs the renderer to still be alive: a texture releases
		// itself, and an ID3D12Resource holds its own reference on the device.
		//
		// AND THE FIFTH BACKEND HAD TO SUPPLY THAT SECOND HALF BY HAND, which is
		// the one thing this paragraph did not anticipate. A VkImage is a handle
		// with no reference in it and vkDestroyImage takes the device as an
		// argument, so a texture outliving the renderer would be a call into a
		// device that has gone - with nothing left to raise a diagnostic. Every
		// VulkanTexture therefore holds a shared_ptr to the device it was made
		// on, which is COM's guarantee written out; engine/render/vulkan/
		// device_resources.h carries the argument. The sentence above still
		// holds on every backend - it is now true by construction on five rather
		// than by luck on four.

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

		// The same, for a font, and it is the same because a Font is the same
		// kind of thing a SpriteSheet is: engine data over a texture handle.
		// This used to be RenderResources::Impl::font, one copy per backend
		// behind the wall, and it is on the seam now for the reason the sheet
		// accessor beside it always was.
		//
		// IT DOES NOT MAKE THE THREE BELOW REDUNDANT. Those are what a caller
		// wants - the extent of a string, whether the atlas can spell it - and
		// measure_text is a pinned term of the pixel contract
		// (tests/render/pixel_tests.cpp). This is what the engine's own draw
		// path wants, because a glyph walk needs the Font itself, and a backend
		// asking for one through here rather than through Impl is what lets the
		// font table stop being a backend's.
		const Font* font(FontHandle font) const;

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
		// STILL HERE THOUGH THE ANSWER IS NO LONGER BEHIND THE SEAM. Font is
		// engine data now, so a caller holding one could ask it directly - but
		// a caller does not hold one, it holds a FontHandle, and the table that
		// turns a handle back into a font is this. The paragraph below is the
		// reason the question exists; this is the reason it is asked here.
		//
		// ASKED OF THE ATLAS, NOT OF WHAT WOULD HAPPEN. The loader installs a
		// stand-in glyph on every font it builds, so drawing an unrenderable
		// string no longer fails; it draws question marks. "Will this throw"
		// has therefore stopped being a question worth answering, and "will
		// the player read what I wrote" has not.
		//
		// WHY A CLIENT WANTS TO ASK. Without it a client can only guess, or
		// convert every content string through a hand-written table of what it
		// believes the font holds. One did, and only after measuring the
		// alternative: a curly apostrophe in a weapon description let the game
		// start, load and reach the menu, and would have thrown four screens
		// later, during play. The throw is gone - a missing glyph draws a
		// question mark now - which changed the question from "will this kill
		// the process" to "will the player read what I wrote", and left it
		// worth asking.
		//
		// Per UTF-16 unit, so a character outside the basic plane is two and
		// reports at the first of them - which is where a caller pointing at
		// the problem wants to point anyway, and is how the font will treat it
		// when it draws.
		//
		// A LINE FEED IS NOT AN UNRENDERABLE CHARACTER. U+000A and U+000D lay
		// text out rather than appearing in it, no atlas holds a glyph for
		// either, and the pen walk answers both itself - so they are skipped
		// here too. Asking the atlas about them condemned four strings that
		// draw perfectly well, in the first client that ran this check.
		bool can_render(FontHandle font, std::wstring_view text) const;
		size_t first_unrenderable(FontHandle font,
			std::wstring_view text) const;

		// The texture table, for the resource factory that fills it. Declared in
		// the backend's own header rather than here - see the class comment.
		// It is the texture table and no longer all three.
		class Impl;
		Impl* impl() const { return this->impl_.get(); }

	private:
		// The two tables whose resource type is an engine type, so neither
		// needs hiding and neither may be written three times. A device loss
		// touches neither: a font's device half is its atlas, which is a
		// texture handle, and a sheet's is the same.
		Registry<Font> fonts_{ "Font" };
		Registry<SpriteSheet> sprite_sheets_{ "SpriteSheet" };

		std::unique_ptr<Impl> impl_;
	};
}
