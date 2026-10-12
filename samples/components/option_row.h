#pragma once

#include "engine/input/direction.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/ui/widget.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace components
{
	// One settings row: a label, and a value the player steps through a fixed
	// list of with left and right - a difficulty, a window mode, a colour
	// palette.
	//
	// COPY IT, DO NOT LINK IT. This file and option_row.cpp are meant to be
	// dropped into a game and edited there; the README beside them says why
	// components are copied rather than shipped as engine code.
	//
	// A CONTAINER, SO THE CURSOR LANDS ON THE ROW. FocusGroup::add takes a
	// UiWidget, and UiContainer is one, so the row is a single focus target
	// whose bounds are the label and the value together (widget.h). Focus
	// paints the label only: the value keeps its own colour, so the player
	// can tell what is selected from what is being pointed at.
	//
	// THE ROW DOES NOT READ INPUT. Which keys mean left is a binding and is
	// the page's; the page finds the focused row and calls adjust(). That
	// keeps the row testable with no devices at all, which option_row_tests
	// does.
	class OptionRow final : public labrador::UiContainer
	{
	public:
		// Called with the new index whenever adjust() moves it, and never
		// when set_index() does: the page that sets a value already knows it.
		using Changed = std::function<void(size_t index)>;

		// `choices` must not be empty and `index` must be inside it; either
		// mistake throws std::invalid_argument naming the row (T6). `wrap`
		// decides what right does on the last choice - back to the first for
		// an unordered list such as a window mode, nothing for an ordered one
		// such as a difficulty, where wrapping from Hard to Easy surprises.
		OptionRow(const std::string& name,
			const std::wstring& label,
			std::vector<std::wstring> choices,
			size_t index,
			const mattmath::Vector2F& label_position,
			float value_x,
			const std::string& font_name,
			labrador::RenderResources* render_resources,
			Changed on_change = nullptr,
			bool wrap = true);

		// The children are members and the container holds pointers to them,
		// so a copy or a move would leave the new row drawing the old one's.
		OptionRow(const OptionRow&) = delete;
		OptionRow& operator=(const OptionRow&) = delete;

		// Steps the value. Left and right move it; up, down and none are not
		// this row's and are ignored. Returns whether the index changed, which
		// is the signal a page plays a sound on - false at either end of a row
		// that does not wrap.
		bool adjust(labrador::Direction direction);

		size_t index() const;
		const std::wstring& choice() const;
		size_t choice_count() const;

		// Throws std::invalid_argument for an index outside the choices.
		void set_index(size_t index);

		// The two parts, for a page that lines other things up against them.
		const labrador::UiText& label() const;
		const labrador::UiText& value() const;

		void set_colour(const labrador::Colour& colour) override;

	private:
		// In this order on purpose: value_ is built from the constructor's
		// `choices` before choices_ moves out of it.
		labrador::UiText label_;
		labrador::UiText value_;
		std::vector<std::wstring> choices_;
		size_t index_ = 0;
		Changed on_change_;
		bool wrap_ = true;
	};
}
