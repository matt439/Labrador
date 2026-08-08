#pragma once

#include "engine/math/matt_math.h"

#include <vector>

namespace artattack
{
	class UiWidget;

	// Which way the player pushed. Produced by the input module from a stick
	// or a d-pad; consumed here as a pure direction with no device in it.
	enum class Direction
	{
		none,
		up,
		down,
		left,
		right,
	};

	// The widget a move in `direction` should land on, or nullptr if there is
	// nowhere to go.
	//
	// This is the whole of controller navigation, and it is arithmetic over
	// bounds(). Nothing else about a widget matters: not its type, not its
	// name, not the order it was declared in. That is the point - the game
	// today keeps a per-screen adjacency table written by hand
	// (game/states/main_menu.cpp is 1,976 lines and a large fraction of it is
	// four if-chains per page, keyed by std::string and compared every frame),
	// and every one of those tables is derivable from where the widgets are.
	//
	// The rule, in full, because T3 says a simple model still has exact
	// semantics:
	//
	//  1. A candidate is ahead if it clears `from` on the axis of travel - its
	//     trailing edge is at or past `from`'s leading one, give or take half
	//     of the smaller box, so rows that overlap slightly still navigate.
	//     Edges and not centres, and that is the whole of why: a left-aligned
	//     column
	//     of labels with different text lengths has a different centre x on
	//     every row, so a centre test makes "right" from "Standard" find "Team
	//     Deathmatch" one row down. By edges, nothing in that column clears
	//     another row's right edge and left/right correctly find nothing.
	//  2. Among those ahead, the winner minimises the gap, tie-broken by how
	//     far off the line of travel it sits. Down a column that is the next
	//     row; in a grid it is the cell below rather than the one below and
	//     across.
	//  3. If nothing lies that way and `wrap` is true, the walk continues from
	//     the far edge: among the candidates *behind* you - the same test with
	//     the direction reversed - the furthest one wins, so a column cycles.
	//     Every hand-written table in the game today wraps, which is why it is
	//     the default. Defining "behind" that way, rather than as "everything
	//     not ahead", is what stops a wrap on one axis from answering a press
	//     on the other.
	//  4. `from` is never its own answer, and candidates with a degenerate
	//     (zero-area) box are skipped - an empty container reports one, and
	//     focus landing on nothing that can be seen is not navigation.
	//
	// Hidden widgets are the caller's business, not this function's: pass the
	// set that should be reachable. A page that hides a row also stops
	// offering it.
	//
	// `candidates` holds loans; nothing here keeps a pointer past the call.
	UiWidget* nearest_in_direction(const UiWidget& from,
		Direction direction,
		const std::vector<UiWidget*>& candidates,
		bool wrap = true);

	// The same walk over bare rectangles, which is what it really is. Exposed
	// because it is the testable half and because a caller with geometry but
	// no widgets (a grid of spawn points, a tile picker) should not have to
	// invent widgets to use it. Returns an index into `candidates`, or -1.
	int nearest_in_direction(const mattmath::RectangleF& from,
		Direction direction,
		const std::vector<mattmath::RectangleF>& candidates,
		bool wrap = true);
}
