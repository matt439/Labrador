#include "engine/ui/focus.h"

#include "engine/ui/widget.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace mattmath;

namespace artattack
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

		UiWidget* to = nearest_in_direction(*from, direction, this->visuals(),
			wrap);
		if (to == nullptr || to == from)
		{
			return false;
		}
		return this->set_focused(slot, to);
	}

	bool FocusGroup::activate(int slot) const
	{
		const int index = this->focused_.at(static_cast<size_t>(slot));
		if (index < 0)
		{
			return false;
		}
		return this->buttons_[static_cast<size_t>(index)].activate();
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

	std::vector<UiWidget*> FocusGroup::visuals() const
	{
		std::vector<UiWidget*> result;
		result.reserve(this->buttons_.size());
		for (const Button& button : this->buttons_)
		{
			result.push_back(button.visual());
		}
		return result;
	}
}
