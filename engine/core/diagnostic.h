#pragma once

#include <string_view>

namespace labrador
{
	// Receives one non-fatal engine message synchronously on its reporting
	// thread. The message is borrowed until the call returns and may include
	// a trailing newline. Copy it if it must be kept. The context is borrowed
	// from the caller that installed the callback.
	//
	// A callback must not throw, replace the callback, or recursively report
	// diagnostics. Calls from different threads may overlap; the callback
	// must synchronise its own shared state and must not call engine objects.
	using DiagnosticCallback = void (*)(std::string_view message,
		void* context) noexcept;

	// Replaces the one process-wide callback. nullptr restores stderr and
	// ignores context. Set it before creating engine objects, or while no
	// engine/diagnostic calls are in flight, including callbacks from drivers.
	// Registration is not concurrent with reporting or another registration.
	//
	// Keep the callback code and context alive until engine objects and their
	// workers/driver callbacks have stopped, then reset before destroying the
	// context. Browser frame callbacks outlive main, so their context must too.
	// No callback state is owned, allocated or destroyed by this function.
	void set_diagnostic_callback(DiagnosticCallback callback,
		void* context = nullptr) noexcept;

	// Reports a non-fatal message through the current callback, or writes its
	// exact bytes to stderr, adding a newline if one is absent. The engine
	// retains no message or callback storage and performs no deferred call.
	// There is no ordering guarantee between threads; stderr write failures
	// are ignored.
	// Fatal exceptions and browser Module.onError reporting are separate.
	void report_diagnostic(std::string_view message) noexcept;
}
