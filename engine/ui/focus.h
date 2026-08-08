#pragma once

#include "engine/render/colour.h"
#include "engine/ui/button.h"
#include "engine/ui/navigation.h"

#include <vector>

namespace artattack
{
	class UiWidget;

	// How a focused widget is told apart from an unfocused one.
	//
	// A colour swap is what the game does today and it is all the mechanism
	// there is here; anything richer - a ring, a scale pulse, a sound - is the
	// game's, and it has the focus change to hang it on.
	struct FocusStyle
	{
		Colour focused = Colour::white;
		Colour unfocused = Colour::gray;
	};

	// A set of focusable widgets, which one each input slot is on, and the
	// walk between them.
	//
	// SLOTS. Focus is per slot from the start rather than retrofitted, because
	// split-screen is the stated case (PHILOSOPHY/UI: "per-viewport focus for
	// split-screen") and because retrofitting it means revisiting every page.
	// A page where every pad drives one shared cursor - which is every page in
	// the paint-shooter's main menu - uses one slot and passes 0. A page where
	// each player picks their own team or weapon gives each player a slot, and
	// gets for free the thing that currently does not exist anywhere in the
	// tree.
	//
	// WHY THE PAINT IS DERIVED, NOT APPLIED. Every mutation repaints every
	// widget from the focus of every slot. It would be cheaper to repaint only
	// the two that changed, and that is exactly the bug the game has: with one
	// cursor per pad, slot 0 leaving a widget slot 1 is still on would paint it
	// unfocused. Deriving the paint makes "focused" mean "some slot is on it",
	// which is the only definition that survives more than one cursor. The
	// cost is O(widgets) on a press, on a menu, and the widget count is single
	// digits.
	//
	// LOANS. Every widget here is a loan; the group stores raw pointers and
	// outlives nothing. The page owns its widgets and must outlive its
	// FocusGroup, which is the ordinary case since both are members of the
	// same page.
	class FocusGroup
	{
	public:
		// One slot unless told otherwise: the shared cursor is the common case
		// and the one a reader should not have to spell.
		explicit FocusGroup(int slot_count = 1,
			FocusStyle style = FocusStyle());

		// Registers a focusable widget. Declaration order does not decide
		// navigation - bounds() does - so this may be called in whatever order
		// reads best. Returns the widget back for convenience at call sites
		// that are building it in the same expression.
		//
		// The first widget added receives focus in every slot, so a page is
		// never in the "nothing is focused" state its first frame. Pages that
		// want a different starting widget call set_focused() after building.
		UiWidget* add(UiWidget* visual, Button::Action on_activate = nullptr);
		void clear();
		size_t size() const;

		int slot_count() const;

		UiWidget* focused(int slot) const;
		// Moves the slot's focus. A widget that is not in this group is
		// rejected rather than silently stored, because a focus pointing at
		// something the walk cannot reach is a dead end a player cannot
		// escape.
		bool set_focused(int slot, const UiWidget* widget);

		// Walks to the nearest widget in `direction` and moves focus there.
		// Returns whether focus actually moved, which is the signal a caller
		// wants for "play the cursor sound".
		bool move(int slot, Direction direction, bool wrap = true);

		// Runs the focused widget's action. Returns whether anything ran.
		bool activate(int slot) const;

		const FocusStyle& style() const;
		void set_style(const FocusStyle& style);

		// Repaints every widget from the current focus of every slot. Called
		// automatically by add/set_focused/move/set_style; exposed because a
		// page that recolours a widget for its own reasons - a disabled row, a
		// team tint - has to put the focus paint back afterwards.
		void refresh_style() const;

	private:
		int index_of(const UiWidget* widget) const;
		std::vector<UiWidget*> visuals() const;

		std::vector<Button> buttons_;
		// One focused index per slot; -1 is "nothing", which only happens
		// before the first add() or after clear().
		std::vector<int> focused_;
		FocusStyle style_;
	};
}
