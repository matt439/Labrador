#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "samples/minimal/states/hello_state.h"

#include <Windows.h>

#include <memory>

using namespace labrador;

TEST_CASE("the template answers native close with one game-owned confirmation")
{
	ApplicationOptions options;
	options.window_class_name = L"MinimalCloseRequestTests";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
	Application app(options);
	app.initialize();
	app.load_manifest(GAME_MANIFEST);
	app.keyboard()->set_focused(true);
	app.keyboard()->poll();
	app.transition_to(std::make_unique<HelloState>(&app));
	StateContext& context = app;
	const HWND window = static_cast<HWND>(app.window());
	REQUIRE(window != nullptr);

	SendMessageW(window, WM_CLOSE, 0, 0);
	CHECK(context.depth() == 2);
	CHECK(IsWindow(window));
	SendMessageW(window, WM_CLOSE, 0, 0);
	SendMessageW(window, WM_SYSCOMMAND, SC_CLOSE, 0);
	CHECK(context.depth() == 2);
	CHECK(IsWindow(window));

	// Declining the question resumes the greeting, which answers a later X.
	app.keyboard()->on_key_down(Key::escape);
	app.keyboard()->poll();
	context.update(1.0f / 60.0f);
	CHECK(context.depth() == 1);
	app.keyboard()->on_key_up(Key::escape);
	app.keyboard()->poll();
	context.update(1.0f / 60.0f);
	SendMessageW(window, WM_CLOSE, 0, 0);
	CHECK(context.depth() == 2);

	SUBCASE("confirming quits finally without opening another question")
	{
		app.keyboard()->on_key_down(Key::enter);
		app.keyboard()->poll();
		context.update(1.0f / 60.0f);
		CHECK(context.depth() == 1);
		CHECK(app.window() == nullptr);
		CHECK_FALSE(IsWindow(window));
		MSG message = {};
		CHECK(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) != FALSE);
	}
	SUBCASE("teardown leaves an unanswered question without callbacks")
	{
		CHECK(IsWindow(window));
		CHECK(context.depth() == 2);
	}
}
