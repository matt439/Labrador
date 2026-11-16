#include <doctest/doctest.h>

#include "engine/render/d3d11/backend.h"
#include "tests/render/device_fixture.h"

#include <cstdint>
#include <optional>
#include <stdexcept>

using namespace labrador;

namespace
{
	class LimitObserver final : public DeviceNotify
	{
	public:
		void on_device_lost() override
		{
			++this->lost_calls;
			this->lost_without_limit = !this->renderer->texture_dimension_limit().has_value();
		}
		void on_device_restored() override
		{
			++this->restored_calls;
			this->restored_limit = this->renderer->texture_dimension_limit();
		}

		// Borrowed while attached; the test fixture owns the renderer.
		Renderer* renderer = nullptr;
		int lost_calls = 0;
		int restored_calls = 0;
		bool lost_without_limit = false;
		std::optional<std::uint32_t> restored_limit;
	};
}

TEST_CASE("D3D11 texture limit follows the backend notification lifecycle")
{
	LimitObserver observer;
	render_tests::DeviceFixture fixture;
	observer.renderer = &fixture.renderer;
	const std::optional<std::uint32_t> initial = fixture.renderer.texture_dimension_limit();
	REQUIRE(initial.has_value());
	CHECK(initial == texture_limit_for_feature_level(
		fixture.renderer.impl()->device_resources.GetD3DDevice()->GetFeatureLevel()));
	CHECK_THROWS_AS(fixture.renderer.create_device(fixture.window.handle(), 64, 64, 0),
		std::invalid_argument);
	CHECK(fixture.renderer.texture_dimension_limit() == initial);

	fixture.renderer.set_device_notify(&observer);
	// Exercise the backend callbacks on an idle device, without removing it.
	// This checks notification ordering, not physical device-loss recovery.
	fixture.renderer.impl()->OnDeviceLost();
	CHECK(observer.lost_calls == 1);
	CHECK(observer.lost_without_limit);
	CHECK_FALSE(fixture.renderer.texture_dimension_limit().has_value());
	fixture.renderer.impl()->OnDeviceRestored();
	CHECK(observer.restored_calls == 1);
	CHECK(observer.restored_limit == initial);
	CHECK(fixture.renderer.texture_dimension_limit() == initial);
	fixture.renderer.set_device_notify(nullptr);
}
