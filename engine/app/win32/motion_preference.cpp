#include "engine/app/motion_preference.h"

#include "engine/app/win32/motion_preference.h"

namespace labrador
{
	bool query_reduced_motion(SystemParametersQuery query)
	{
		BOOL enabled = TRUE;
		const BOOL succeeded = query(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
		return succeeded != FALSE && enabled == FALSE;
	}

	bool prefers_reduced_motion()
	{
		return query_reduced_motion(SystemParametersInfoW);
	}
}
