#include <doctest/doctest.h>

#include "engine/render/d3d11/backend.h"

#include <cstdint>
#include <optional>

using namespace labrador;

TEST_CASE("D3D11 texture limits follow the selected feature level without a device")
{
	for (const D3D_FEATURE_LEVEL level : { D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_10_1 })
	{
		CHECK(texture_limit_for_feature_level(level) ==
			std::optional<std::uint32_t>(8192u));
	}
	for (const D3D_FEATURE_LEVEL level : { D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1 })
	{
		CHECK(texture_limit_for_feature_level(level) ==
			std::optional<std::uint32_t>(16384u));
	}
	for (const D3D_FEATURE_LEVEL level : { D3D_FEATURE_LEVEL_9_1,
		D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3,
		static_cast<D3D_FEATURE_LEVEL>(0), static_cast<D3D_FEATURE_LEVEL>(0xffff) })
	{
		CHECK_FALSE(texture_limit_for_feature_level(level).has_value());
	}
}
