#include "samples/components/binding_row.h"

#include <cstddef>

using namespace labrador;

namespace components
{
	namespace
	{
		constexpr Colour value_colour(235, 190, 90);

		bool any_held(const KeyboardState& state)
		{
			for (unsigned int index = 1; index < static_cast<unsigned int>(Key::count); ++index)
			{
				if (state.is_down(static_cast<Key>(index)))
				{
					return true;
				}
			}
			return false;
		}
	}

	BindingRow::BindingRow(const std::string& name, const std::wstring& label,
		const std::wstring& value, const Vector2F& position, float value_x,
		const std::string& font_name, RenderResources* resources) :
		UiContainer(name),
		label_(name + "_label", label, font_name, position, resources),
		value_(name + "_value", value, font_name, Vector2F(value_x, position.y), resources, value_colour),
		normal_value_(value)
	{
		this->add_child(&this->label_);
		this->add_child(&this->value_);
	}

	void BindingRow::set_value(const std::wstring& value)
	{
		this->normal_value_ = value;
		if (!this->capturing_ && this->value_.text() != value)
		{
			this->value_.set_text(value);
		}
	}

	void BindingRow::begin_capture()
	{
		if (this->capturing_)
		{
			return;
		}
		this->capturing_ = true;
		this->waiting_ = true;
		this->value_.set_text(L"Release held keys");
	}

	void BindingRow::cancel_capture()
	{
		this->capturing_ = false;
		this->waiting_ = false;
		if (this->value_.text() != this->normal_value_)
		{
			this->value_.set_text(this->normal_value_);
		}
	}

	bool BindingRow::capturing() const { return this->capturing_; }
	bool BindingRow::waiting_for_release() const { return this->waiting_; }
	const UiText& BindingRow::label() const { return this->label_; }
	const UiText& BindingRow::value() const { return this->value_; }
	void BindingRow::set_colour(const Colour& colour) { this->label_.set_colour(colour); }

	CaptureStep BindingRow::update_capture(const KeyboardState& now, const KeyboardState& before)
	{
		if (!this->capturing_)
		{
			return {};
		}
		if (!now.focused)
		{
			this->cancel_capture();
			return { CaptureKind::focus_lost, Key::none };
		}
		if (this->waiting_)
		{
			if (!any_held(now))
			{
				this->waiting_ = false;
				this->value_.set_text(L"Press one key");
			}
			return {};
		}

		Key candidate = Key::none;
		std::size_t count = 0;
		for (unsigned int index = 1; index < static_cast<unsigned int>(Key::count); ++index)
		{
			const Key key = static_cast<Key>(index);
			if (labrador::pressed(now, before, key))
			{
				candidate = key;
				++count;
			}
		}
		if (count == 0)
		{
			return {};
		}
		this->waiting_ = true;
		this->value_.set_text(L"Release held keys");
		return count == 1 ? CaptureStep{ CaptureKind::key, candidate }
			: CaptureStep{ CaptureKind::multiple, Key::none };
	}
}
