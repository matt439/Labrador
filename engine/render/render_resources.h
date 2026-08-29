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
	// BEHIND THE SEAM, and this is why the pimpl: the storage of a texture is
	// whatever the backend calls one, and a translation unit that resolved a
	// name would otherwise compile a graphics API to do it. A headless test
	// could not construct a TextObject even with draw() clean, because that
	// constructor resolves a font and measures a string.
	//
	// THE RULE ABOUT WHICH HALF LIVES WHERE, and it is one rule rather than a
	// decision per method: a call whose signature names a backend type is on
	// Impl, in engine/render/<backend>/backend.h, and the resource factory
	// includes that header deliberately; nothing else does. A call whose
	// signature names only engine types is here.
	//
	// The storage follows the same rule. The font and sheet tables are members
	// of this class and only the texture table is on Impl, because a Font and a
	// SpriteSheet are engine data (font.h, sprite_sheet.h) and a texture is not.
	// That is why the methods below are compiled once rather than once per
	// backend, and it costs no indirection: a pimpl is there so a caller
	// resolving a name does not compile a graphics API, and a table of engine
	// data behind one is hidden from nobody. SEAM.md#4 carries the argument.
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
		// Impl beside the textures for the reason stated above: a Font is engine
		// data (font.h), so its signature names nothing a backend owns.
		//
		// The atlas underneath it is a texture like any other and goes in through
		// Impl, which is the one part of loading a font that does need a device.
		void add_font(const std::string& font_name,
			std::unique_ptr<Font> font);

		// Drops everything the device owns, leaving the names - and therefore
		// every handle resolved from them - in place, so that a drawable holding
		// one before the loss draws the right thing after the reload refills its
		// slot.
		//
		// TEXTURES, AND THAT IS THE WHOLE LIST. The sheets are not device
		// resources: a sheet's device half is its texture, which is a handle. A
		// font's is too - so a device loss takes a font's atlas out of the texture
		// table and leaves the glyphs, the spacing and the handle that names the
		// atlas exactly where they were.
		//
		// IT IS ON THE SEAM RATHER THAN ON Impl, unlike the adds beside it,
		// because its signature names nothing a backend owns and its one caller is
		// the shell - which learns from DeviceNotify that a device it has never
		// heard of has gone away. Making the shell say which *kinds* of resource a
		// loss takes would be the seam telling it what a texture is.
		//
		// PRECONDITION: IT IS THE FIRST HALF OF A DEVICE LOSS, NOT A WAY TO DROP
		// TEXTURES. What may follow it is a device being remade and the names
		// being reloaded into it, which is what DeviceNotify guarantees and what
		// Application::on_device_lost/on_device_restored do. Calling it on a live
		// device and reloading without one costs a descriptor-heap slot per name
		// on the D3D12 backend, out of a fixed 256: the slot allocator there is a
		// bump counter with no free list, reset by device creation and by nothing
		// else, because a slot the GPU may still be reading cannot be handed out
		// again and a device loss is the one moment nothing is reading. THE
		// SENTENCE IS THE FIX AND THE FREE LIST IS NOT (T1, T3): a free list would
		// buy back 256 slots for a call nothing in this repository makes, and
		// would have to be told when the GPU had finished with each one - which is
		// the frames-in-flight bookkeeping d3d12/backend.h keeps below the seam,
		// arriving at the seam. If a client ever needs to drop content on a live
		// device, that is a different entry point and it should be asked for by
		// name.
		void release_device_resources();

		// CONSTRAINT: A RenderResources OUTLIVES THE Renderer IT WAS FILLED
		// AGAINST. This class holds whatever a backend calls a texture, and on TWO
		// of the five that is a resource the GPU may still be reading - an
		// ID3D12Resource on one and a VkImage on the other - whose only wait is
		// ~Renderer::Impl, so a shell that declares its table before its renderer
		// releases every texture ahead of the one wait on the whole shutdown path.
		// The D3D12 debug layer calls that OBJECT_DELETED_WHILE_STILL_IN_USE and
		// engine/render/d3d12/device_resources.cpp asks it to break on one; the
		// Vulkan validation layers report the same thing about a VkImage.
		//
		// It is stated here rather than only kept, because it costs nothing on
		// three of the five backends and is therefore invisible in three
		// configurations out of five. Members destruct in reverse declaration
		// order, so keeping it means declaring the table BEFORE the renderer -
		// which is the opposite of the order they are CREATED in, and the
		// paragraph on set_resources in renderer.h says why that order is forced
		// the other way. Everything in this repository that holds both keeps it:
		// engine/app/application.h, and the four test files whose harnesses stand
		// in for a shell - pixel_tests.cpp, null_tests.cpp, renderer_seam_tests.cpp
		// and tests/scene/fanout_tests.cpp, each of which cites this paragraph
		// where it declares the pair.
		//
		// Nothing here needs the renderer to still be alive, and on the Vulkan
		// backend that is true by construction rather than by luck: a VkImage is a
		// handle with no reference in it and vkDestroyImage takes the device as an
		// argument, so every VulkanTexture holds a shared_ptr to the device it was
		// made on - COM's guarantee written out. engine/render/vulkan/
		// device_resources.h carries that argument.

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

		// The same, for a font, and it is the same because a Font is the same kind
		// of thing a SpriteSheet is: engine data over a texture handle.
		//
		// IT DOES NOT MAKE THE THREE BELOW REDUNDANT. Those are what a caller
		// wants - the extent of a string, whether the atlas can spell it - and
		// measure_text is a pinned term of the pixel contract
		// (tests/render/pixel_tests.cpp). This is what the engine's own draw path
		// wants, because a glyph walk needs the Font itself.
		const Font* font(FontHandle font) const;

		// The unscaled extent of `text` in `font`.
		//
		// MEASUREMENT IS EAGER, and it lives here rather than on Renderer because
		// measuring needs the font table and this is the font table. The
		// alternative - a cache inside TextObject filled on first read - is
		// unsound rather than merely slower: text_bounds() is const and every view
		// worker enters it on the same object at the same time, so a mutable cache
		// filled on first read is a data race by construction.
		//
		// So a TextObject measures in its constructor, on one thread, through
		// exactly the pointer it already holds.
		mattmath::Vector2F measure_text(FontHandle font,
			const std::wstring& text) const;

		// Whether `font` has a glyph for every character in `text`, and where the
		// first character it has none for is - wstring_view::npos when there is
		// none.
		//
		// ASKED OF THE ATLAS, NOT OF WHAT WOULD HAPPEN. The loader installs a
		// stand-in glyph on every font that offers a candidate - the font's own
		// default character, then '?', then ' ' - which is every font in this
		// tree, so drawing an unrenderable string does not fail; it draws question
		// marks. A font carrying none of the three gets no stand-in and drops the
		// glyph. So the question this answers is "will the player read what I
		// wrote", not "will this throw".
		//
		// WHY A CLIENT WANTS TO ASK. Without it a client can only guess, or convert
		// every content string through a hand-written table of what it believes the
		// font holds. One did, and only after measuring the alternative: a curly
		// apostrophe in a weapon description let the game start, load and reach the
		// menu, and would have failed four screens later, during play.
		//
		// Per UTF-16 unit, so a character outside the basic plane is two and
		// reports at the first of them - which is where a caller pointing at the
		// problem wants to point anyway, and is how the font will treat it when it
		// draws.
		//
		// A LINE FEED IS NOT AN UNRENDERABLE CHARACTER. U+000A and U+000D lay text
		// out rather than appearing in it, no atlas holds a glyph for either, and
		// the pen walk answers both itself - so they are skipped here too. Asking
		// the atlas about them condemns strings that draw perfectly well.
		bool can_render(FontHandle font, std::wstring_view text) const;
		size_t first_unrenderable(FontHandle font,
			std::wstring_view text) const;

		// The texture table, for the resource factory that fills it. Declared in
		// the backend's own header rather than here - see the class comment.
		class Impl;
		Impl* impl() const { return this->impl_.get(); }

	private:
		// The two tables whose resource type is an engine type, so neither needs
		// hiding. A device loss touches neither: a font's device half is its
		// atlas, which is a texture handle, and a sheet's is the same.
		Registry<Font> fonts_{ "Font" };
		Registry<SpriteSheet> sprite_sheets_{ "SpriteSheet" };

		std::unique_ptr<Impl> impl_;
	};
}
