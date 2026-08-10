#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/text.h"
#include <memory>
#include <string>

// A question put over whatever asked it, which answers by closing.
//
// This is the second half of what a state is on this engine. HelloState shows
// one screen; this shows the other thing the stack does - a state pushed above
// another, with the one below suspended but still drawn, and a typed result
// handed back when this one pops (engine/core/state_context.h).
//
// It knows nothing about who pushed it. There is no pointer back to HelloState,
// no callback stored here, and no shared flag polled by somebody: the caller
// named the result type at the push, this one names it at the pop, and the
// engine is what joins them.
class ConfirmState : public artattack::State
{
public:
	ConfirmState(artattack::Application* app, std::wstring question);

	void init() override;
	void update(float dt) override;
	void draw(artattack::Renderer& renderer) const override;

	// A box over the screen, not a replacement for it - so the state below
	// keeps drawing and this appears on top of it.
	bool covers_screen() const override { return false; }

private:
	artattack::Application* app_ = nullptr;
	std::wstring question_;

	std::unique_ptr<artattack::Text> prompt_ = nullptr;
};
