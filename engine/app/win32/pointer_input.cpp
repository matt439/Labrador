#include "engine/app/win32/pointer_input.h"

#include <Windows.h>

namespace labrador
{
	namespace
	{
		// The signature Windows puts in the extra information of a mouse
		// message it synthesised from a pen or a touch, under this mask, and
		// the bit that says it was touch.
		constexpr uint32_t synthesised_mask = 0xFFFFFF00u;
		constexpr uint32_t synthesised_signature = 0xFF515700u;
		constexpr uint32_t from_touch_bit = 0x80u;
	}

	TouchMessage touch_message(unsigned int message, uint64_t w_param)
	{
		const WPARAM packed = static_cast<WPARAM>(w_param);
		switch (message)
		{
		case WM_POINTERDOWN:
			return TouchMessage::down;

		// A touch digitiser that reports hover sends updates for a finger
		// above the glass. Only one on it is a point.
		case WM_POINTERUPDATE:
			return IS_POINTER_INCONTACT_WPARAM(packed) ?
				TouchMessage::move : TouchMessage::none;

		case WM_POINTERUP:
			return IS_POINTER_CANCELED_WPARAM(packed) ?
				TouchMessage::cancel : TouchMessage::up;

		case WM_POINTERCAPTURECHANGED:
			return TouchMessage::cancel;

		default:
			return TouchMessage::none;
		}
	}

	bool mouse_message_is_from_touch(intptr_t extra_info)
	{
		const uint32_t low = static_cast<uint32_t>(
			static_cast<uint64_t>(extra_info) & 0xFFFFFFFFu);
		return (low & synthesised_mask) == synthesised_signature &&
			(low & from_touch_bit) != 0;
	}
}
