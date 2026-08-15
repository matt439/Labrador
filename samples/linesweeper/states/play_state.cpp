#include "samples/linesweeper/states/play_state.h"

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

using namespace mattmath;
using namespace artattack;

namespace linesweeper
{
	namespace
	{
		// Spelt once, resolved to a handle once, never looked up per frame
		// (PHILOSOPHY T7, T8).
		const std::string font_name = "courier_new_bold_16";
	}

	PlayState::PlayState(Application* app) : app_(app)
	{

	}

	void PlayState::init()
	{
		const Vector2F resolution =
			this->app_->resolution_manager()->resolution_vec();

		RenderResources* resources = this->app_->render_resources();

		// One view, so no thread pool and no partitioner - the scene's fan-out
		// is over views, and a fan-out over one is a fan-out over nothing.
		this->scene_ = std::make_unique<Scene>(nullptr, nullptr);

		this->title_ = this->scene_->add(std::make_unique<Label>(
			L"LineSweeper", font_name, Vector2F(32.0f, 32.0f), resources,
			Colour::white));

		// The stub's whole output, and it is deliberately the two numbers this
		// sample is about: how big the well is, and how big the value that
		// holds it is. Both are compile-time constants, so this string is
		// built once in init() and never rebuilt - draw() is const and has
		// nothing to compute.
		const std::wstring status =
			L"well " + std::to_wstring(well_columns) + L"x" +
			std::to_wstring(well_visible_rows) + L" + " +
			std::to_wstring(well_buffer_rows) + L" buffer   |   sizeof(World) " +
			std::to_wstring(sizeof(World)) + L" bytes";

		this->status_ = this->scene_->add(std::make_unique<Label>(
			status, font_name, Vector2F(32.0f, resolution.y - 48.0f), resources,
			Colour::dark_gray));

		// Adds are pending until a tick ends, and the frame drawn before the
		// first update() is a real frame - so whoever fills a scene ends its
		// first tick by hand.
		this->scene_->end_tick();

		this->scene_->add_view(Viewport(RectangleF(Vector2F::ZERO, resolution)));
	}

	void PlayState::update(float dt)
	{
		// Nothing steps the World yet: rules/tick.h is the next commit, and
		// until it exists there is no verb to call. What is already true is
		// where the call will go - here, once, on one thread, before anything
		// is drawn - and that dt is a parameter rather than a member.
		//
		// The World will not be stepped with dt either. Every duration in the
		// rules is denominated in fixed ticks, which is what keeps a replay
		// independent of how long a frame took; dt belongs to the
		// presentation, which is the half that is allowed to be smooth.
		this->scene_->update(dt);
		this->scene_->end_tick();
	}

	void PlayState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer);
	}
}
