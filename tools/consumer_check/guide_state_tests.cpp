// Behavioural checks of the guides' generated game code, in a native-null
// consumer. These fixtures drive the finished examples; they are not another
// implementation of the examples and do not enter the distributed starter.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "engine/render/null/recording.h"
#include "engine/render/resource_factory.h"
#include "game/states/hello_state.h"

#if defined(GUIDE_SCREEN)
#include "game/states/title_state.h"
#endif

#include <Windows.h>

#include <memory>
#include <vector>

using namespace labrador;

namespace
{
	ApplicationOptions hidden_options()
	{
		ApplicationOptions options;
		options.window_class_name = L"CompiledGuideStateTests";
		options.visible = false;
		options.max_threads = 1;
		options.view_capacity = 1;
		return options;
	}

	void step(Application& app)
	{
		app.keyboard()->poll();
		static_cast<StateContext&>(app).update(1.0f / 60.0f);
	}

	void prepare(Application& app)
	{
		app.initialize();
		app.load_manifest(GAME_MANIFEST);
		app.keyboard()->set_focused(true);
		app.keyboard()->poll();
	}

#if defined(GUIDE_SCREEN)
	class TitleWitness : public TitleState
	{
	public:
		TitleWitness(Application* app, bool* destroyed) :
			TitleState(app), destroyed_(destroyed) {}
		~TitleWitness() override { *this->destroyed_ = true; }
	private:
		bool* destroyed_;
	};
#else
	std::vector<RecordedSprite> blocks(Application& app)
	{
		Renderer& renderer = *app.renderer();
		renderer.begin_frame();
		static_cast<StateContext&>(app).draw(renderer);
		renderer.submit();
		renderer.end_frame();
		const TextureHandle white = app.render_resources()->resolve_texture(white_texture_name);
		std::vector<RecordedSprite> result;
		for (const RecordedSprite& sprite : recorded_sprites(renderer))
		{
			if (sprite.texture == white)
			{
				result.push_back(sprite);
			}
		}
		return result;
	}
#endif
}

#if defined(GUIDE_SCREEN)
TEST_CASE("the guide's title transitions to the greeting on Enter")
{
	bool destroyed = false;
	Application app(hidden_options());
	prepare(app);
	app.transition_to(std::make_unique<TitleWitness>(&app, &destroyed));
	app.keyboard()->on_key_down(Key::enter);
	step(app);
	CHECK(destroyed);
	CHECK(app.depth() == 1);
	app.keyboard()->on_key_up(Key::enter);
	step(app);
	// HelloState's Escape opens a question, unlike the title's immediate quit.
	app.keyboard()->on_key_down(Key::escape);
	step(app);
	CHECK(app.depth() == 2);
	CHECK(app.window() != nullptr);
}

TEST_CASE("the guide's title quits immediately on Escape")
{
	Application app(hidden_options());
	prepare(app);
	app.transition_to(std::make_unique<TitleState>(&app));
	app.keyboard()->on_key_down(Key::escape);
	step(app);
	CHECK(app.window() == nullptr);
	MSG message = {};
	CHECK(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) != FALSE);
}
#else
TEST_CASE("the guide's actual greeting state stops and slides its block at the wall")
{
	Application app(hidden_options());
	prepare(app);
	app.transition_to(std::make_unique<HelloState>(&app));
	app.keyboard()->on_key_down(Key::right);
	for (int tick = 0; tick < 60; ++tick)
	{
		step(app);
	}
	std::vector<RecordedSprite> drawn = blocks(app);
	REQUIRE(drawn.size() == 2);
	CHECK(drawn[0].corners[0].position == Vector2F(800.0f, 160.0f));
	CHECK(drawn[1].corners[0].position.x == doctest::Approx(768.0f));
	CHECK(drawn[1].corners[0].position.y == doctest::Approx(420.0f));
	CHECK(drawn[1].corners[0].colour == Colour::goldenrod);

	app.keyboard()->on_key_down(Key::up);
	for (int tick = 0; tick < 10; ++tick)
	{
		step(app);
	}
	drawn = blocks(app);
	REQUIRE(drawn.size() == 2);
	CHECK(drawn[1].corners[0].position.x == doctest::Approx(768.0f));
	CHECK(drawn[1].corners[0].position.y < 420.0f);
	app.keyboard()->on_key_up(Key::right);
	app.keyboard()->on_key_up(Key::up);
	step(app);
	drawn = blocks(app);
	REQUIRE(drawn.size() == 2);
	CHECK(drawn[1].corners[0].colour == Colour::deep_sky_blue);
}
#endif
