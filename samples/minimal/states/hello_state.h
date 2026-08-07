#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/text.h"
#include <memory>

// One screen: a line of text that follows the left stick, and B to quit.
//
// A state is the whole of what a game is to the engine (PHILOSOPHY, Structural
// types) - there is no IGame to implement. The engine calls init() once, then
// update() and draw() every frame, and never asks anything else of you.
//
// It borrows the Application rather than owning anything: the shell created
// every service before this was constructed and outlives it.
class HelloState : public State
{
public:
	explicit HelloState(Application* app);

	void init() override;
	void update() override;
	void draw() override;

private:
	Application* app_ = nullptr;

	// Built in init() rather than the constructor, because a Text resolves its
	// font name against RenderResources - and that only has the font once the
	// manifest has been walked.
	std::unique_ptr<Text> greeting_ = nullptr;
	std::unique_ptr<Text> hint_ = nullptr;

	MattMath::Vector2F position_ = { 0.0f, 0.0f };
};
