#include <doctest/doctest.h>
#include "engine/core/registry.h"
#include <memory>
#include <string>

namespace
{
	// Something with identity, so a test can tell one instance from another
	// after a slot has been refilled.
	struct Resource
	{
		explicit Resource(int id) : id(id) {}
		int id = 0;
	};

	std::unique_ptr<Resource> make(int id)
	{
		return std::make_unique<Resource>(id);
	}
}

namespace RegistryTests
{
	TEST_SUITE("RegistryTests")
	{
		TEST_CASE("resolved handle reads back the resource that was added")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));
			registry.add("second", make(2));

			CHECK(registry.get(registry.resolve("first"))->id == 1);
			CHECK(registry.get(registry.resolve("second"))->id == 2);

			// The by-name read is the same read.
			CHECK(registry.get("first")->id == 1);
		}

		TEST_CASE("distinct names resolve to distinct handles")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));
			registry.add("second", make(2));

			CHECK(registry.resolve("first") != registry.resolve("second"));

			// And the same name twice is the same handle - resolving is not
			// allocation.
			CHECK(registry.resolve("first") == registry.resolve("first"));
		}

		TEST_CASE("resolving a name nothing loaded throws")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));

			CHECK_THROWS_AS(registry.resolve("typo"), std::out_of_range);
			CHECK_THROWS_AS(registry.get("typo"), std::out_of_range);
		}

		TEST_CASE("an unresolved handle throws rather than reading slot zero")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));

			const Registry<Resource>::handle unresolved;
			CHECK_FALSE(unresolved.valid());
			CHECK_THROWS_AS(registry.get(unresolved), std::out_of_range);
		}

		// The reason a handle is a slot index rather than a pointer. A device
		// loss releases every texture and font and the reload builds new ones;
		// everything already drawing is still holding the handles it resolved
		// beforehand, and they have to keep working.
		TEST_CASE("a handle survives release and reload, and sees the new resource")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));
			registry.add("second", make(2));

			const auto first = registry.resolve("first");
			const auto second = registry.resolve("second");

			registry.release_all();

			// Released is not absent-but-fine: reading says so, loudly.
			CHECK_THROWS_AS(registry.get(first), std::out_of_range);
			CHECK_FALSE(registry.contains("first"));

			// The reload replays the same names and builds different objects.
			registry.add("first", make(10));
			registry.add("second", make(20));

			// Same handles, resolved before the loss, now reading the rebuilt
			// resources - and still not crossed over with each other.
			CHECK(registry.get(first)->id == 10);
			CHECK(registry.get(second)->id == 20);
			CHECK(registry.contains("first"));
		}

		TEST_CASE("reloading in a different order does not shuffle handles")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));
			registry.add("second", make(2));

			const auto first = registry.resolve("first");

			registry.release_all();

			// A loader is free to walk its assets in another order on the
			// rebuild. Slots belong to names, not to arrival order.
			registry.add("second", make(20));
			registry.add("first", make(10));

			CHECK(registry.get(first)->id == 10);
		}

		TEST_CASE("re-adding a live name replaces in place and keeps the handle")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));

			const auto first = registry.resolve("first");
			registry.add("first", make(99));

			CHECK(registry.resolve("first") == first);
			CHECK(registry.get(first)->id == 99);
		}

		TEST_CASE("contains reports what is loaded now, not what was named once")
		{
			Registry<Resource> registry("Resource");
			CHECK_FALSE(registry.contains("first"));

			registry.add("first", make(1));
			CHECK(registry.contains("first"));

			registry.release_all();
			CHECK_FALSE(registry.contains("first"));
		}

		TEST_CASE("a released resource names itself in the throw")
		{
			Registry<Resource> registry("Texture");
			registry.add("player_sheet", make(1));
			const auto handle = registry.resolve("player_sheet");
			registry.release_all();

			// The whole point of the name kept beside the slot: the error says
			// which resource, not just that one was missing.
			try
			{
				registry.get(handle);
				FAIL("expected a throw");
			}
			catch (const std::out_of_range& error)
			{
				const std::string message = error.what();
				CHECK(message.find("player_sheet") != std::string::npos);
				CHECK(message.find("Texture") != std::string::npos);
			}
		}

		// A handle carries the resource type, so this is a compile-time
		// property rather than a runtime one - but the registry alias is what
		// callers actually spell, so pin that it is the type it should be.
		TEST_CASE("the registry's handle alias matches the resource type")
		{
			CHECK(std::is_same_v<Registry<Resource>::handle, Handle<Resource>>);
		}
	}
}
