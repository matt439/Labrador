#pragma once

#include <functional>
#include <memory>

namespace labrador
{
	class UiWidget;

	// A widget, what activating it means, and whether it may be reached.
	//
	// The visual is a loan - whatever the game already draws, a label or an
	// image - and the button owns the callable the game supplies. That split
	// is the whole design: the engine knows which widget the player is on
	// and when they pressed the button, and it knows nothing at all about
	// what pressing it does.
	//
	// The action is stored once, when the page is built. It is not on the
	// frame path: nothing here is called per frame, only on the frame a
	// player activates something, so the indirection std::function costs is
	// paid once per press rather than per widget per frame - which is what a
	// string-keyed if-chain compared every frame would cost.
	class Button
	{
	public:
		using Action = std::function<void()>;

		Button() = default;
		// A button with no action is legal and is not a mistake: pages use
		// focusable rows that change a value on left/right and do nothing on
		// A. activate() on one of those is a no-op, not a throw.
		explicit Button(UiWidget* visual, Action on_activate = nullptr);
		// Copies copy the stored std::function into independent action storage.
		// Captures and reference-wrapped objects retain their own copying,
		// sharing, or borrowing semantics.
		Button(const Button& other);
		Button& operator=(const Button& other);
		Button(Button&&) noexcept = default;
		Button& operator=(Button&&) noexcept = default;

		UiWidget* visual() const;
		bool has_action() const;

		// Runs the action if there is one. Returns whether anything ran, so a
		// caller can tell "nothing is bound here" from "something happened"
		// without asking twice.
		// The stored action remains alive until it returns, even if it removes
		// this button or reallocates its FocusGroup. Mutable captures persist
		// between calls; activation does not copy the callable.
		// Each invocation retains ownership until return or exception. If the
		// button has gone, the last active invocation destroys the callable
		// (PHILOSOPHY T11's bounded ownership exception).
		//
		// It does not consult enabled(). Whether a disabled entry may be
		// activated is a question about a cursor, and a Button has none; the
		// answer lives one level up, in FocusGroup::activate.
		bool activate() const;

		// Whether a cursor may land here. True on construction, because a row
		// a page went to the trouble of registering is one it means to offer.
		//
		// A disabled entry is registered and unreachable, which is a different
		// thing from either of the two states around it. Not registering it at
		// all loses the press - there is no way to be on the row, so there is
		// nothing to refuse and nothing to explain - and registering it live
		// means the walk lands on it. This is the third state, and it is
		// separate from `on_activate_` being null on purpose: a row with no
		// action is one the player may sit on where A means nothing, which is
		// most option rows.
		bool enabled() const;
		void set_enabled(bool enabled);

	private:
		UiWidget* visual_ = nullptr;
		std::shared_ptr<Action> on_activate_;
		bool enabled_ = true;
	};
}
