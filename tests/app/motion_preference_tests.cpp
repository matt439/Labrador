#include <doctest/doctest.h>

#include "engine/app/motion_preference.h"
#if defined(_WIN32)
#include "engine/app/win32/motion_preference.h"

namespace
{
	BOOL animation_enabled = TRUE;
	BOOL query_succeeded = TRUE;
	int queries = 0;

	BOOL WINAPI system_parameters(UINT action, UINT parameter, PVOID output, UINT flags)
	{
		CHECK(action == SPI_GETCLIENTAREAANIMATION);
		CHECK(parameter == 0);
		CHECK(flags == 0);
		REQUIRE(output != nullptr);
		*static_cast<BOOL*>(output) = animation_enabled;
		++queries;
		return query_succeeded;
	}
}

TEST_CASE("reduced motion samples the client-area animation setting afresh")
{
	query_succeeded = TRUE;
	queries = 0;
	animation_enabled = TRUE;
	CHECK_FALSE(labrador::query_reduced_motion(system_parameters));
	animation_enabled = FALSE;
	CHECK(labrador::query_reduced_motion(system_parameters));
	animation_enabled = 2;
	CHECK_FALSE(labrador::query_reduced_motion(system_parameters));
	CHECK(queries == 3);
}

TEST_CASE("an unavailable animation setting does not ask for reduced motion")
{
	query_succeeded = FALSE;
	animation_enabled = FALSE;
	CHECK_FALSE(labrador::query_reduced_motion(system_parameters));
	animation_enabled = TRUE;
	CHECK_FALSE(labrador::query_reduced_motion(system_parameters));
}

TEST_CASE("the public reduced-motion query reads the native setting without a window")
{
	BOOL enabled = TRUE;
	const BOOL succeeded = SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
	CHECK(labrador::prefers_reduced_motion() == (succeeded != FALSE && enabled == FALSE));
}
#else
TEST_CASE("a host without matchMedia has no reduced-motion request")
{
	CHECK_FALSE(labrador::prefers_reduced_motion());
}
#endif
