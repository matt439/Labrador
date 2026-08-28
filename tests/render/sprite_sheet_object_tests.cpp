#include <doctest/doctest.h>

#include "engine/core/name_table.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/animation_strip.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/sprite_sheet_object.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// The handle a drawable keeps instead of a name.
//
// WHY THIS FILE EXISTS. SpriteSheetObject was one of the twelve .cpp under
// engine/render/ that no test named. It is thirty-four lines and it exists for
// one reason, which its header states: the name is resolved once here rather
// than descended per draw, per drawable, from every render worker at once
// (PHILOSOPHY T7, T8). A test that only checked the accessor would miss the
// whole claim.
//
// So what is stated here is the consequence of holding a handle rather than a
// name, and it cuts both ways. A sheet replaced under the same name IS seen
// through a handle already held - which is what a device restore needs and is
// the last case below. And a handle resolved against one sheet means nothing
// against another, which is why set_sprite_sheet is protected and why
// AnimationObject moves both together.

namespace
{
	using labrador::AnimationStrip;
	using labrador::DrawObject;
	using labrador::NameTable;
	using labrador::RenderResources;
	using labrador::SpriteFrame;
	using labrador::SpriteSheet;
	using labrador::SpriteSheetObject;
	using labrador::TextureHandle;
	using mattmath::RectangleI;

	// THE STATE IS protected AND THAT IS THE CLASS'S SHAPE - the same argument
	// draw_object_tests.cpp makes next door. A SpriteSheetObject is a base for
	// TextureObject and AnimationObject, so a derived probe is how its caller
	// sees it.
	class Probe : public SpriteSheetObject
	{
	public:
		using SpriteSheetObject::SpriteSheetObject;

		using DrawObject::colour;
		using DrawObject::draw_rotation;
		using DrawObject::flip;
		using DrawObject::layer_depth;
		using DrawObject::origin;
		using DrawObject::render_resources;
		using SpriteSheetObject::set_sprite_sheet;
		using SpriteSheetObject::sprite_sheet;
	};

	// A sheet with no frames and no strips, told apart from its neighbours by
	// the texture handle it carries. Nothing here draws, so the handle is a
	// label rather than a resource.
	std::unique_ptr<SpriteSheet> sheet_marked(int texture_index)
	{
		return std::make_unique<SpriteSheet>(TextureHandle(texture_index),
			NameTable<SpriteFrame>("sprite frame"),
			NameTable<AnimationStrip>("animation strip"));
	}

	class Content
	{
	public:
		Content() : resources()
		{
			this->resources.add_sprite_sheet("first", sheet_marked(1));
			this->resources.add_sprite_sheet("second", sheet_marked(2));
		}

		RenderResources resources;
	};
}

TEST_CASE("a default sprite sheet object holds no table and no sheet")
{
	const Probe object;

	// Nothing is resolved and nothing is pointed at, which is what lets every
	// drawable be default-constructible. Asking it for its sheet would go
	// through a null table, so the statement is about the table.
	CHECK(object.render_resources() == nullptr);
}

TEST_CASE("the sheet name is resolved once, in the constructor")
{
	Content content;
	const Probe object("first", &content.resources);

	CHECK(object.render_resources() == &content.resources);
	CHECK(object.sprite_sheet() == content.resources.sprite_sheet("first"));
	CHECK(object.sprite_sheet()->texture() == TextureHandle(1));
}

TEST_CASE("a sheet name the table does not have is refused at construction")
{
	Content content;

	// At construction rather than at the first draw, which is the point of
	// resolving once: a content bug says so while the level is loading.
	CHECK_THROWS_AS(Probe("no_such_sheet", &content.resources),
		std::out_of_range);
}

TEST_CASE("the drawable's own state is untouched by the sheet it points at")
{
	Content content;
	const Probe object("first", &content.resources, labrador::Colour::red,
		1.5f, mattmath::Vector2F(4.0f, 8.0f), labrador::SpriteFlip::horizontal,
		0.25f);

	// Everything past the table is DrawObject's and passes straight through.
	// Stated because the constructor takes seven parameters and forwards five
	// of them, which is exactly the shape a reordering breaks silently.
	CHECK(object.colour() == labrador::Colour::red);
	CHECK(object.draw_rotation() == 1.5f);
	CHECK(object.origin() == mattmath::Vector2F(4.0f, 8.0f));
	CHECK(object.flip() == labrador::SpriteFlip::horizontal);
	CHECK(object.layer_depth() == 0.25f);
}

TEST_CASE("re-pointing at another sheet moves the handle, not the table")
{
	Content content;
	Probe object("first", &content.resources);

	object.set_sprite_sheet("second");

	CHECK(object.sprite_sheet()->texture() == TextureHandle(2));
	CHECK(object.render_resources() == &content.resources);

	// And the same refusal as the constructor's, for the same reason.
	CHECK_THROWS_AS(object.set_sprite_sheet("no_such_sheet"),
		std::out_of_range);
}

TEST_CASE("a sheet replaced under its own name is seen through a held handle")
{
	Content content;
	const Probe object("first", &content.resources);

	CHECK(object.sprite_sheet()->texture() == TextureHandle(1));

	// THIS IS WHAT THE HANDLE BUYS, and it is the reason add_sprite_sheet
	// replaces rather than rejects: a device loss empties the texture under a
	// sheet and the reload puts a new sheet in the same slot, so every
	// drawable that resolved the name before the loss draws the right thing
	// after it - without being told, and without holding the name to look it
	// up again.
	content.resources.add_sprite_sheet("first", sheet_marked(3));

	CHECK(object.sprite_sheet()->texture() == TextureHandle(3));
}
