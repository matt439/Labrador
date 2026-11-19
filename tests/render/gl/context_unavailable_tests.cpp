#include <doctest/doctest.h>

#include "engine/render/device_unavailable.h"
#include "engine/render/gl/gl_functions.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "tests/render/test_window.h"
#include "tests/render/win32/import_hook.h"

#include <Windows.h>

#include <bit>
#include <cstring>
#include <stdexcept>
#include <string>

// Address the existing imports; each probe temporarily redirects only this
// executable's slots and restores their pointers and protections on return.
extern "C"
{
	extern decltype(&wglGetProcAddress) __imp_wglGetProcAddress;
	extern decltype(&DescribePixelFormat) __imp_DescribePixelFormat;
	extern decltype(&SetPixelFormat) __imp_SetPixelFormat;
	extern decltype(&wglMakeCurrent) __imp_wglMakeCurrent;
}

namespace
{
	enum class Failure
	{
		create_extension,
		version,
		profile,
		resource_exhaustion,
		invalid_context_state,
		unreported_creation_failure,
		gl_entry_point,
		swap_extension,
		swap_entry_point,
		pixel_channels,
		set_pixel_format,
		make_current
	};

	struct Probe
	{
		Failure failure;
		int injections = 0;
		int current_requests = 0;
	};

	thread_local Probe* active_probe = nullptr;

	template <typename Function>
	Function native_function(const wchar_t* library, const char* name)
	{
		const FARPROC address = GetProcAddress(GetModuleHandleW(library), name);
		if (address == nullptr)
		{
			throw std::runtime_error(std::string("The WGL fixture cannot load ") + name);
		}
		return std::bit_cast<Function>(address);
	}

