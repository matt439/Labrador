#pragma once

#include "engine/math/rectanglei.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"

#include <memory>
#include <utility>
#include <vector>

namespace components
{
	// Everything a component needs to be built, with no window and no device.
	//
	// A font is engine data (render_resources.h), so it can be made from
	// glyphs with an empty atlas handle: a component measures its text and
	// never draws here. Every printable ASCII character is a ten-unit cell on
	// a twenty-unit line, so a measurement is exact, the way
	// tests/render/font_tests.cpp builds its fonts.
	class TestContent
	{
	public:
		TestContent()
		{
			std::vector<labrador::Glyph> glyphs;

			for (char32_t character = U' '; character <= U'~'; ++character)
			{
				labrador::Glyph glyph;
				glyph.character = character;
				glyph.subrect = mattmath::RectangleI(0, 0, 10, 10);
				glyphs.push_back(glyph);
			}

			this->resources.add_font("font",
				std::make_unique<labrador::Font>(labrador::TextureHandle(),
					std::move(glyphs), 20.0f));
		}

		labrador::RenderResources resources;
	};
}
