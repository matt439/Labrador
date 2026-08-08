#include "samples/minimal/states/hello_state.h"

#include "engine/core/state_context.h"
#include "samples/minimal/states/confirm_state.h"

using namespace DirectX;
using namespace mattmath;
using namespace artattack;

namespace
{
	// Asset names, matching content/manifest.json. They are spelt once and
	// resolved to handles once (PHILOSOPHY T7, T8) - nothing below looks a
	// resource up by name per frame.
	const std::string font_name = "courier_new_bold_16";

	constexpr float move_speed = 400.0f;
}

HelloState::HelloState(Application* app) : app_(app)
{

}

void HelloState::init()
{
	const Vector2F resolution =
		this->app_->resolution_manager()->resolution_vec();
	this->position_ = Vector2F(resolution.x * 0.5f - 120.0f,
		resolution.y * 0.5f);

	RenderResources* resources = this->app_->render_resources();

	// No thread pool and no partitioner: those are the scene's per-view
	// fan-out, and one view does not need one.
	this->scene_ = std::make_unique<Scene>(nullptr, nullptr);

	this->greeting_ = this->scene_->add(std::make_unique<Label>(
		L"Hello from the engine.", font_name, this->position_, resources,
		colour_consts::WHITE));

	this->hint_ = this->scene_->add(std::make_unique<Label>(
		L"Left stick moves it. B quits.", font_name,
		Vector2F(24.0f, resolution.y - 40.0f), resources,
		colour_consts::DARK_GRAY));

	// Every add is pending until a tick ends, so that a weapon firing mid-tick
	// cannot invalidate the loop walking the objects. Nothing has ticked yet
	// and the first frame is a real frame, so the first tick ends here.
	this->scene_->end_tick();

	// One view, for the whole window, in screen space - the identity camera is
	// what a list starts with, so this one is never mentioned again. A
	// split-screen game clears this list and refills it every tick, one entry
	// per player, and nothing else about the shape changes.
	this->scene_->add_view(Viewport(RectangleF(Vector2F::ZERO, resolution)));
}

void HelloState::update(float dt)
{
	const GamePad::State pad = this->app_->gamepad()->GetState(0);
	if (pad.IsConnected())
	{
		if (pad.IsBPressed())
		{
			// Ask, rather than quit. The result type is named here and matched
			// at the pop; the question knows nothing about this state, and the
			// callback runs once, when it closes, rather than being a flag read
			// on every frame until something sets it.
			this->context()->push<bool>(
				std::make_unique<ConfirmState>(this->app_, L"Really quit?"),
				[this](const bool& quit)
				{
					if (quit)
					{
						this->app_->quit();
					}
				});
			return;
		}

		this->position_.x += pad.thumbSticks.leftX * move_speed * dt;
		this->position_.y -= pad.thumbSticks.leftY * move_speed * dt;
		this->greeting_->set_position(this->position_);
	}

	// Steps every object in the scene, and admits or retires whatever this tick
	// asked for. Nothing here has anything to step, and that is the point: the
	// shape is the same whether the scene holds two labels or five thousand
	// paint tiles.
	this->scene_->update(dt);
	this->scene_->end_tick();
}

void HelloState::on_suspend()
{
	this->greeting_->set_colour(colour_consts::DIM_GRAY);
}

void HelloState::on_resume()
{
	this->greeting_->set_colour(colour_consts::WHITE);
}

void HelloState::draw(Renderer& renderer) const
{
	// The whole of drawing a screen. The scene declares this frame's views off
	// the list init() filled, culls each object to what that view can see and
	// draws it - so there is no loop over the game's own objects here, and
	// there is nothing to keep in step when one is added.
	//
	// Nothing below names a graphics type. There is no deferred context to
	// index, no sprite batch to Begin and End, and no command list to finish,
	// execute and Release - which was five of the eleven lines this function
	// used to be, and three caller obligations stated nowhere in the tree.
	//
	// A game with something over the world - a HUD, a divider, a countdown -
	// passes a second argument, and it runs per view, on that view's worker.
	// game/objects/level.cpp has one.
	this->scene_->draw(renderer);
}
