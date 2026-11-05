#pragma once

// The names content/manifest.json gives the sample's two assets, spelt once.
//
// Every object that draws resolves its own handles from these on
// construction, never per frame (PHILOSOPHY T7, T8), except the particle
// field, which takes a resolved handle so that it can be stepped with no
// device at all (particles.h). The frame bench and the capture tool load and
// resolve the same two names, so renaming an asset is an edit here and in
// the manifest.
namespace linesweeper
{
	// The one white texel every block, panel, button and spark is drawn
	// from: a solid rectangle is a sprite with a tint (README, The whole
	// screen is one white texel).
	inline constexpr const char* block_texture_name = "white";

	// The one font, monospaced, which the hint lines' hand columns rely on
	// (hints.h).
	inline constexpr const char* font_name = "jetbrains_mono_bold_16";
}
