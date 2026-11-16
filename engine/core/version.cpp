#include "engine/core/version.h"
#include "engine/core/version_values.h"

namespace labrador
{
	Version version()
	{
		return Version{detail::version_major, detail::version_minor,
			detail::version_patch, detail::version_string,
			detail::version_is_development};
	}
}
