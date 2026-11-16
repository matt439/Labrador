#include <doctest/doctest.h>

#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "tests/render/device_fixture.h"
#include "tests/render/test_textures.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

TEST_CASE("selected device reports its texture bound and refuses oversized replacement")
{
	using namespace labrador;
	render_tests::DeviceFixture fixture;
	const std::optional<std::uint32_t> limit =
		fixture.renderer.texture_dimension_limit();
	REQUIRE(limit.has_value());
	REQUIRE(*limit > 0);
	const RenderDeviceInfo& info = fixture.renderer.device_info();
	std::printf("Selected 2D texture dimension limit: %u texels; backend=%s, "
		"device=%s, kind=%s.\n", static_cast<unsigned>(*limit),
		info.backend.c_str(), info.device_name.c_str(),
		info.kind == RenderDeviceKind::software ? "software" : "hardware");

	add_texture_asset(fixture.renderer, fixture.resources, "retained",
		render_tests::flat_texture(255, 255, 255));
	const TextureHandle retained = fixture.resources.resolve_texture("retained");
	const Vector2I original = fixture.resources.texture_size(retained);
	if (*limit >= static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
	{
		// TextureData dimensions are int: no representable positive request
		// can exceed this device's bound. Do not narrow it to invent one.
		return;
	}
	for (const bool wide : { true, false })
	{
		TextureData oversized;
		oversized.width = wide ? static_cast<int>(*limit + 1u) : 1;
		oversized.height = wide ? 1 : static_cast<int>(*limit + 1u);
		// Metadata only. The bound must be checked before an allocation or
		// access to image bytes, so even an enormous reported limit is cheap.
		for (const TextureFormat format : { TextureFormat::r8g8b8a8_unorm,
			TextureFormat::bc3_unorm })
		{
			oversized.format = format;
			for (const std::string name : { "new-oversized", "retained" })
			{
				std::string message;
				try
				{
					add_texture_asset(fixture.renderer, fixture.resources, name, oversized);
					FAIL("oversized request reached an upload");
				}
				catch (const std::runtime_error& error)
				{
					message = error.what();
				}
				CHECK(message.find(name) != std::string::npos);
				CHECK(message.find(std::to_string(oversized.width) + "x" +
					std::to_string(oversized.height)) != std::string::npos);
				CHECK(message.find("2D texture dimension limit of " +
					std::to_string(*limit)) != std::string::npos);
			}
		}
		CHECK_THROWS_AS(fixture.resources.resolve_texture("new-oversized"),
			std::out_of_range);
		CHECK(fixture.resources.resolve_texture("retained") == retained);
		CHECK(fixture.resources.texture_size(retained) == original);
	}
}
