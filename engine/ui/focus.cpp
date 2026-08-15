#include "engine/ui/focus.h"

#include "engine/ui/widget.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace labrador
{
	FocusGroup::FocusGroup(int slot_count, FocusStyle style) :
		style_(style)
	{
		if (slot_count < 1)
		{
			// Loud, and at construction: a zero-slot group accepts every call
			// and does nothing, which is a menu that ignores the player.
			throw std::invalid_argument(
				"FocusGroup: slot_count must be at least 1");
		}
		this->focused_.assign(static_cast<size_t>(slot_count), -1);
	}

	UiWidget* FocusGroup::add(UiWidget* visual, Button::Action on_activate)
	{
		if (visual == nullptr)
		{
			throw std::invalid_argument("FocusGroup::add: null widget");
		}
		this->buttons_.emplace_back(visual, std::move(on_activate));

		if (this->buttons_.size() == 1)
		{
			std::fill(this->focused_.begin(), this->focused_.end(), 0);
		}
		this->refresh_style();
		return visual;
	}

	void FocusGroup::clear()
	{
		this->buttons_.clear();
		std::fill(this->focused_.begin(), this->focused_.end(), -1);
	}

	size_t FocusGroup::size() const
	{
		return this->buttons_.size();
	}

	int FocusGroup::slot_count() const
	{
		return static_cast<int>(this->focused_.size());
	}

	UiWidget* FocusGroup::focused(int slot) const
	{
		// Throws rather than clamping. A slot index is a pad number or a
		// viewport number, and a page that asks for one it did not reserve has
		// a bug the player would otherwise experience as a cursor that will
		// not move.
		const int index = this->focused_.at(static_cast<size_t>(slot));
		return index < 0 ? nullptr
			: this->buttons_[static_cast<size_t>(index)].visual();
	}

	bool FocusGroup::set_focused(int slot, const UiWidget* widget)
	{
		const int index = this->index_of(widget);
		if (index < 0)
		{
			return false;
		}
		this->focused_.at(static_cast<size_t>(slot)) = index;
		this->refresh_style();
		return true;
	}

	bool FocusGroup::move(int slot, Direction direction, bool wrap)
	{
		UiWidget* from = this->focused(slot);
		if (from == nullptr || direction == Direction::none)
		{
			return false;
		}

		// Disabled entries are not in the candidate list, so the walk lands on
		// the next live row rather than stopping at a dead one. `from` is
		// passed separately and may itself be disabled - which is the live
		// case, a row switched off under a cursor already sitting on it - and
		// moving off it still works.
		UiWidget* to = nearest_in_direction(*from, direction,
			this->enabled_visuals(), wrap);
		if (to == nullptr || to == from)
		{
			return false;
		}
		return this->set_focused(slot, to);
	}

	Activation FocusGroup::activate(int slot) const
	{
		const int index = this->focused_.at(static_cast<size_t>(slot));
		if (index < 0)
		{
			return Activation::none;
		}

		const Button& button = this->buttons_[static_cast<size_t>(index)];
		if (!button.enabled())
		{
			// Before the action rather than instead of it: a disabled entry
			// may well have one bound, and the press is being refused, not
			// found to be empty.
			return Activation::refused;
		}
		return button.activate() ? Activation::ran : Activation::none;
	}

	bool FocusGroup::set_enabled(const UiWidget* widget, bool enabled)
	{
		const int index = this->index_of(widget);
		if (index < 0)
		{
			return false;
		}
		this->buttons_[static_cast<size_t>(index)].set_enabled(enabled);

		// The paint is derived, so switching a row off is enough to recolour
		// it. A page doing this per frame - which the live case is - repaints
		// single digits of widgets on a menu, which is what the class comment
		// already priced.
		this->refresh_style();
		return true;
	}

	bool FocusGroup::enabled(const UiWidget* widget) const
	{
		const int index = this->index_of(widget);
		if (index < 0)
		{
			return false;
		}
		return this->buttons_[static_cast<size_t>(index)].enabled();
	}

	const FocusStyle& FocusGroup::style() const
	{
		return this->style_;
	}

	void FocusGroup::set_style(const FocusStyle& style)
	{
		this->style_ = style;
		this->refresh_style();
	}

	void FocusGroup::refresh_style() const
	{
		for (size_t i = 0; i < this->buttons_.size(); i++)
		{
			// Disabled outranks focused, and it has to. The cursor is left
			// where it was when a row is switched off under it, so "focused
			// and disabled" is a state a player reaches by standing still -
			// and painting that row focused would be the screen saying it is
			// available at the moment it refuses to be picked.
			if (!this->buttons_[i].enabled())
			{
				this->buttons_[i].visual()->set_colour(this->style_.disabled);
				continue;
			}

			const int index = static_cast<int>(i);
			const bool has_focus = std::find(this->focused_.begin(),
				this->focused_.end(), index) != this->focused_.end();

			this->buttons_[i].visual()->set_colour(has_focus
				? this->style_.focused
				: this->style_.unfocused);
		}
	}

	int FocusGroup::index_of(const UiWidget* widget) const
	{
		if (widget == nullptr)
		{
			return -1;
		}
		for (size_t i = 0; i < this->buttons_.size(); i++)
		{
			if (this->buttons_[i].visual() == widget)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	std::vector<UiWidget*> FocusGroup::enabled_visuals() const
	{
		std::vector<UiWidget*> result;
		result.reserve(this->buttons_.size());
		for (const Button& button : this->buttons_)
		{
			if (button.enabled())
			{
				result.push_back(button.visual());
			}
		}
		return result;
	}
}
