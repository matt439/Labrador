#pragma once

#include "engine/input/keyboard.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/ui/widget.h"

#include <string>

namespace components
{
	enum class CaptureKind
	{
		none, key, multiple, focus_lost,
	};

	struct CaptureStep
	{
		CaptureKind kind = CaptureKind::none;
		labrador::Key key = labrador::Key::none;
	};

	// Copyable capture row. The page supplies snapshots and owns action names,
	// allowed/protected keys, duplicate policy, cancellation and persistence.
	// No device is polled here and no character is inferred from typed text.
	class BindingRow final : public labrador::UiContainer
	{
	public:
		BindingRow(const std::string& name, const std::wstring& label,
			const std::wstring& value, const labrador::Vector2F& position,
			float value_x, const std::string& font_name,
			labrador::RenderResources* resources);

		BindingRow(const BindingRow&) = delete;
		BindingRow& operator=(const BindingRow&) = delete;

		void set_value(const std::wstring& value);
		void begin_capture();
		void cancel_capture();
		bool capturing() const;
		bool waiting_for_release() const;

		// Every capture and candidate waits for a fully released snapshot.
		// This ignores the opener and keys held before capture. A multi-key
		// edge is refused as ambiguous; neither ordering nor OS repeat wins.
		// A candidate leaves capture active so the page may accept or refuse it.
		CaptureStep update_capture(const labrador::KeyboardState& now,
			const labrador::KeyboardState& before);

		const labrador::UiText& label() const;
		const labrador::UiText& value() const;
		void set_colour(const labrador::Colour& colour) override;

	private:
		labrador::UiText label_;
		labrador::UiText value_;
		std::wstring normal_value_;
		bool capturing_ = false;
		bool waiting_ = false;
	};
}
