#include "engine/app/win32/system_command.h"

#include <Windows.h>

namespace labrador
{
	bool is_bare_menu_activation(uint64_t w_param, intptr_t l_param)
	{
		return (w_param & 0xFFF0u) == SC_KEYMENU && l_param == 0;
	}
}
