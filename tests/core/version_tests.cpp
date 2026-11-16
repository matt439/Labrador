#include <doctest/doctest.h>

#include "engine/core/version.h"

#include <ostream>

TEST_CASE("the library version agrees with the consumer-visible CMake facts")
{
	const labrador::Version current = labrador::version();
	CHECK(current.major == LABRADOR_EXPECTED_VERSION_MAJOR);
	CHECK(current.minor == LABRADOR_EXPECTED_VERSION_MINOR);
	CHECK(current.patch == LABRADOR_EXPECTED_VERSION_PATCH);
	CHECK(current.string == LABRADOR_EXPECTED_VERSION_STRING);
	CHECK(current.development == (LABRADOR_EXPECTED_VERSION_IS_DEVELOPMENT != 0));
	const labrador::Version again = labrador::version();
	CHECK(again.string.data() == current.string.data());
	CHECK(again.string == current.string);
}
