#pragma once

#include <windows.h>

#include <bit>
#include <stdexcept>
#include <string>

namespace render_tests
{
	// The Vulkan loader's own export called `name`, fetched from vulkan-1.dll
	// rather than linked, so that a test executable defining the same entry
	// point to observe or fail it can still forward to the real one. Throws
	// naming the export when the loader is not loaded or lacks it.
	template<typename Function>
	Function loader_function(const char* name)
	{
		const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
		const FARPROC address = module == nullptr ? nullptr : GetProcAddress(module, name);
		if (address == nullptr)
		{
			throw std::runtime_error(std::string("Vulkan loader export missing: ") + name);
		}
		static_assert(sizeof(Function) == sizeof(FARPROC));
		return std::bit_cast<Function>(address);
	}
}
