#include "tests/render/device_fixture.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#include <doctest/doctest.h>
#include <gl/GL.h>

TEST_CASE("GL texture refusal leaves no allocated object and permits retry")
{
	using namespace labrador;
	render_tests::DeviceFixture fixture;
	TextureData texture;
	texture.width = 1;
	texture.height = 1;
	texture.format = TextureFormat::b4g4r4a4_unorm;
	texture.levels.push_back(texture_level(texture.format, 1, 1, 0));
	texture.pixels.assign(2, 255);
	for (int attempt = 0; attempt < 3; ++attempt)
	{
		// A successful upload would alter this binding; a format refusal must
		// precede allocation and binding, preserving the caller's object.
		GLuint sentinel = 0;
		glGenTextures(1, &sentinel);
		glBindTexture(GL_TEXTURE_2D, sentinel);
		CHECK_THROWS_AS(add_texture_asset(fixture.renderer, fixture.resources,
			"unsupported", texture), std::runtime_error);
		GLint bound = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
		CHECK(static_cast<GLuint>(bound) == sentinel);
		glDeleteTextures(1, &sentinel);
	}
	texture.format = TextureFormat::r8g8b8a8_unorm;
	texture.levels[0] = texture_level(texture.format, 1, 1, 0);
	texture.pixels.assign(4, 255);
	CHECK_NOTHROW(add_texture_asset(fixture.renderer, fixture.resources, "unsupported", texture));
	CHECK_NOTHROW(fixture.resources.resolve_texture("unsupported"));
}
