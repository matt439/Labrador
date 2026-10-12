#pragma once

#include "engine/ui/widget.h"

namespace components
{
	// A UiText whose words and colour can be read back.
	//
	// UiText draws both but keeps the getters protected, from TextObject and
	// DrawObject, so a component built from plain UiText cannot be asked what
	// it is showing - and its tests could only infer it from the measured box.
	// widget.h leaves every leaf open for one extra behaviour, and this is the
	// smallest one: two using-declarations.
	//
	// Every component here uses it, so copy this header with whichever one
	// you take. It is the one file they share.
	class Caption final : public labrador::UiText
	{
	public:
		using labrador::UiText::UiText;

		using labrador::TextObject::text;
		using labrador::DrawObject::colour;
	};
}
