#include <doctest/doctest.h>
#include "tests/render/device_fixture.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

// The GL a test may call directly, which is the context's own header: GLES 3.0
// in a browser and the Windows SDK's 1.1 on the desktop, where it needs
// <Windows.h> first - the same choice gl/gl_functions.h makes for the
// backend.
#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <Windows.h>
#include <gl/GL.h>
#endif

#include <stdexcept>
#include <string>

TEST_CASE("GL texture refusal leaves the caller's binding untouched and permits retry")
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

TEST_CASE("GL refuses a format outside TextureFormat by name, before allocating")
{
	using namespace labrador;
	render_tests::DeviceFixture fixture;

	// Laid out by hand, because texture_level refuses this value itself.
	TextureData texture;
	texture.width = 1;
	texture.height = 1;
	texture.format = static_cast<TextureFormat>(99);
	TextureLevel level;
	level.width = 1;
	level.height = 1;
	level.stride = 4;
	level.rows = 1;
	level.size = 4;
	texture.levels.push_back(level);
	texture.pixels.assign(4, 255);

	GLuint sentinel = 0;
	glGenTextures(1, &sentinel);
	glBindTexture(GL_TEXTURE_2D, sentinel);
	std::string message;
	try
	{
		add_texture_asset(fixture.renderer, fixture.resources, "unknown_format",
			texture);
	}
	catch (const std::runtime_error& error)
	{
		message = error.what();
	}
	CHECK(message.find("unknown_format") != std::string::npos);
	GLint bound = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
	CHECK(static_cast<GLuint>(bound) == sentinel);
	glDeleteTextures(1, &sentinel);
	CHECK_THROWS_AS(fixture.resources.resolve_texture("unknown_format"),
		std::out_of_range);
}
