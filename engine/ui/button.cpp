#include "engine/ui/button.h"

#include "engine/ui/widget.h"

namespace labrador
{
	Button::Button(UiWidget* visual, Action on_activate) :
		visual_(visual), on_activate_(on_activate
			? std::make_shared<Action>(std::move(on_activate)) : nullptr)
	{
	}

	Button::Button(const Button& other) :
		visual_(other.visual_), on_activate_(other.on_activate_
			? std::make_shared<Action>(*other.on_activate_) : nullptr),
		enabled_(other.enabled_)
	{
	}

	Button& Button::operator=(const Button& other)
	{
		// Copies retain Button's value semantics; only an invocation shares
		// ownership with the button whose action is running.
		Button copy(other);
		*this = std::move(copy);
		return *this;
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
		const std::shared_ptr<Action> action = this->on_activate_;
		if (!action)
		{
			return false;
		}
		(*action)();
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
