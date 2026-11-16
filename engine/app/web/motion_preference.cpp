#include "engine/app/motion_preference.h"

#include <emscripten/em_js.h>

EM_JS(int, labrador_prefers_reduced_motion, (), {
	if (typeof matchMedia !== "function") {
		return 0;
	}
	try {
		return matchMedia("(prefers-reduced-motion: reduce)").matches === true ? 1 : 0;
	} catch (error) {
		return 0;
	}
});

namespace labrador
{
	bool prefers_reduced_motion()
	{
		return labrador_prefers_reduced_motion() != 0;
	}
}