	using GetWglProc = PROC(WINAPI*)(LPCSTR);
	using DescribeFormat = int(WINAPI*)(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
	using SetFormat = BOOL(WINAPI*)(HDC, int, const PIXELFORMATDESCRIPTOR*);
	using MakeCurrent = BOOL(WINAPI*)(HDC, HGLRC);

	HGLRC WINAPI refuse_core_context(HDC, HGLRC, const int*)
	{
		Probe& probe = *active_probe;
		++probe.injections;
		if (probe.failure == Failure::unreported_creation_failure)
			return nullptr;
		SetLastError(probe.failure == Failure::version ? 0x2095 :
			probe.failure == Failure::profile ? 0x2096 :
			probe.failure == Failure::resource_exhaustion ? ERROR_NOT_ENOUGH_MEMORY :
			ERROR_INVALID_HANDLE);
		return nullptr;
	}

	const char* WINAPI no_extensions_arb(HDC)
	{
		++active_probe->injections;
		return "";
	}

	const char* WINAPI no_extensions_ext()
	{
		++active_probe->injections;
		return "";
	}

	PROC WINAPI observed_get_proc(LPCSTR name)
	{
		Probe* probe = active_probe;
		if (probe != nullptr)
		{
			if (std::strcmp(name, "wglCreateContextAttribsARB") == 0)
			{
				if (probe->failure == Failure::create_extension)
				{
					++probe->injections;
					return nullptr;
				}
				if (probe->failure == Failure::version || probe->failure == Failure::profile ||
					probe->failure == Failure::resource_exhaustion ||
					probe->failure == Failure::invalid_context_state ||
					probe->failure == Failure::unreported_creation_failure)
				{
					// An unrelated earlier API's error must not classify a later
					// failure that provides no native reason of its own.
					SetLastError(0x2095);
					return std::bit_cast<PROC>(&refuse_core_context);
				}
			}
			if ((probe->failure == Failure::gl_entry_point &&
				std::strcmp(name, "glCreateShader") == 0) ||
				(probe->failure == Failure::swap_entry_point &&
				std::strcmp(name, "wglSwapIntervalEXT") == 0))
			{
				++probe->injections;
				return nullptr;
			}
			if (probe->failure == Failure::swap_extension)
			{
				if (std::strcmp(name, "wglGetExtensionsStringARB") == 0)
					return std::bit_cast<PROC>(&no_extensions_arb);
				if (std::strcmp(name, "wglGetExtensionsStringEXT") == 0)
					return std::bit_cast<PROC>(&no_extensions_ext);
			}
		}
		static const GetWglProc real = native_function<GetWglProc>(L"opengl32.dll", "wglGetProcAddress");
		return real(name);
	}

	int WINAPI observed_describe_format(HDC device, int format, UINT size,
		LPPIXELFORMATDESCRIPTOR description)
	{
		static const DescribeFormat real = native_function<DescribeFormat>(L"gdi32.dll", "DescribePixelFormat");
		const int result = real(device, format, size, description);
		if (result != 0 && active_probe != nullptr &&
			active_probe->failure == Failure::pixel_channels)
		{
			++active_probe->injections;
			description->cAlphaBits = 0;
		}
		return result;
	}

	BOOL WINAPI observed_set_format(HDC device, int format, const PIXELFORMATDESCRIPTOR* description)
	{
		if (active_probe != nullptr && active_probe->failure == Failure::set_pixel_format)
		{
			++active_probe->injections;
			SetLastError(ERROR_INVALID_PIXEL_FORMAT);
			return FALSE;
		}
		static const SetFormat real = native_function<SetFormat>(L"gdi32.dll", "SetPixelFormat");
		return real(device, format, description);
	}

	BOOL WINAPI observed_make_current(HDC device, HGLRC context)
	{
		if (active_probe != nullptr && context != nullptr &&
			++active_probe->current_requests == 2 && active_probe->failure == Failure::make_current)
		{
			++active_probe->injections;
			SetLastError(ERROR_INVALID_HANDLE);
			return FALSE;
		}
		static const MakeCurrent real = native_function<MakeCurrent>(L"opengl32.dll", "wglMakeCurrent");
		return real(device, context);
	}

	class ProbeScope
	{
	public:
		explicit ProbeScope(Probe& probe)
		{
			if (active_probe != nullptr)
				throw std::logic_error("WGL startup probes cannot nest");
			active_probe = &probe;
		}
		~ProbeScope() { active_probe = nullptr; }
		ProbeScope(const ProbeScope&) = delete;
		ProbeScope& operator=(const ProbeScope&) = delete;

	private:
		render_tests::ImportHook<GetWglProc> proc_{ __imp_wglGetProcAddress, observed_get_proc };
		render_tests::ImportHook<DescribeFormat> describe_{ __imp_DescribePixelFormat, observed_describe_format };
		render_tests::ImportHook<SetFormat> set_{ __imp_SetPixelFormat, observed_set_format };
		render_tests::ImportHook<MakeCurrent> current_{ __imp_wglMakeCurrent, observed_make_current };
	};
}

TEST_CASE("WGL startup capability refusals have the renderer-unavailable category")
{
	struct Case
	{
		Failure failure;
		const char* detail;
	};
	for (const Case& test : {
		Case{ Failure::create_extension, "WGL_ARB_create_context" },
		Case{ Failure::version, "Windows error 8341" },
		Case{ Failure::profile, "Windows error 8342" },
		Case{ Failure::gl_entry_point, "glCreateShader" },
		Case{ Failure::swap_extension, "WGL_EXT_swap_control" },
		Case{ Failure::swap_entry_point, "lacks its setter or getter" },
		Case{ Failure::pixel_channels, "a0" } })
	{
		render_tests::TestWindow window(64, 64);
		labrador::RenderResources resources;
		labrador::Renderer renderer;
		Probe probe{ test.failure };
		const ProbeScope scope(probe);
		try
		{
			renderer.create_device(window.handle(), 64, 64, 1);
			FAIL("the injected WGL capability failure was accepted");
		}
		catch (const labrador::RenderDeviceUnavailable& error)
		{
			CHECK(std::string(error.what()).find(test.detail) != std::string::npos);
		}
		REQUIRE(probe.injections > 0);
	}
}

TEST_CASE("WGL startup state and unrelated native failures remain ordinary errors")
{
	for (const Failure failure : { Failure::resource_exhaustion,
		Failure::invalid_context_state, Failure::unreported_creation_failure,
		Failure::set_pixel_format, Failure::make_current })
	{
		render_tests::TestWindow window(64, 64);
		labrador::RenderResources resources;
		labrador::Renderer renderer;
		Probe probe{ failure };
		const ProbeScope scope(probe);
		try
		{
			renderer.create_device(window.handle(), 64, 64, 1);
			FAIL("the injected native failure was accepted");
		}
		catch (const labrador::RenderDeviceUnavailable&)
		{
			FAIL("caller state or an unrelated native error became device absence");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find("Windows error") != std::string::npos);
		}
		REQUIRE(probe.injections > 0);
	}
	labrador::Renderer renderer;
	CHECK_THROWS_AS(renderer.create_device(nullptr, 64, 64, 1), std::invalid_argument);
}

TEST_CASE("missing WGL swap control during later reconfiguration stays an ordinary error")
{
	render_tests::TestWindow window(64, 64);
	labrador::RenderResources resources;
	labrador::Renderer renderer;
	renderer.create_device(window.handle(), 64, 64, 1);
	Probe probe{ Failure::swap_extension };
	const ProbeScope scope(probe);
	try
	{
		labrador::configure_swap_interval(wglGetCurrentDC(), 1);
		FAIL("the injected runtime capability failure was accepted");
	}
	catch (const labrador::RenderDeviceUnavailable&)
	{
		FAIL("runtime presentation reconfiguration became startup absence");
	}
	catch (const std::runtime_error& error)
	{
		CHECK(std::string(error.what()).find("WGL_EXT_swap_control") != std::string::npos);
	}
	CHECK(probe.injections > 0);
}
