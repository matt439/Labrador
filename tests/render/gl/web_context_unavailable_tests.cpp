#include <doctest/doctest.h>

#include "engine/render/device_unavailable.h"
#include "engine/render/renderer.h"
#include "tests/render/test_window.h"

#include <emscripten/em_js.h>

#include <stdexcept>
#include <string>

EM_JS_DEPS(labrador_web_context_test, "$UTF8ToString");

EM_JS(void, labrador_test_refuse_webgl_context, (const char* target, int mode), {
	const canvas = document.querySelector(UTF8ToString(target));
	const request = Object.getOwnPropertyDescriptor(canvas, "getContext");
	canvas.labradorContextRefusal = { request };
	canvas.getContext = function (kind, ...args) {
		if (kind === "webgl2") {
			if (mode === 1) {
				canvas.dispatchEvent(new WebGLContextEvent("webglcontextcreationerror", {
					statusMessage: "Synthetic browser device requirement refusal"
				}));
			}
			return null;
		}
		return HTMLCanvasElement.prototype.getContext.call(this, kind, ...args);
	};
});

EM_JS(void, labrador_test_restore_webgl_context, (const char* target), {
	const canvas = document.querySelector(UTF8ToString(target));
	const original = canvas.labradorContextRefusal?.request;
	if (original) {
		Object.defineProperty(canvas, "getContext", original);
	} else {
		delete canvas.getContext;
	}
	delete canvas.labradorContextRefusal;
});

EM_JS(int, labrador_test_bind_2d_context, (const char* target), {
	return document.querySelector(UTF8ToString(target)).getContext("2d") ? 1 : 0;
});

EM_JS(void, labrador_test_hide_webgl2_api, (int hide), {
	if (hide) {
		Module.labradorWebgl2ApiDescriptor = Object.getOwnPropertyDescriptor(globalThis, "WebGL2RenderingContext");
		Object.defineProperty(globalThis, "WebGL2RenderingContext", {
			configurable: true, writable: true, value: undefined
		});
	} else {
		const original = Module.labradorWebgl2ApiDescriptor;
		if (original) {
			Object.defineProperty(globalThis, "WebGL2RenderingContext", original);
		} else {
			delete globalThis.WebGL2RenderingContext;
		}
		delete Module.labradorWebgl2ApiDescriptor;
	}
});

namespace
{
	class ContextRefusal
	{
	public:
		ContextRefusal(void* target, int mode) : target_(static_cast<const char*>(target))
		{
			labrador_test_refuse_webgl_context(this->target_, mode);
		}
		~ContextRefusal() { labrador_test_restore_webgl_context(this->target_); }
		ContextRefusal(const ContextRefusal&) = delete;
		ContextRefusal& operator=(const ContextRefusal&) = delete;

	private:
		const char* target_;
	};

	class MissingWebgl2Api
	{
	public:
		MissingWebgl2Api() { labrador_test_hide_webgl2_api(1); }
		~MissingWebgl2Api() { labrador_test_hide_webgl2_api(0); }
		MissingWebgl2Api(const MissingWebgl2Api&) = delete;
		MissingWebgl2Api& operator=(const MissingWebgl2Api&) = delete;
	};
}

TEST_CASE("an explicitly absent WebGL2 API has the renderer-unavailable category")
{
	render_tests::TestWindow window(64, 64);
	const MissingWebgl2Api missing;
	labrador::Renderer renderer;
	try
	{
		renderer.create_device(window.handle(), 64, 64, 1);
		FAIL("a browser without the required WebGL2 API was accepted");
	}
	catch (const labrador::RenderDeviceUnavailable& error)
	{
		CHECK(std::string(error.what()).find("WebGL2RenderingContext API") != std::string::npos);
	}
}

TEST_CASE("a WebGL creation-error event retains native detail without guessing device absence")
{
	render_tests::TestWindow window(64, 64);
	const ContextRefusal refusal(window.handle(), 1);
	labrador::Renderer renderer;
	try
	{
		renderer.create_device(window.handle(), 64, 64, 1);
		FAIL("the failed WebGL drawing-buffer creation was accepted");
	}
	catch (const labrador::RenderDeviceUnavailable&)
	{
		FAIL("an ambiguous context-creation event became device absence");
	}
	catch (const std::runtime_error& error)
	{
		CHECK(std::string(error.what()).find("WebGL2 context") != std::string::npos);
		CHECK(std::string(error.what()).find("Synthetic browser device requirement refusal") !=
			std::string::npos);
	}
}

TEST_CASE("a bare WebGL null and an existing 2D canvas remain ordinary startup errors")
{
	SUBCASE("a null without a creation-error event is not classified by guesswork")
	{
		render_tests::TestWindow window(64, 64);
		const ContextRefusal refusal(window.handle(), 2);
		labrador::Renderer renderer;
		try
		{
			renderer.create_device(window.handle(), 64, 64, 1);
			FAIL("the unclassified null was accepted");
		}
		catch (const labrador::RenderDeviceUnavailable&)
		{
			FAIL("a bare null was reported as proven device unavailability");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find("No context-creation error") != std::string::npos);
		}
	}
	SUBCASE("the real browser refuses a canvas already bound to 2D")
	{
		render_tests::TestWindow window(64, 64);
		REQUIRE(labrador_test_bind_2d_context(static_cast<const char*>(window.handle())) == 1);
		labrador::Renderer renderer;
		try
		{
			renderer.create_device(window.handle(), 64, 64, 1);
			FAIL("a canvas bound to an incompatible context was accepted");
		}
		catch (const labrador::RenderDeviceUnavailable&)
		{
			FAIL("existing canvas state became device absence");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find("WebGL2 context") != std::string::npos);
		}
	}
}

TEST_CASE("a missing or non-canvas WebGL target remains a caller error")
{
	labrador::Renderer renderer;
	CHECK_THROWS_AS(renderer.create_device(nullptr, 64, 64, 1), std::invalid_argument);
	std::string missing = "#labrador-no-such-canvas";
	CHECK_THROWS_AS(renderer.create_device(missing.data(), 64, 64, 1), std::invalid_argument);
	std::string non_canvas = "body";
	CHECK_THROWS_AS(renderer.create_device(non_canvas.data(), 64, 64, 1), std::invalid_argument);
}
