#pragma once

#include "engine/render/colour.h"
#include "engine/ui/button.h"
#include "engine/ui/navigation.h"

#include <vector>

namespace labrador
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

		// The third state, and the reason it is here rather than left to the
		// page: derived paint and page paint cannot share a widget. Every
		// mutation repaints every widget from focus, so a page that coloured a
		// row itself was writing over a colour the next add() or move() would
		// write back, and the header used to say so and offer nothing better.
		// A colour this group derives is the only kind that survives.
		Colour disabled = Colour::dark_gray;
	};

	// What activating a slot's focused entry did.
	//
	// AN ENUM RATHER THAN THE bool IT REPLACES, because that bool already
	// carried two meanings - "nothing is focused" and "this row has no action
	// bound", the second of which is most option rows - and a third would have
	// left it meaning nothing a caller could act on. The one a page has to act
	// on is `refused`: a screen that cannot say why a row is unavailable is a
	// screen that appears to have ignored the press.
	enum class Activation
	{
		// Nothing ran and nothing is wrong. No cursor is on anything, or the
		// entry it is on has no action - a row that changes a value on
		// left/right and means nothing on A.
		none,

		// The entry's action ran.
		ran,

		// The entry is disabled. The action was not run and the page is being
		// told, so that it can say what the row is waiting for. Which row it
		// was is focused(slot), because the reason is per row: "that mode is
		// not built yet" and "plug in a fourth pad" are different sentences.
		refused,
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
		//
		// A disabled widget is accepted, and is not that dead end. move()
		// takes `from` as itself rather than as a candidate, so a cursor on a
		// disabled row walks off it normally - which it has to, since the live
		// case puts it there without anyone asking.
		bool set_focused(int slot, const UiWidget* widget);

		// Walks to the nearest widget in `direction` and moves focus there.
		// Returns whether focus actually moved, which is the signal a caller
		// wants for "play the cursor sound".
		//
		// Disabled entries are not candidates and are jumped over rather than
		// stopped at: a disabled row between two live ones costs one press, not
		// two.
		bool move(int slot, Direction direction, bool wrap = true);

		// Runs the focused entry's action, unless it is disabled. See
		// Activation for what the three answers are for.
		Activation activate(int slot) const;

		// Whether the cursor may land on `widget`, and the setter that decides
		// it. Both answer false for a widget this group does not hold, which is
		// set_focused's rule and for set_focused's reason.
		//
		// A SETTER AND NOT A SECOND add(), because the case that cannot be
		// worked around is the live one. A page whose rows become available
		// while it is open - a fourth player count that needs a fourth pad,
		// and a pad can be plugged in between two frames - cannot express that
		// by choosing what to register: rebuilding the group every frame to
		// track it resets every cursor every frame with it. So enablement is a
		// property that changes, and registering a permanently dead row is the
		// same call made once.
		//
		// DISABLING THE FOCUSED ROW DOES NOT MOVE THE CURSOR. The player's
		// hand is on it and yanking it elsewhere mid-press is worse than
		// sitting on a row that answers `refused` and says why. Nothing here
		// hides a widget either: it stays drawn, in the disabled colour, which
		// is what makes it possible to say the row exists and is not ready.
		bool set_enabled(const UiWidget* widget, bool enabled);
		bool enabled(const UiWidget* widget) const;

		const FocusStyle& style() const;
		void set_style(const FocusStyle& style);

		// Repaints every widget from the current focus of every slot, and from
		// whether it is enabled at all. Called automatically by add,
		// set_focused, move, set_style and set_enabled; exposed because a page
		// that recolours a widget for its own reasons - a team tint - has to
		// put the focus paint back afterwards.
		//
		// A disabled row used to be the other example here, and it was the gap
		// rather than the case. What this offered it was a repaint fighting a
		// repaint, and the group won every time, because the next add() or
		// move() paints all of them back to one of two colours.
		void refresh_style() const;

	private:
		int index_of(const UiWidget* widget) const;

		// The widgets a cursor may reach, which is what the walk is given.
		// navigation.h asks for a candidate list and says what belongs in one -
		// "hidden widgets are the caller's business, pass the set that should
		// be reachable" - so an entry is skipped by not being offered, rather
		// than by a test inside the walk.
		std::vector<UiWidget*> enabled_visuals() const;

		std::vector<Button> buttons_;
		// One focused index per slot; -1 is "nothing", which only happens
		// before the first add() or after clear().
		std::vector<int> focused_;
		FocusStyle style_;
	};
}
