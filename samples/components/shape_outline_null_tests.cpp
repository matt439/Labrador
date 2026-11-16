#include <doctest/doctest.h>

#include "samples/components/shape_outline.h"

#include "engine/math/circle.h"
#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/triangle.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/viewport.h"

#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace labrador;
using components::ShapeOutline;

namespace
{
	class Drawing
	{
	public:
		Drawing()
		{
			this->renderer.create_device(nullptr, 640, 360, 2);
			this->renderer.set_resources(&this->resources);
			TextureData data;
			data.width = data.height = 1;
			data.levels.push_back(texture_level(data.format, 1, 1, 0));
			data.pixels.assign(4, 255);
			add_texture_asset(this->renderer, this->resources, "white", data);
			this->white = this->resources.resolve_texture("white");
		}

		std::vector<RecordedSprite> draw(const Shape& shape, int segments = 16)
		{
			this->renderer.begin_frame();
			this->renderer.set_view_count(1);
			DrawList list = this->renderer.view(0);
			list.set_viewport(Viewport(0, 0, 640, 360));
			const ShapeOutline outline(this->white);
			outline.draw(list, shape, 2.0f, Colour::yellow, segments);
			this->renderer.submit();
			return recorded_sprites(this->renderer);
		}

		RenderResources resources;
		Renderer renderer;
		TextureHandle white;
	};
}

TEST_CASE("shape outlines submit white-texture quads for every supported boundary")
{
	Drawing drawing;
	const RectangleF rectangle(10, 20, 40, 30);
	const Triangle triangle(10, 20, 50, 20, 30, 60);
	const Quad quad(RectangleF(10, 20, 40, 30));
	const RectangleRotated rotated(Vector2F(100, 100), Vector2F(0, 1),
		Vector2F(-1, 0), Vector2F(30, 20));
	const Circle circle(100, 100, 20);
	const std::array<const Shape*, 5> shapes = { &rectangle, &triangle, &quad, &rotated, &circle };
	const std::array<std::size_t, 5> counts = { 4, 3, 4, 4, 8 };
	for (std::size_t index = 0; index < shapes.size(); ++index)
	{
		const std::vector<RecordedSprite> sprites = drawing.draw(*shapes[index], 8);
		REQUIRE(sprites.size() == counts[index]);
		for (const RecordedSprite& sprite : sprites)
		{
			CHECK(sprite.texture == drawing.white);
			for (const SpriteVertex& corner : sprite.corners)
			{
				CHECK(corner.colour == Colour::yellow);
			}
			CHECK(sprite.corners[0].texcoord == Vector2F(0, 0));
			CHECK(sprite.corners[3].texcoord == Vector2F(1, 1));
		}
	}
	const std::vector<RecordedSprite> rectangle_draw = drawing.draw(rectangle);
	CHECK(rectangle_draw[0].corners[0].position == Vector2F(10, 19));
	CHECK(rectangle_draw[0].corners[3].position == Vector2F(50, 21));
}

TEST_CASE("const outline draws repeat in independent views without changing the shape")
{
	Drawing drawing;
	const Circle circle(100, 100, 20);
	const ShapeOutline outline(drawing.white);
	drawing.renderer.begin_frame();
	drawing.renderer.set_view_count(2);
	for (int view = 0; view < 2; ++view)
	{
		DrawList list = drawing.renderer.view(view);
		list.set_viewport(Viewport(0, 0, 640, 360));
		outline.draw(list, circle, 2.0f, Colour::yellow, 8);
	}
	drawing.renderer.submit();
	const std::vector<RecordedSprite>& sprites = recorded_sprites(drawing.renderer);
	REQUIRE(sprites.size() == 16);
	for (std::size_t index = 0; index < 8; ++index)
	{
		CHECK(sprites[index].view == 0);
		CHECK(sprites[index + 8].view == 1);
		for (std::size_t corner = 0; corner < 4; ++corner)
		{
			CHECK(sprites[index].corners[corner].position == sprites[index + 8].corners[corner].position);
		}
	}
	CHECK(circle.center() == Vector2F(100, 100));
	CHECK(circle.radius() == 20);
}

TEST_CASE("zero edges are omitted and invalid derived strokes submit nothing")
{
	Drawing drawing;
	CHECK(drawing.draw(RectangleF()).empty());
	CHECK(drawing.draw(Quad()).empty());
	CHECK(drawing.draw(RectangleRotated()).empty());
	CHECK(drawing.draw(Circle(20, 30, 0)).empty());
	CHECK(drawing.draw(Triangle(0, 0, 10, 0, 20, 0)).size() == 3);
	drawing.renderer.begin_frame();
	drawing.renderer.set_view_count(1);
	DrawList list = drawing.renderer.view(0);
	list.set_viewport(Viewport(0, 0, 640, 360));
	const ShapeOutline outline(drawing.white);
	const RectangleF rectangle(10, 20, 30, 40);
	for (const float thickness : { 0.0f, -1.0f, std::numeric_limits<float>::infinity() })
	{
		CHECK_THROWS_AS(outline.draw(list, rectangle, thickness, Colour::yellow, 8), std::invalid_argument);
	}
	volatile float runtime_largest = std::numeric_limits<float>::max();
	const float largest = runtime_largest;
	CHECK_THROWS_AS(outline.draw(list, Triangle(0, 0, 100, 0, largest, 0),
		2.0f, Colour::yellow, 8), std::invalid_argument);
	drawing.renderer.submit();
	CHECK(recorded_sprites(drawing.renderer).empty());
}
