#pragma once

#include "tests/render/dxgi_factory_fixture.h"

#include <Windows.h>

#include <bit>
#include <stdexcept>

namespace render_tests::direct3d_startup
{
	struct Probe
	{
		HRESULT creation_failure = S_OK;
		HRESULT fallback_failure = S_OK;
		HRESULT capability_failure = S_OK;
		bool capability_success = false;
		int creation_attempts = 0;
		int capability_attempts = 0;
	};

	inline thread_local Probe* active = nullptr;

	class Scope
	{
	public:
		explicit Scope(Probe& probe) : previous_(active) { active = &probe; }
		~Scope() { active = previous_; }
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
	private:
		Probe* previous_;
	};

	template<class Function>
	Function system_function(const wchar_t* library, const char* name)
	{
		const HMODULE module = LoadLibraryExW(library, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
		const FARPROC address = module == nullptr ? nullptr : GetProcAddress(module, name);
		if (address == nullptr)
		{
			throw std::runtime_error("The Direct3D startup fixture could not load a system entry point");
		}
		return std::bit_cast<Function>(address);
	}

	inline HRESULT WINAPI create_factory1(REFIID iid, void** factory)
	{
		static const decltype(&CreateDXGIFactory1) real =
			system_function<decltype(&CreateDXGIFactory1)>(L"dxgi.dll", "CreateDXGIFactory1");
		const HRESULT result = real(iid, factory);
		return SUCCEEDED(result) ? dxgi_factory::wrap_factory(iid, factory) : result;
	}

	inline HRESULT WINAPI create_factory2(UINT flags, REFIID iid, void** factory)
	{
		static const decltype(&CreateDXGIFactory2) real =
			system_function<decltype(&CreateDXGIFactory2)>(L"dxgi.dll", "CreateDXGIFactory2");
		const HRESULT result = real(flags, iid, factory);
		return SUCCEEDED(result) ? dxgi_factory::wrap_factory(iid, factory) : result;
	}
}
