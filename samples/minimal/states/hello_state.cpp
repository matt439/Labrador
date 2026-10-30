#include "samples/minimal/states/hello_state.h"

#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "samples/minimal/states/confirm_state.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>

using namespace labrador;

namespace
{
	// Asset names, matching content/manifest.json. They are spelt once and
	// resolved to handles once (PHILOSOPHY T7, T8) - nothing below looks a
	// resource up by name per frame.
	const std::string font_name = "jetbrains_mono_bold_16";

	// How far the stick has to move before it counts. One number, applied
	// once, by the thing that knows what the stick is for - which is what
	// engine/input/gamepad.h asks for and what the pad API's own deadzone
	// modes take away.
	constexpr float stick_deadzone = 0.2f;

	// Where a touch marker's text sits relative to the point, so the "+" that
	// starts it is over the finger rather than below and right of it.
	const Vector2F touch_marker_offset = Vector2F(-5.0f, -9.0f);

	// The keyboard's answer to the question the stick answers, in the same
	// units: a direction, at most one unit per axis, +y down like everything
	// else in this engine.
	//
	// TWO KEYS PER DIRECTION AND NO TABLE, because four directions is not a
	// table yet - samples/linesweeper has nine bindings and earns one. Key
	// positions, not characters, which is what a movement binding wants: this
	// is the block of keys under the left hand whatever the layout prints on
	// them (keyboard.h).
	Vector2F keyboard_direction(const Keyboard& keyboard)
	{
		Vector2F direction = Vector2F::zero;

		if (keyboard.held(Key::left) || keyboard.held(Key::a))
		{
			direction.x -= 1.0f;
		}
		if (keyboard.held(Key::right) || keyboard.held(Key::d))
		{
			direction.x += 1.0f;
		}
		if (keyboard.held(Key::up) || keyboard.held(Key::w))
		{
			direction.y -= 1.0f;
		}
		if (keyboard.held(Key::down) || keyboard.held(Key::s))
		{
			direction.y += 1.0f;
		}

		return direction;
	}
}

HelloState::HelloState(Application* app) : app_(app)
{

}

void HelloState::init()
{
	const Vector2F resolution =
		this->app_->resolution_manager()->resolution_vec();
	this->motion_.centre = Vector2F(resolution.x * 0.5f - 120.0f,
		resolution.y * 0.5f);

	RenderResources* resources = this->app_->render_resources();

	// No thread pool and no partitioner: those are the scene's per-view
	// fan-out, and one view does not need one.
	this->scene_ = std::make_unique<Scene>(nullptr, nullptr);

	this->greeting_ = this->scene_->add(std::make_unique<Label>(
		L"Hello from the engine.", font_name, this->motion_.centre, resources,
		labrador::Colour::white));

	this->hint_ = this->scene_->add(std::make_unique<Label>(
		L"Left stick, arrows or WASD move it. B or Escape asks to quit. Touch shows a marker.", font_name,
		Vector2F(24.0f, resolution.y - 40.0f), resources,
		labrador::Colour::dark_gray));

	for (Label*& marker : this->touch_markers_)
	{
		marker = this->scene_->add(std::make_unique<Label>(L"", font_name,
			Vector2F::zero, resources, labrador::Colour::gold));
	}

	// Every add is pending until a tick ends, so that a weapon firing mid-tick
	// cannot invalidate the loop walking the objects. Nothing has ticked yet
	// and the first frame is a real frame, so the first tick ends here.
	this->scene_->end_tick();

	// One view, for the whole window, in screen space - the identity camera is
	// what a list starts with, so this one is never mentioned again. A
	// split-screen game clears this list and refills it every tick, one entry
	// per player, and nothing else about the shape changes.
	this->scene_->add_view(Viewport(RectangleF(Vector2F::zero, resolution)));
}

void HelloState::update(float dt)
{
	const Gamepads& pads = *this->app_->gamepads();
	const Keyboard& keyboard = *this->app_->keyboard();

	// A single-player game accepts every slot: the system need not put its
	// only pad in slot 0. Empty slots contribute neutral input, so neither
	// the stick nor the button needs a connected() guard. Several pads share
	// the greeting; their directions add, with the rules clamping the sum.
	bool quit_requested = keyboard.pressed(Key::escape);
	Vector2F stick_direction = Vector2F::zero;
	for (int slot = 0; slot < Gamepads::max_count; ++slot)
	{
		quit_requested |= pads.pressed(slot, GamepadButton::b);
		stick_direction += apply_deadzone(pads.state(slot).left_stick,
			stick_deadzone);
	}

	if (quit_requested)
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

	// The stick is already in this engine's convention - +y is down, the
	// same as every rectangle and viewport - and the deadzone is applied
	// here, once, with the magnitude rescaled so a small push moves slowly
	// instead of jumping to the threshold. The rules take both devices'
	// answers as directions and never see a device, which is what lets
	// tests/motion_tests.cpp run them with none.
	this->motion_ = step_motion(this->motion_,
		stick_direction,
		keyboard_direction(keyboard), dt);

	// A marker on every finger, at the point's client pixels - which are this
	// view's pixels, because the one view is the whole window in screen
	// space. A game with a camera turns them into world coordinates through
	// it, as it would the mouse's.
	const std::span<const TouchPoint> points = this->app_->touch()->points();
	for (size_t i = 0; i < this->touch_markers_.size(); ++i)
	{
		Label* marker = this->touch_markers_[i];
		if (i < points.size())
		{
			marker->set_text(L"+" + std::to_wstring(points[i].id));
			marker->set_position(Vector2F(
				static_cast<float>(points[i].position.x),
				static_cast<float>(points[i].position.y)) +
				touch_marker_offset);
		}
		else
		{
			marker->set_text(L"");
		}
	}

	// Where the greeting goes, worked out by the game's rules rather than by
	// the engine - nothing in the engine has an opinion about where a game's
	// things are.
	this->greeting_->set_position(greeting_position(this->motion_));

	// Steps every object in the scene, and admits or retires whatever this tick
	// asked for. Nothing here has anything to step, and that is the point: the
	// shape is the same whether the scene holds two labels or five thousand
	// objects.
	this->scene_->update(dt);
	this->scene_->end_tick();
}

void HelloState::on_suspend()
{
	this->greeting_->set_colour(labrador::Colour::dim_gray);
}

void HelloState::on_resume()
{
	this->greeting_->set_colour(labrador::Colour::white);
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
	// execute and Release - which would be five more lines here and three
	// caller obligations stated nowhere in the tree.
	//
	// A game with something over the world - a HUD, a divider, a countdown -
	// passes a second argument, and it runs per view, on that view's worker.
	// samples/local_multiplayer/play_state.cpp has one, a title bar over each
	// player's view.
	this->scene_->draw(renderer);
}
