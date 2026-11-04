#include <doctest/doctest.h>

#include "engine/app/win32/pointer_input.h"

#include <Windows.h>

#include <cstdint>

TEST_CASE("a touch pointer's messages are the four touch calls")
{
	using labrador::TouchMessage;
	using labrador::touch_message;

	// The pointer's flags ride in the high word of WPARAM, its id in the low.
	const auto w = [](unsigned int flags)
	{
		return static_cast<uint64_t>(MAKEWPARAM(42, flags));
	};

	CHECK(touch_message(WM_POINTERDOWN,
		w(POINTER_MESSAGE_FLAG_INCONTACT)) == TouchMessage::down);
	CHECK(touch_message(WM_POINTERUPDATE,
		w(POINTER_MESSAGE_FLAG_INCONTACT)) == TouchMessage::move);
	CHECK(touch_message(WM_POINTERUP, w(0)) == TouchMessage::up);

	// A finger above a digitiser that reports hover is not a point.
	CHECK(touch_message(WM_POINTERUPDATE, w(POINTER_MESSAGE_FLAG_INRANGE)) ==
		TouchMessage::none);

	// Taken away rather than lifted.
	CHECK(touch_message(WM_POINTERUP, w(POINTER_MESSAGE_FLAG_CANCELED)) ==
		TouchMessage::cancel);
	CHECK(touch_message(WM_POINTERCAPTURECHANGED, w(0)) ==
		TouchMessage::cancel);

	CHECK(touch_message(WM_POINTERENTER, w(0)) == TouchMessage::none);
	CHECK(touch_message(WM_MOUSEMOVE, w(0)) == TouchMessage::none);
}

TEST_CASE("the touch signature is told from the pen's and from none")
{
	using labrador::mouse_message_is_from_touch;

	CHECK(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF515780u)));
	CHECK(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF5157FFu)));

	// A pen keeps its mouse messages: the signature without the touch bit.
	CHECK_FALSE(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF515700u)));
	CHECK_FALSE(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF51577Fu)));

	// A real mouse, and anything else that happens to set the bit.
	CHECK_FALSE(mouse_message_is_from_touch(0));
	CHECK_FALSE(mouse_message_is_from_touch(static_cast<intptr_t>(0x12345680u)));
}
