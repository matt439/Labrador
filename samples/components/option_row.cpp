#include "samples/components/option_row.h"

#include <stdexcept>
#include <utility>

using namespace labrador;
using namespace mattmath;

namespace components
{
	namespace
	{
		// Not white, and not either of FocusStyle's colours, so the value
		// never reads as the thing the cursor is on. Change it to your game's.
		constexpr Colour value_colour(235, 190, 90);

		// The entry the row starts on, after checking there is one. It is
		// called from the initialiser list because the value's text is that
		// entry, so the check has to come before the member is built.
		const std::wstring& starting_choice(const std::string& name,
			const std::vector<std::wstring>& choices, size_t index)
		{
			if (choices.empty() || index >= choices.size())
			{
				throw std::invalid_argument("OptionRow '" + name +
					"' needs a non-empty choice list and an index inside it");
			}

			return choices[index];
		}
	}

	OptionRow::OptionRow(const std::string& name,
		const std::wstring& label,
		std::vector<std::wstring> choices,
		size_t index,
		const Vector2F& label_position,
		float value_x,
		const std::string& font_name,
		RenderResources* render_resources,
		Changed on_change,
		bool wrap) :
		UiContainer(name),
		label_(name + "_label", label, font_name, label_position,
			render_resources),
		value_(name + "_value", starting_choice(name, choices, index),
			font_name, Vector2F(value_x, label_position.y), render_resources,
			value_colour),
		choices_(std::move(choices)),
		index_(index),
		on_change_(std::move(on_change)),
		wrap_(wrap)
	{
		this->add_child(&this->label_);
		this->add_child(&this->value_);
	}

	bool OptionRow::adjust(Direction direction)
	{
		const size_t last = this->choices_.size() - 1;
		size_t next = this->index_;

		if (direction == Direction::right)
		{
			if (this->index_ < last)
			{
				next = this->index_ + 1;
			}
			else if (this->wrap_)
			{
				next = 0;
			}
		}
		else if (direction == Direction::left)
		{
			if (this->index_ > 0)
			{
				next = this->index_ - 1;
			}
			else if (this->wrap_)
			{
				next = last;
			}
		}

		if (next == this->index_)
		{
			return false;
		}

		this->index_ = next;
		this->value_.set_text(this->choices_[next]);

		if (this->on_change_)
		{
			this->on_change_(next);
		}

		return true;
	}

	size_t OptionRow::index() const
	{
		return this->index_;
	}

	const std::wstring& OptionRow::choice() const
	{
		return this->choices_[this->index_];
	}

	size_t OptionRow::choice_count() const
	{
		return this->choices_.size();
	}

	void OptionRow::set_index(size_t index)
	{
		if (index >= this->choices_.size())
		{
			throw std::invalid_argument("OptionRow '" + this->name() +
				"': index out of range");
		}

		this->index_ = index;
		this->value_.set_text(this->choices_[index]);
	}

	const UiText& OptionRow::label() const
	{
		return this->label_;
	}

	const UiText& OptionRow::value() const
	{
		return this->value_;
	}

	void OptionRow::set_colour(const Colour& colour)
	{
		this->label_.set_colour(colour);
	}
}
