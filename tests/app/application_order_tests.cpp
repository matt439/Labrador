#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/renderer.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

// Application's three steps taken out of order: a main.cpp that lost its
// initialize() line is a message naming the call, not an access violation
// (T6). No case here calls initialize(), so none opens a window or makes a
// device, and they run under every native render and audio backend.
//
// Not compiled for a browser, where run()'s contract is the page's
// (application.h); the guard itself is the same code on both platforms.

namespace
{
	using namespace labrador;

	ApplicationOptions uninitialized_options(const wchar_t* class_name)
	{
		ApplicationOptions options;
		options.window_class_name = class_name;
		options.visible = false;
		options.view_capacity = 1;
		options.max_threads = 1;
		return options;
	}

	// Writes down whether it was ever entered.
	class EntryWitness : public State
	{
	public:
		explicit EntryWitness(bool* entered) : entered_(entered) {}

		void init() override { *this->entered_ = true; }
		void update(float /*dt*/) override {}
		void draw(Renderer& /*renderer*/) const override {}

	private:
		bool* entered_ = nullptr;
	};
}

TEST_CASE("load_manifest before initialize throws, naming both calls")
{
	Application app(uninitialized_options(L"LoadManifestBeforeInitializeTestWindow"));

	try
	{
		app.load_manifest("./manifest.json");
		FAIL("load_manifest() ran with no loader to load into");
	}
	catch (const std::logic_error& error)
	{
		const std::string message = error.what();
		CHECK(message.find("Application::load_manifest") != std::string::npos);
		CHECK(message.find("Application::initialize()") != std::string::npos);
	}

	// Still the Application it was: nothing was built on the way to the throw.
	CHECK(app.window() == nullptr);
	CHECK(app.resource_loader() == nullptr);
}

TEST_CASE("run before initialize throws, naming both calls, and enters no state")
{
	Application app(uninitialized_options(L"RunBeforeInitializeTestWindow"));

	bool entered = false;
	try
	{
		std::ignore = app.run(std::make_unique<EntryWitness>(&entered));
		FAIL("run() ran with no window to pump");
	}
	catch (const std::logic_error& error)
	{
		const std::string message = error.what();
		CHECK(message.find("Application::run") != std::string::npos);
		CHECK(message.find("Application::initialize()") != std::string::npos);
	}

	CHECK_FALSE(entered);
	CHECK(app.depth() == 0);
	CHECK(app.window() == nullptr);
}
