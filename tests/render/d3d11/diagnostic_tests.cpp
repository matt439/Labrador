#include <doctest/doctest.h>

#include "engine/render/renderer.h"
#include "tests/diagnostic_capture.h"
#include "tests/render/test_window.h"

#include <string_view>

TEST_CASE("Direct3D 11 Debug device notices reach the diagnostic sink")
{
	labrador_tests::DiagnosticCapture capture;
	{
		render_tests::TestWindow window(64, 64);
		labrador::Renderer renderer;
		renderer.create_device(window.handle(), 64, 64, 1);
		// A selected hardware adapter is named, and WARP fallback is named
		// too. Optional debug-layer warnings may accompany either path.
		CHECK(capture.calls() > 0);
		CHECK(capture.messages().find("Direct3D Adapter") != std::string_view::npos);
	}
	CHECK_FALSE(capture.overflow());
}
