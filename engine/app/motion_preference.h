#pragma once

namespace labrador
{
	// Whether the platform currently asks for reduced motion: disabled client-area
	// animations on Windows, or prefers-reduced-motion: reduce in a browser.
	// Returns false when the setting cannot be read or the browser has no query API.
	//
	// Read on each call, with no cached value or listeners. The result is a snapshot
	// until the caller asks again, including before a window is opened or after it
	// closes. Call on the application thread. This changes no effects or options;
	// the game chooses when to refresh and whether an explicit player choice wins.
	bool prefers_reduced_motion();
}
