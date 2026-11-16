#include "samples/components/gallery_bindings.h"

#include <cstddef>
#include <optional>
#include <stdexcept>

namespace components
{
	namespace
	{
		std::size_t action_index(GalleryAction action)
		{
			switch (action)
			{
			case GalleryAction::restart_previews: return 0;
			case GalleryAction::reset_difficulty: return 1;
			default: throw std::invalid_argument("GalleryBindings: invalid gallery action.");
			}
		}

		bool assignable(labrador::Key key)
		{
			return (key >= labrador::Key::a && key <= labrador::Key::z) ||
				(key >= labrador::Key::digit_0 && key <= labrador::Key::digit_9);
		}
	}

	labrador::Key GalleryBindings::key(GalleryAction action) const
	{
		return this->keys_[action_index(action)];
	}

	BindingResult GalleryBindings::try_bind(GalleryAction action, labrador::Key key)
	{
		const std::size_t index = action_index(action);
		if (!assignable(key)) return BindingResult::protected_key;
		if (this->keys_[index] == key) return BindingResult::unchanged;
		const std::size_t other = 1 - index;
		if (this->keys_[other] == key) return BindingResult::duplicate;
		this->keys_[index] = key;
		return BindingResult::changed;
	}

	std::array<std::string_view, 2> GalleryBindings::tokens() const
	{
		return { labrador::key_name(this->keys_[0]), labrador::key_name(this->keys_[1]) };
	}

	BindingResult GalleryBindings::load_tokens(const std::array<std::string_view, 2>& tokens)
	{
		const std::optional<labrador::Key> first = labrador::key_from_name(tokens[0]);
		const std::optional<labrador::Key> second = labrador::key_from_name(tokens[1]);
		if (!first || !second) return BindingResult::invalid_token;
		if (!assignable(*first) || !assignable(*second)) return BindingResult::protected_key;
		if (*first == *second) return BindingResult::duplicate;
		const std::array<labrador::Key, 2> parsed{ *first, *second };
		if (parsed == this->keys_) return BindingResult::unchanged;
		this->keys_ = parsed;
		return BindingResult::changed;
	}

	void GalleryBindings::reset()
	{
		this->keys_ = { labrador::Key::r, labrador::Key::d };
	}
}
