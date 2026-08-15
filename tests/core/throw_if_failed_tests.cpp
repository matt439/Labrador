#include <doctest/doctest.h>
#include "engine/core/throw_if_failed.h"
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace labrador;

// What a D3D11 failure is, as a type.
//
// THIS IS A CONTRACT TEST, NOT A BEHAVIOUR TEST, and the contract is written
// down in a different module. engine/render/resource_factory.h says
// add_texture_asset throws std::runtime_error naming the texture and the format
// when the device will not take it. The GL backend does exactly that at four
// sites. The D3D11 backend routes every failure through ThrowIfFailed, and
// com_exception used to derive from std::exception - so that sentence had never
// once described the implementation that existed when it was written, and
// git log -S over the header shows it never could have.
//
// A CATCH SITE COULD NOT TELL, WHICH IS WHY NOTHING NOTICED. Every catch in
// this repository is catch (const std::exception&), so both types were caught
// identically and the divergence was invisible from the outside. It would have
// become visible the first time a client wrote the catch the header invites -
// and by then it would have been a client's bug to diagnose, in a backend it
// cannot see.
//
// So the assertion is the one a compiler can make and a reader cannot forget.

namespace ThrowIfFailedTests
{
	TEST_SUITE("ThrowIfFailedTests")
	{
		TEST_CASE("CONTRACT: a device failure is a std::runtime_error")
		{
			// The static form, so this is settled at compile time in every
			// configuration - including the two where no D3D11 file is built
			// at all and no runtime assertion below could ever run.
			static_assert(
				std::is_base_of<std::runtime_error, com_exception>::value,
				"render/resource_factory.h declares std::runtime_error. "
				"ThrowIfFailed is how the D3D11 backend keeps that promise, "
				"so com_exception has to be one.");

			CHECK(std::is_base_of<std::runtime_error, com_exception>::value);
		}

		TEST_CASE("ThrowIfFailed passes success through and throws on failure")
		{
			CHECK_NOTHROW(ThrowIfFailed(S_OK));

			// E_FAIL rather than a made-up value, so the message below is one
			// somebody could actually meet.
			CHECK_THROWS_AS(ThrowIfFailed(E_FAIL), std::runtime_error);

			// And catching it as the declared type is what the header invites
			// a client to write. This is the line that would not have compiled
			// as a catch of the old type.
			bool caught = false;
			try
			{
				ThrowIfFailed(E_FAIL);
			}
			catch (const std::runtime_error& error)
			{
				caught = true;

				// The HRESULT is in the message. It is the only thing this
				// exception knows and printing it is the whole of what a
				// caller can do with one, so a message that lost it would
				// leave a failure report saying nothing at all.
				const std::string text = error.what();
				CHECK(text.find("80004005") != std::string::npos);
			}
			CHECK(caught);
		}

		TEST_CASE("two live exceptions do not share one buffer")
		{
			// what() used to format into a function-local static char[64], so
			// every com_exception in a process returned the same pointer and
			// the last one to be asked overwrote the answer for all of them.
			// The review that found this argued it could not race today -
			// Scene::draw is the only add_task site and the pool catches per
			// task - and that argument is correct and is not a fix. Holding
			// the message in the base costs nothing and removes the question.
			const com_exception first(E_FAIL);
			const com_exception second(E_INVALIDARG);

			const std::string first_text = first.what();
			const std::string second_text = second.what();

			CHECK(first_text != second_text);
			CHECK(first_text.find("80004005") != std::string::npos);
			CHECK(second_text.find("80070057") != std::string::npos);
		}
	}
}
