#include <doctest/doctest.h>

#include "tests/allocation_scope.h"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

TEST_CASE("ASan allocation probes count C and C++ requests and preserve refused vectors")
{
	labrador_tests::require_allocation_scope_counts();
	std::vector<int> values;
	labrador_tests::AllocationSample sample;
	sample.refused_attempt = 1;
	bool refused = false;
	{
		labrador_tests::AllocationScope scope(sample);
		try
		{
			values.resize(91);
		}
		catch (const std::bad_alloc&)
		{
			refused = true;
		}
	}
	REQUIRE(refused);
	CHECK(sample.refused == 1);
	CHECK_FALSE(sample.hook_conflict);
	CHECK(values.empty());
	CHECK(labrador_tests::asan_allocation_hooks() == labrador_tests::AsanAllocationHooks{});

	// A post-allocation callback cannot inject C malloc failure. The probe
	// must refuse to trust such a requested failpoint instead of passing it.
	labrador_tests::AllocationSample raw;
	raw.refused_attempt = 1;
	void* allocated = nullptr;
	{
		labrador_tests::AllocationScope scope(raw);
		allocated = std::malloc(71);
	}
	REQUIRE(allocated != nullptr);
	std::free(allocated);
	CHECK(raw.hook_conflict);
	CHECK(labrador_tests::asan_allocation_hooks() == labrador_tests::AsanAllocationHooks{});
}

TEST_CASE("ASan test allocation replacement supports aligned arrays and nothrow forms")
{
	struct alignas(64) Aligned
	{
		int values[16] = { 17 };
	};
	const std::unique_ptr<Aligned> aligned = std::make_unique<Aligned>();
	const std::unique_ptr<Aligned[]> array(new (std::nothrow) Aligned[3]);
	REQUIRE(array != nullptr);
	CHECK(aligned->values[0] == 17);
	CHECK(array[2].values[0] == 17);
}
