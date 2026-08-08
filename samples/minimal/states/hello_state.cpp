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

	this->greeting_ = std::make_unique<Text>(L"Hello from the engine.",
		font_name, this->position_, resources, colour_consts::WHITE);

	this->hint_ = std::make_unique<Text>(
		L"Left stick moves it. B quits.", font_name,
		Vector2F(24.0f, resolution.y - 40.0f), resources,
		colour_consts::DARK_GRAY);
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
	// One view: the whole window, in screen space. A split-screen game says
	// set_view_count(players) here and fills each one; the shape does not
	// otherwise change, which is the point of the seam.
	//
	// Nothing below names a graphics type. There is no deferred context to
	// index, no sprite batch to Begin and End, and no command list to finish,
	// execute and Release - which was five of the eleven lines this function
	// used to be, and three caller obligations stated nowhere in the tree.
	renderer.set_view_count(1);
	DrawList list = renderer.view(0);

	this->greeting_->draw(list);
	this->hint_->draw(list);
}
