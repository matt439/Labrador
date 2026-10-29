#include <doctest/doctest.h>
#include "engine/core/registry.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
using namespace labrador;

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

	class ThrowingStorage
	{
	public:
		ThrowingStorage() = default;
		explicit ThrowingStorage(int id, bool throw_on_move = false) :
			resource_(make(id)), throw_on_move_(throw_on_move) {}
		ThrowingStorage(const ThrowingStorage&) = delete;
		ThrowingStorage& operator=(const ThrowingStorage&) = delete;
		ThrowingStorage(ThrowingStorage&& other)
		{
			if (other.throw_on_move_)
			{
				throw std::runtime_error("storage move refused");
			}
			this->resource_ = std::move(other.resource_);
		}
		ThrowingStorage& operator=(ThrowingStorage&&) = default;

		Resource* get() const { return this->resource_.get(); }

	private:
		std::unique_ptr<Resource> resource_;
		bool throw_on_move_ = false;
	};
}

namespace RegistryTests
{
	TEST_SUITE("RegistryTests")
	{
		TEST_CASE("a failed new resource leaves its name absent and retryable")
		{
			Registry<Resource, ThrowingStorage> registry("Resource");
			registry.add("kept", ThrowingStorage(1));
			const Handle<Resource> kept = registry.resolve("kept");

			CHECK_THROWS_AS(registry.add("retry", ThrowingStorage(2, true)),
				std::runtime_error);
			CHECK_THROWS_AS(registry.resolve("retry"), std::out_of_range);
			CHECK(registry.get(kept)->id == 1);

			registry.add("other", ThrowingStorage(3));
			const Handle<Resource> other = registry.resolve("other");
			CHECK_FALSE(registry.contains("retry"));
			CHECK_THROWS_AS(registry.resolve("retry"), std::out_of_range);
			registry.add("retry", ThrowingStorage(4));

			CHECK(registry.resolve("retry") != other);
			CHECK(registry.get("retry")->id == 4);
			CHECK(registry.get(other)->id == 3);
			CHECK(registry.get(kept)->id == 1);
		}

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

		// A handle names a slot rather than its current object. Releasing and
		// refilling resources must preserve previously resolved handles so their
		// readers reach the replacements. Texture storage across device loss is
		// one use of this contract; this case exercises the generic registry.
		TEST_CASE("a handle survives release and reload, and sees the new resource")
		{
			Registry<Resource> registry("Resource");
			registry.add("first", make(1));
			registry.add("second", make(2));

			const Handle<Resource> first = registry.resolve("first");
			const Handle<Resource> second = registry.resolve("second");

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

			const Handle<Resource> first = registry.resolve("first");

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

			const Handle<Resource> first = registry.resolve("first");
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
			const Handle<Resource> handle = registry.resolve("player_sheet");
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
