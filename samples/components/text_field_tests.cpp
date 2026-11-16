#include <doctest/doctest.h>

#include "samples/components/text_field.h"

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"
#include "engine/render/text_encoding.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using components::TextEdit;
using components::TextField;
using components::TextFieldLimits;
using namespace labrador;

namespace
{
	class FieldContent
	{
	public:
		explicit FieldContent(bool caret = true,
			RectangleI caret_cell = RectangleI(0, 0, 10, 10))
		{
			std::vector<Glyph> glyphs;
			for (char32_t character = U' '; character <= U'~'; ++character)
			{
				if (character == U'|' && !caret)
				{
					continue;
				}
				Glyph glyph;
				glyph.character = character;
				glyph.subrect = RectangleI(0, 0, 10, 10);
				if (character == U' ')
				{
					glyph.subrect = RectangleI(0, 0, 1, 1);
					glyph.x_advance = 9.0f;
				}
				if (character == U'|')
				{
					glyph.subrect = caret_cell;
				}
				glyphs.push_back(glyph);
			}
			for (char32_t character : { U'\u00E9', U'\u2019' })
			{
				Glyph glyph;
				glyph.character = character;
				glyph.subrect = RectangleI(0, 0, 10, 10);
				glyphs.push_back(glyph);
			}
			std::unique_ptr<Font> font = std::make_unique<Font>(
				TextureHandle(), std::move(glyphs), 20.0f);
			font->set_stand_in(U'?');
			this->resources.add_font("font", std::move(font));
		}

		RenderResources resources;
	};
}

TEST_CASE("TextField appends UTF-8 batches and backspaces complete two/three-byte code points")
{
	FieldContent content;
	TextField field("name", L"Name", "", { 3, 40.0f },
		Vector2F::zero, 100.0f, "font", &content.resources);
	CHECK(field.append("ignored") == TextEdit::inactive);
	CHECK_FALSE(field.backspace());
	REQUIRE(field.begin_edit());
	CHECK(field.display().text() == L"|");
	CHECK(field.append("A\xC3\xA9\xE2\x80\x99") == TextEdit::accepted);
	CHECK(field.draft() == "A\xC3\xA9\xE2\x80\x99");
	CHECK(field.value().empty());
	CHECK(field.code_points() == 3);
	CHECK(field.display().text() == L"A\u00E9\u2019|");
	CHECK(field.append("B") == TextEdit::length_limit);
	CHECK(field.backspace());
	CHECK(field.draft() == "A\xC3\xA9");
	CHECK(field.code_points() == 2);
	CHECK(field.backspace());
	CHECK(field.draft() == "A");
	CHECK(field.backspace());
	CHECK(field.draft().empty());
	CHECK_FALSE(field.backspace());
	CHECK(field.display().text() == L"|");
}

TEST_CASE("TextField confirms drafts, cancels to the prior value and ignores inactive commands")
{
	FieldContent content;
	TextField field("name", L"Name", "Old", {}, Vector2F::zero,
		100.0f, "font", &content.resources);
	CHECK_FALSE(field.confirm());
	CHECK_FALSE(field.cancel());
	CHECK(field.begin_edit());
	CHECK(field.append(" value") == TextEdit::accepted);
	CHECK_FALSE(field.begin_edit());
	CHECK(field.draft() == "Old value");
	CHECK(field.cancel());
	CHECK_FALSE(field.editing());
	CHECK(field.value() == "Old");
	CHECK(field.draft() == "Old");
	CHECK(field.display().text() == L"Old");
	CHECK(field.append(std::string_view("\xFF")) == TextEdit::inactive);
	CHECK(field.begin_edit());
	CHECK(field.append("!") == TextEdit::accepted);
	CHECK(field.confirm());
	CHECK(field.value() == "Old!");
	CHECK_FALSE(field.editing());
	CHECK(field.display().text() == L"Old!");
	CHECK(field.begin_edit());
	CHECK_FALSE(field.confirm());
	CHECK_FALSE(field.editing());
	CHECK(field.value() == "Old!");
}

TEST_CASE("TextField refuses a whole invalid or unrenderable batch without substitution or partial edits")
{
	FieldContent content;
	TextField field("name", L"Name", "A", {}, Vector2F::zero,
		100.0f, "font", &content.resources);
	field.begin_edit();
	const std::wstring before = field.display().text();
	CHECK(field.append("B\xFF") == TextEdit::invalid_utf8);
	CHECK(field.append("\xC0\xAF") == TextEdit::invalid_utf8);
	CHECK(field.append("B\n") == TextEdit::unsupported_character);
	CHECK(field.append("\t") == TextEdit::unsupported_character);
	CHECK(field.append("\xF0\x9F\x98\x80") == TextEdit::unsupported_character);
	CHECK(field.append("B\xCE\xBB") == TextEdit::missing_glyph);
	CHECK(field.draft() == "A");
	CHECK(field.value() == "A");
	CHECK(field.display().text() == before);
	CHECK(field.append("") == TextEdit::unchanged);
}

TEST_CASE("TextField reserves its drawn end caret and trailing spaces inside the width limit")
{
	FieldContent content;
	TextField field("name", L"Name", "A", { 10, 30.0f },
		Vector2F::zero, 100.0f, "font", &content.resources);
	field.begin_edit();
	CHECK(field.append(" ") == TextEdit::accepted);
	CHECK(field.display().text() == L"A |");
	CHECK(field.display().bounds().width == 30.0f);
	CHECK(field.append(" ") == TextEdit::width_limit);
	CHECK(field.draft() == "A ");
	CHECK(field.confirm());
	CHECK(field.value() == "A ");
	CHECK(field.begin_edit());
	CHECK(field.display().text() == L"A |");
}

TEST_CASE("TextField validates limits, initial contents and a real caret glyph before use")
{
	FieldContent content;
	CHECK_THROWS_AS(TextField("name", L"Name", "", { 0, 100.0f },
		Vector2F::zero, 100.0f, "font", &content.resources), std::invalid_argument);
	CHECK_THROWS_AS(TextField("name", L"Name", "", { 12, 0.0f },
		Vector2F::zero, 100.0f, "font", &content.resources), std::invalid_argument);
	CHECK_THROWS_AS(TextField("name", L"Name", "", { 12,
		std::numeric_limits<float>::quiet_NaN() }, Vector2F::zero,
		100.0f, "font", &content.resources), std::invalid_argument);
	CHECK_THROWS_AS(TextField("name", L"Name", "", { 12, 9.0f },
		Vector2F::zero, 100.0f, "font", &content.resources), std::invalid_argument);
	CHECK_THROWS_AS(TextField("name", L"Name", "Bad\n", {},
		Vector2F::zero, 100.0f, "font", &content.resources), std::invalid_argument);
	CHECK_THROWS_AS(TextField("name", L"Name", "Bad\xFF", {},
		Vector2F::zero, 100.0f, "font", &content.resources), std::invalid_argument);
	FieldContent without_caret(false);
	CHECK_THROWS_AS(TextField("name", L"Name", "", {}, Vector2F::zero,
		100.0f, "font", &without_caret.resources), std::invalid_argument);
	FieldContent invisible_caret(true, RectangleI(0, 0, 0, 10));
	CHECK_THROWS_AS(TextField("name", L"Name", "", {}, Vector2F::zero,
		100.0f, "font", &invisible_caret.resources), std::invalid_argument);
	FieldContent pixel_caret(true, RectangleI(0, 0, 1, 1));
	CHECK_NOTHROW(TextField("name", L"Name", "", {}, Vector2F::zero,
		100.0f, "font", &pixel_caret.resources));
}
