#pragma once

#include <optional>
#include <string>

namespace labrador
{
	// A window's configured display metadata, separate from its render device
	// and from measured frame/presentation timing. Unknown fields stay empty.
	struct WindowDisplayInfo
	{
		// UTF-8 logical display identifier for this session, or empty when
		// unavailable. It is not a persistent physical-panel identifier.
		std::string monitor_id;

		// Nominal configured whole Hz, greater than one, when the platform
		// reports it. Windows' integer mode value does not identify fractional
		// refresh, VRR cadence, achieved FPS or physical scan-out timing.
		// Unavailable/default/invalid platform values remain std::nullopt.
		std::optional<int> nominal_refresh_hz;
	};
}
