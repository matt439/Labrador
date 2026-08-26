#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "engine/math/vector2f.h"
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

	// The two numbers the greeting's placement is worked out from every frame:
	// the point it circles, which the stick moves, and how far round the
	// circle it currently is. update() turns the pair into a transform - the
	// engine has no opinion about where anything is, and this is what doing it
	// by hand looks like.
	mattmath::Vector2F position_ = { 0.0f, 0.0f };
	float spin_ = 0.0f;
};
