#include "engine/render/resource_factory.h"

#include "engine/render/dds_file.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_font_file.h"
#include "engine/render/texture_data.h"

#include <memory>
#include <string>
#include <utility>

namespace artattack
{
	namespace
	{
		// The stand-in glyph a font draws when asked for one it does not have.
		//
		// WITHOUT ONE, A MISSING GLYPH IS AN EXCEPTION. Font::drawn throws when
		// there is no stand-in, and U+0000 - "nobody chose" - is what every
		// .spritefont in this tree carries, because it is what MakeSpriteFont
		// writes by default, along with exactly 95 glyphs, U+0020 to U+007E.
		// Nothing chose that region and nothing records it, so the
		// out-of-the-box behaviour of the engine's own font kind was: any
		// character a text editor inserts on its own - a curly apostrophe, an
		// en-dash - kills the process on the frame that first drew it, which is
		// typically several screens from wherever the string was read.
		//
		// The engine already documented the opposite. text_encoding.h promises
		// that invalid UTF-8 becomes U+FFFD so that "text about to be drawn
		// should show mojibake, not vanish" - and U+FFFD is outside 32..126, so
		// the documented graceful path led directly into the throw it was
		// written to avoid. It leads to a question mark now.
		//
		// Degradation rather than a throw is also what the audio half of the
		// loader already does: a missing optional wave bank becomes a bank that
		// plays nothing rather than a dead process.
		//
		// '?' FIRST, ' ' AFTER IT, and nothing if the font has neither. A
		// question mark says "something was here and could not be drawn", which
		// is the mojibake text_encoding.h asks for; a space says only that the
		// text is oddly spaced, and is the better of the two only when there is
		// no question mark to be had. A font with neither is not a text font -
		// an icon atlas, say - and is left exactly as it was rather than warned
		// about, because there is nothing wrong with it.
		//
		// THE FILE'S OWN CHOICE IS HONOURED FIRST, which it never was before: a
		// .spritefont says which character it would like, and SpriteFont read
		// that and then this overwrote it. It is U+0000 in every font here, so
		// nothing in this tree changes; a font built by somebody who did choose
		// now gets what they chose.
		void install_stand_in(Font& font, char32_t chosen)
		{
			if (chosen != 0 && font.contains(chosen))
			{
				font.set_stand_in(chosen);
				return;
			}
			if (font.contains(U'?'))
			{
				font.set_stand_in(U'?');
				return;
			}
			if (font.contains(U' '))
			{
				font.set_stand_in(U' ');
			}
		}
	}

	void load_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name)
	{
		add_texture_asset(renderer, resources, name,
			read_dds_file(directory + name + ".dds"));
	}

	void load_font_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name)
	{
		const SpriteFontFile file =
			read_sprite_font_file(directory + name + ".spritefont");

		// THE ATLAS GOES IN THE TEXTURE TABLE, UNDER A NAME NO MANIFEST CAN
		// PRODUCE. It has to be in there rather than held by the Font: the
		// table is what a device loss empties and a reload refills, and a
		// texture the table has never heard of would survive a loss as a
		// dangling pointer inside a font that nothing thinks is a device
		// resource. The colon is what makes the name safe - it cannot appear in
		// a Windows filename, and every other name in this table is one.
		const std::string atlas_name = "font:" + name;
		add_texture_asset(renderer, resources, atlas_name, file.atlas);

		std::unique_ptr<Font> font = std::make_unique<Font>(
			resources.resolve_texture(atlas_name), file.glyphs,
			file.line_spacing);
		install_stand_in(*font, file.stand_in);

		resources.add_font(name, std::move(font));
	}
}
