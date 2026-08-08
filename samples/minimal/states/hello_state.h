#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include <memory>

// One screen: a line of text that follows the left stick, and B to quit -
// which asks first, by putting a second state on top of this one.
//
// A state is the whole of what a game is to the engine (PHILOSOPHY, Structural
// types) - there is no IGame to implement. The engine calls init() once, then
// update() and draw() every frame, and never asks anything else of you.
//
// It borrows the Application rather than owning anything: the shell created
// every service before this was constructed and outlives it.
class HelloState : public artattack::State
{
public:
	explicit HelloState(artattack::Application* app);

	void init() override;
	void update(float dt) override;
	void draw(artattack::Renderer& renderer) const override;

	// Something is above this state, or has just left. update() stops being
	// called either way - the stack sees to that - so these are for what a game
	// does *besides* stepping: a paused mix, a dimmed world, a saved
	// checkpoint. Dimming is what this one has.
	void on_suspend() override;
	void on_resume() override;

private:
	artattack::Application* app_ = nullptr;

	// What is on screen, and where it is watched from. A game does not loop
	// over its own objects to draw them: it registers them once and says how
	// many views the frame has. This one has one view, so the scene needs no
	// thread pool - a fan-out over a single pane is a fan-out over nothing.
	std::unique_ptr<artattack::Scene> scene_ = nullptr;

	// Built in init() rather than the constructor, because a Label resolves its
	// font name against RenderResources - and that only has the font once the
	// manifest has been walked.
	//
	// Borrowed, not owned: the scene owns everything added to it, and add()
	// hands back a pointer of the object's own type for exactly this - the
	// things the game still has something to say to.
	artattack::Label* greeting_ = nullptr;
	artattack::Label* hint_ = nullptr;

	mattmath::Vector2F position_ = { 0.0f, 0.0f };
};
