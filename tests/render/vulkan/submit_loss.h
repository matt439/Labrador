#pragma once

// RenderPixelTests has one vkQueueSubmit definition, in
// pending_submission_tests.cpp. A case that needs a submit to report device
// loss arms this count on its own thread rather than defining a second one.
//
// While it is positive, each vkQueueSubmit on this thread is not forwarded:
// the count is decremented and VK_ERROR_DEVICE_LOST is answered. The device
// itself stays live, so this exercises the engine's reported-loss recovery,
// not a genuine device removal.
namespace render_tests
{
	inline thread_local int reported_submit_losses = 0;
}
