#pragma once

#include <functional>

namespace artattack
{
	class UiWidget;

	// A widget and what activating it means.
	//
	// The engine owns neither half. The visual is a loan - whatever the game
	// already draws, a label or an image - and the action is a callable the
	// game supplies. That split is the whole design: the engine knows which
	// widget the player is on and when they pressed the button, and it knows
	// nothing at all about what pressing it does.
	//
	// The action is stored once, when the page is built. It is not on the
	// frame path: nothing here is called per frame, only on the frame a
	// player activates something, so the indirection std::function costs is
	// paid once per press rather than per widget per frame - which is what
	// the string-keyed if-chain it replaces costs today.
	class Button
	{
	public:
		using Action = std::function<void()>;

		Button() = default;
		// A button with no action is legal and is not a mistake: pages use
		// focusable rows that change a value on left/right and do nothing on
		// A. activate() on one of those is a no-op, not a throw.
		explicit Button(UiWidget* visual, Action on_activate = nullptr);

		UiWidget* visual() const;
		bool has_action() const;

		// Runs the action if there is one. Returns whether anything ran, so a
		// caller can tell "nothing is bound here" from "something happened"
		// without asking twice.
		bool activate() const;

	private:
		UiWidget* visual_ = nullptr;
		Action on_activate_ = nullptr;
	};
}
