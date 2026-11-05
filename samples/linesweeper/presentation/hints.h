#pragma once

#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/layout.h"

// The two control hints under the board in landscape, one line per device.
//
// PlayState draws them, and the pacing fixture, the frame bench and the
// capture tool draw the same two labels to reproduce its screen, so the text
// and the places are spelt here once. A binding that changes in
// states/play_state.cpp changes its line here.
namespace linesweeper
{
	inline constexpr const wchar_t* keyboard_hint_text =
		L"keyboard   arrows move   Z X rotate   space drops   C holds";
	inline constexpr const wchar_t* pad_hint_text =
		L"pad        d-pad moves   A B rotate   Y drops       X holds";

	// Left-aligned on the HUD column and columnised against each other by
	// hand, which only works because the font is monospaced.
	//
	// At the bottom of the landscape screen, which is board space rather
	// than the buffer: the camera puts it where the buffer's bottom is
	// (arrangement.h).
	inline constexpr float hint_x = hud_column_x;
	inline constexpr float keyboard_hint_y = landscape_height - 60.0f;
	inline constexpr float pad_hint_y = landscape_height - 36.0f;
}
