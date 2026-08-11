#include "samples/minimal/states/confirm_state.h"

#include "engine/core/state_context.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

using namespace DirectX;
using namespace mattmath;
using namespace artattack;

namespace
{
	const std::string font_name = "courier_new_bold_16";
}

ConfirmState::ConfirmState(Application* app, std::wstring question) :
	app_(app), question_(std::move(question))
{

}

void ConfirmState::init()
{
	const Vector2F resolution =
		this->app_->resolution_manager()->resolution_vec();

	this->prompt_ = std::make_unique<Text>(
		this->question_ + L"   A = yes, B = no", font_name,
		Vector2F(resolution.x * 0.5f - 200.0f, resolution.y * 0.5f + 60.0f),
		this->app_->render_resources(), artattack::Colour::goldenrod);
}

void ConfirmState::update(float /*dt*/)
{
	const Gamepads& pads = *this->app_->gamepads();

	// pressed(), not held(): the B that opened this question is still down on
	// the frame it opens, and the engine polls every frame whatever is running,
	// so that B was down last frame too and is not a press here. A state does
	// not have to notice the transition it was created by.
	//
	// Popping from inside update() is the ordinary case, and it is safe: the
	// stack defers it until this call has returned, so the object whose
	// update() is running is never destroyed underneath it.
	if (pads.pressed(0, GamepadButton::a))
	{
		this->context()->pop(true);
	}
	else if (pads.pressed(0, GamepadButton::b))
	{
		this->context()->pop(false);
	}
}

void ConfirmState::draw(Renderer& renderer) const
{
	// The state below has already drawn into this view, and this draws over it.
	// Nothing here has to know that, and it does not set the view count: the
	// state that fills the screen decides how many views the frame has.
	DrawList list = renderer.view(0);
	this->prompt_->draw(list);
}
