#include "engine/ui/button.h"

#include "engine/ui/widget.h"

namespace artattack
{
	Button::Button(UiWidget* visual, Action on_activate) :
		visual_(visual), on_activate_(std::move(on_activate))
	{
	}

	UiWidget* Button::visual() const
	{
		return this->visual_;
	}

	bool Button::has_action() const
	{
		return static_cast<bool>(this->on_activate_);
	}

	bool Button::activate() const
	{
		if (!this->on_activate_)
		{
			return false;
		}
		this->on_activate_();
		return true;
	}

	bool Button::enabled() const
	{
		return this->enabled_;
	}

	void Button::set_enabled(bool enabled)
	{
		this->enabled_ = enabled;
	}
}
