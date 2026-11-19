#pragma once

#include "engine/input/keyboard.h"

#include <cstdint>
#include <string>

namespace labrador
{
	// Internal SDK boundary for deterministic fixtures. Output buffers hold
	// UTF-16 units; counts exclude a terminator. No callback retains a pointer.
	struct NativeKeyLabelApi
	{
		void* context = nullptr;
		unsigned int (*map_virtual_key)(void* context, unsigned int scan,
			unsigned int map_type, std::uintptr_t layout) = nullptr;
		int (*to_unicode)(void* context, unsigned int virtual_key,
			unsigned int scan, const unsigned char* state, char16_t* output,
			int capacity, unsigned int flags, std::uintptr_t layout) = nullptr;
		int (*key_name)(void* context, std::int32_t message_bits,
			char16_t* output, int capacity) = nullptr;
	};

	// Best-effort UTF-8 label, or empty when unavailable. The caller supplies
	// its current thread's active HKL as an integer: GetKeyNameText uses that
	// thread's layout for named controls. The returned string owns its storage.
	// Character queries use a neutral state and ToUnicodeEx's no-state-change
	// flag. A dead key, ambiguous output or malformed text is unavailable;
	// no state-clearing query is made. A pending platform dead-key buffer can
	// still affect a translation, so this is not a promise of a keycap legend.
	std::string key_label_from_layout(Key key, std::uintptr_t layout);
	std::string key_label_from_layout(Key key, std::uintptr_t layout,
		const NativeKeyLabelApi& api);
}
