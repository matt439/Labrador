#include "engine/core/diagnostic.h"

#include <cstdio>

namespace labrador
{
	namespace
	{
		// Trivial constant-initialised state remains usable during teardown;
		// no global callable or mutex is destroyed before a backend reports.
		constinit DiagnosticCallback diagnostic_callback = nullptr;
		constinit void* diagnostic_context = nullptr;
	}

	void set_diagnostic_callback(DiagnosticCallback callback,
		void* context) noexcept
	{
		diagnostic_callback = callback;
		diagnostic_context = callback == nullptr ? nullptr : context;
	}

	void report_diagnostic(std::string_view message) noexcept
	{
		if (diagnostic_callback != nullptr)
		{
			diagnostic_callback(message, diagnostic_context);
			return;
		}

		if (!message.empty())
		{
			std::fwrite(message.data(), 1, message.size(), stderr);
		}
		if (message.empty() || message.back() != '\n')
		{
			std::fputc('\n', stderr);
		}
	}
}
