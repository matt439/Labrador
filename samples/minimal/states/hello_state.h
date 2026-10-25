#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/input/touch.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/minimal/rules/motion.h"
#include <array>
#include <memory>

// One screen: a line of text that circles a point the stick or the keyboard
// moves, and B or Escape to quit - which asks first, by putting a second state
// on top of this one.
//
// BOTH DEVICES, BECAUSE THIS IS THE TEMPLATE A NEW PROJECT IS COPIED FROM.
// It read the pad alone once, wrapped in `if (pads.connected(0))`, and the
// first thing a stranger with no controller met was a window that could only
// be closed with Alt+F4. The replacement is the shape worth copying as much as
// the fix: neither device is asked whether it is there, because neither has to
// be (update(), and engine/input/gamepad.h on why that guard is a trap).
//
// A state is the whole of what a game is to the engine (PHILOSOPHY, Structural
// types) - there is no IGame to implement. The engine calls init() once, then
// update() and draw() every frame, and never asks anything else of you.
//
// It borrows the Application rather than owning anything: the shell created
// every service before this was constructed and outlives it.
class HelloState : public labrador::State
{
public:
	explicit HelloState(labrador::Application* app);

	void init() override;
	void update(float dt) override;
	void draw(labrador::Renderer& renderer) const override;

	// Something is above this state, or has just left. update() stops being
	// called either way - the stack sees to that - so these are for what a game
	// does *besides* stepping: a paused mix, a dimmed world, a saved
	// checkpoint. Dimming is what this one has.
	void on_suspend() override;
	void on_resume() override;

private:
	labrador::Application* app_ = nullptr;

	// What is on screen, and where it is watched from. A game does not loop
	// over its own objects to draw them: it registers them once and says how
	// many views the frame has. This one has one view, so the scene needs no
	// thread pool - a fan-out over a single pane is a fan-out over nothing.
	std::unique_ptr<labrador::Scene> scene_ = nullptr;

	// Built in init() rather than the constructor, because a Label resolves its
	// font name against RenderResources - and that only has the font once the
	// manifest has been walked.
	//
	// Borrowed, not owned: the scene owns everything added to it, and add()
	// hands back a pointer of the object's own type for exactly this - the
	// things the game still has something to say to.
	labrador::Label* greeting_ = nullptr;
	labrador::Label* hint_ = nullptr;

	// One marker per finger the touch device can hold, each showing the
	// point's id where the point is, and blank while there is no point for
	// it. Not a game mechanic: it is how a person checking a touchscreen sees
	// that every finger arrives once, where it is, with an id of its own.
	std::array<labrador::Label*, labrador::TouchState::capacity>
		touch_markers_ = {};

	// Where the greeting is, as the template's rules keep it: the point it
	// circles and how far round it is. update() hands them what the devices
	// said and places the label where they answer - the engine has no opinion
	// about where anything is, and rules/motion.h is what doing it by hand
	// looks like.
	Motion motion_;
};
