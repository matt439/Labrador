#include "samples/minimal/states/confirm_state.h"

#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

using namespace DirectX;
using namespace mattmath;
using namespace labrador;

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

	// Both answers on both devices, and the offset is roughly half the line's
	// width in a monospaced font. Hand-placed and hand-recentred when the line
	// changed, which is what this engine asks a game to do - it has no
	// autolayout and says so (PHILOSOPHY, UI).
	this->prompt_ = std::make_unique<Text>(
		this->question_ + L"   A or Enter = yes, B or Escape = no", font_name,
		Vector2F(resolution.x * 0.5f - 310.0f, resolution.y * 0.5f + 60.0f),
		this->app_->render_resources(), labrador::Colour::goldenrod);
}

void ConfirmState::update(float /*dt*/)
{
	const Gamepads& pads = *this->app_->gamepads();
	const Keyboard& keyboard = *this->app_->keyboard();

	// pressed(), not held(): the B - or the Escape - that opened this question
	// is still down on the frame it opens, and the engine polls every frame
	// whatever is running, so that button was down last frame too and is not a
	// press here. A state does not have to notice the transition it was
	// created by.
	//
	// The device that opened the question is not the device that has to answer
	// it. Nothing here remembers which one did, and nothing should: a player
	// who quits with Escape and reaches for the pad gets the same dialog.
	//
	// Popping from inside update() is the ordinary case, and it is safe: the
	// stack defers it until this call has returned, so the object whose
	// update() is running is never destroyed underneath it.
	if (pads.pressed(0, GamepadButton::a) || keyboard.pressed(Key::enter))
	{
		this->context()->pop(true);
	}
	else if (pads.pressed(0, GamepadButton::b) ||
		keyboard.pressed(Key::escape))
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
