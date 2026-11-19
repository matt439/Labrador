#pragma once

#include <Windows.h>

#include <exception>
#include <stdexcept>

namespace render_tests
{
	// Changes one import pointer in this test executable for its lexical scope.
	// The operating system, DLL and other processes are never patched.
	template<class Function>
	class ImportHook
	{
	public:
		ImportHook(Function& entry, Function replacement)
			: entry_(&entry), previous_(entry)
		{
			this->replace(replacement);
		}

		~ImportHook()
		{
			try { this->replace(this->previous_); }
			catch (...) { std::terminate(); }
		}

		ImportHook(const ImportHook&) = delete;
		ImportHook& operator=(const ImportHook&) = delete;

	private:
		void replace(Function replacement)
		{
			DWORD protection;
			if (VirtualProtect(this->entry_, sizeof(Function), PAGE_READWRITE, &protection) == FALSE)
			{
				throw std::runtime_error("The test import pointer could not be made writable");
			}
			const Function before = *this->entry_;
			*this->entry_ = replacement;
			DWORD ignored;
			if (VirtualProtect(this->entry_, sizeof(Function), protection, &ignored) == FALSE)
			{
				*this->entry_ = before;
				throw std::runtime_error("The test import pointer's protection could not be restored");
			}
		}

		Function* entry_;
		Function previous_;
	};
}
