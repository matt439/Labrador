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
	//  1. A candidate is considered if its centre is strictly beyond `from`'s
	//     centre along the axis of travel. Ties on that axis are skipped, so a
	//     row of widgets never lets "up" land on a sibling beside you.
	//  2. Among those, the winner minimises (primary, cross) distance in that
	//     order: primary is how far along the axis of travel, cross is how far
	//     off it. Travelling down a column of left-aligned labels therefore
	//     goes to the next label down; in a grid it goes to the one below
	//     rather than the one below-and-across.
	//  3. If nothing lies that way and `wrap` is true, the walk continues from
	//     the far edge: among the candidates strictly *behind* you, the
	//     furthest one wins, so a column cycles. Every hand-written table in
	//     the game today wraps, which is why it is the default. "Strictly
	//     behind" is what keeps a wrap on one axis from answering a press on
	//     the other: in a left-aligned column every row shares a centre x, so
	//     left and right have nothing ahead *and* nothing behind, and the
	//     answer is that nothing happens.
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
