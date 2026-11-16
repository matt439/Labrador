#pragma once

#include "engine/input/keyboard.h"

#include <array>
#include <string_view>

namespace components
{
	enum class GalleryAction
	{
		restart_previews,
		reset_difficulty,
		count,
	};

	enum class BindingResult
	{
		changed,
		unchanged,
		duplicate,
		protected_key,
		invalid_token,
	};

	// The gallery's two shortcuts, independent of input devices and capture UI.
	// Games copy and change this policy: only letter and number-row positions
	// can be assigned here, leaving navigation, confirmation and cancellation
	// keys to the page. No engine action map or persistence is involved.
	class GalleryBindings
	{
	public:
		// Invalid actions throw std::invalid_argument, including count.
		labrador::Key key(GalleryAction action) const;

		// Refusal leaves both assignments unchanged. An invalid Key is protected.
		BindingResult try_bind(GalleryAction action, labrador::Key key);

		// Stable key_name tokens, never layout-dependent display labels.
		std::array<std::string_view, 2> tokens() const;

		// Parses both exact tokens before changing either assignment. A valid
		// pair may swap the current keys; duplicates within that pair are refused.
		BindingResult load_tokens(const std::array<std::string_view, 2>& tokens);

		// Restart previews on R, reset difficulty on D.
		void reset();

	private:
		std::array<labrador::Key, 2> keys_{ labrador::Key::r, labrador::Key::d };
	};
}
