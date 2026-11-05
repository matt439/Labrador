#pragma once

#include "engine/render/throw_if_failed.h"

#include <windows.h>

namespace render_tests
{
	// An auto-reset Win32 event, unsignalled, closed with its owner. Throws
	// the creation failure's HRESULT.
	class Event
	{
	public:
		Event()
		{
			this->handle_ = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
			if (this->handle_ == nullptr)
			{
				labrador::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		~Event()
		{
			CloseHandle(this->handle_);
		}

		Event(const Event&) = delete;
		Event& operator=(const Event&) = delete;

		HANDLE handle() const
		{
			return this->handle_;
		}

	private:
		HANDLE handle_ = nullptr;
	};
}
