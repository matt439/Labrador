#include <doctest/doctest.h>

#include "engine/render/throw_if_failed.h"

#include <stdexcept>
#include <string>
#include <type_traits>

using namespace labrador;

// What a Direct3D failure is, as a type.
//
// THIS IS A CONTRACT TEST, NOT A BEHAVIOUR TEST, and the contract is one file
// over. engine/render/resource_factory.h says add_texture_asset throws
// std::runtime_error naming the texture and the format when the device will not
// take it. The promise has two halves, as throw_if_failed.h says:
// com_exception's base keeps the type half at every ThrowIfFailed site, and
// each backend's add_texture_asset writes the message naming the texture and
// the format, because it is the code that knows them. A com_exception derived from
// std::exception rather than std::runtime_error would break the type half on
// both Direct3D backends while leaving every one of them compiling.
//
// throw_if_failed.h belongs in render/, beside the resource_factory.h whose
// promise it helps keep, and not in core/. This test sits in RenderTests for
// the same reason. It is compiled in every Windows configuration, so the
// assertions below run in all five native ones.
//
// A catch site cannot tell the two bases apart when it catches
// std::exception, so the assertion is the one a compiler can make.

TEST_SUITE("ThrowIfFailedTests")
{
	TEST_CASE("CONTRACT: a device failure is a std::runtime_error")
	{
		// The static form, so this is settled at compile time, including in
		// the configurations that build no Direct3D file.
		static_assert(
			std::is_base_of<std::runtime_error, com_exception>::value,
			"render/resource_factory.h declares std::runtime_error. "
			"com_exception's base keeps the type half of that promise "
			"at every ThrowIfFailed site, so it has to be one.");

		CHECK(std::is_base_of<std::runtime_error, com_exception>::value);
	}

	TEST_CASE("ThrowIfFailed passes success through and throws on failure")
	{
		CHECK_NOTHROW(ThrowIfFailed(S_OK));

		// E_FAIL rather than a made-up value, so the message below is one
		// somebody could actually meet.
		CHECK_THROWS_AS(ThrowIfFailed(E_FAIL), std::runtime_error);

		// And catching it as the declared type is what the header invites
		// a client to write.
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
		// A what() formatting into a function-local static buffer would have
		// every com_exception in a process return the same pointer, so the
		// last one asked would overwrite the answer for all of them. Each
		// exception holds its own message in the base.
		const com_exception first(E_FAIL);
		const com_exception second(E_INVALIDARG);

		const std::string first_text = first.what();
		const std::string second_text = second.what();

		CHECK(first_text != second_text);
		CHECK(first_text.find("80004005") != std::string::npos);
		CHECK(second_text.find("80070057") != std::string::npos);
	}

	TEST_CASE("the named form says which operation failed and with what code")
	{
		// The engine-written Direct3D sites pass the operation, the way
		// Vulkan's check_vk does, so a failure report names the call
		// rather than leaving a bare code and many candidate sites.
		CHECK_NOTHROW(ThrowIfFailed(S_OK, "The swap chain could not be created"));
		CHECK_NOTHROW(ThrowIfFailed(S_FALSE, "The swap chain could not be created"));

		std::string message;
		try
		{
			ThrowIfFailed(E_INVALIDARG, "The swap chain could not be created");
		}
		catch (const std::runtime_error& error)
		{
			message = error.what();
		}
		CHECK(message ==
			"The swap chain could not be created (HRESULT 80070057).");

		// Still a com_exception, so the type contract above covers it.
		CHECK_THROWS_AS(ThrowIfFailed(E_FAIL, "Anything"), com_exception);
	}

	TEST_CASE("hresult_name spells a code as eight upper-case digits")
	{
		CHECK(hresult_name(E_FAIL) == "HRESULT 80004005");
		CHECK(hresult_name(DXGI_ERROR_DEVICE_REMOVED) ==
			"HRESULT 887A0005");
		CHECK(hresult_name(S_OK) == "HRESULT 00000000");
	}
}
