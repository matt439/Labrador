#pragma once

namespace audio
{
	// Opens the packaged bank without a graphics device, plays both one-shots,
	// and checks the persistent voice's lifecycle. Throws on any failed step.
	void smoke_test();
}
