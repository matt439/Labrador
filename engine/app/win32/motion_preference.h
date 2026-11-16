#pragma once

#include <Windows.h>

namespace labrador
{
	// The platform call is supplied for a deterministic test of the real request
	// and its output handling, without changing the user's animation setting.
	using SystemParametersQuery = BOOL (WINAPI *)(UINT, UINT, PVOID, UINT);
	bool query_reduced_motion(SystemParametersQuery query);
}
