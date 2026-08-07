#include <doctest/doctest.h>
#include "engine/core/name_table.h"
#include <string>

namespace
{
	struct Element
	{
		Element() = default;
		explicit Element(int id) : id(id) {}
		int id = 0;
	};
}

namespace NameTableTests
{
	TEST_SUITE("NameTableTests")
	{
		TEST_CASE("resolved handle reads back the element that was added")
		{
			NameTable<Element> table("element");
			table.add("walk", Element(1));
			table.add("jump", Element(2));

			CHECK(table.get(table.resolve("walk")).id == 1);
			CHECK(table.get(table.resolve("jump")).id == 2);
		}

		TEST_CASE("resolving a name the table does not hold throws")
		{
			NameTable<Element> table("animation strip");
			table.add("walk", Element(1));

			CHECK_THROWS_AS(table.resolve("wlak"), std::out_of_range);

			// Naming both the kind and the name is what makes a content typo
			// findable without a debugger.
			try
			{
				table.resolve("wlak");
				FAIL("expected a throw");
			}
			catch (const std::out_of_range& error)
			{
				const std::string message = error.what();
				CHECK(message.find("wlak") != std::string::npos);
				CHECK(message.find("animation strip") != std::string::npos);
			}
		}

		TEST_CASE("an unresolved handle throws rather than reading slot zero")
		{
			NameTable<Element> table("element");
			table.add("walk", Element(1));

			const Handle<Element> unresolved;
			CHECK_THROWS_AS(table.get(unresolved), std::out_of_range);
		}

		TEST_CASE("a repeated name keeps its index")
		{
			NameTable<Element> table("element");
			table.add("walk", Element(1));
			table.add("jump", Element(2));

			const auto walk = table.resolve("walk");
			table.add("walk", Element(99));

			CHECK(table.resolve("walk") == walk);
			CHECK(table.get(walk).id == 99);
			// The other entry is untouched.
			CHECK(table.get(table.resolve("jump")).id == 2);
		}

		// SpriteFrame hands out a RECT* pointing into itself, and callers hold
		// it across a draw. That is only sound because the table stops growing
		// once the loader is done.
		TEST_CASE("references stay valid as the table is filled")
		{
			NameTable<Element> table("element");
			table.add("first", Element(1));

			const auto first = table.resolve("first");
			for (int i = 0; i < 64; i++)
			{
				table.add("filler_" + std::to_string(i), Element(i));
			}

			// Resolved before 64 reallocations, read after them.
			CHECK(table.get(first).id == 1);
		}

		TEST_CASE("contains reports membership")
		{
			NameTable<Element> table("element");
			CHECK_FALSE(table.contains("walk"));
			table.add("walk", Element(1));
			CHECK(table.contains("walk"));
		}
	}
}
