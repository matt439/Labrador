#include <doctest/doctest.h>

#include "engine/core/diagnostic.h"
#include "engine/core/thread_pool.h"
#include "tests/diagnostic_capture.h"
#include "tests/scratch_file.h"

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <atomic>
#include <cstdio>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
	int descriptor(std::FILE* file)
	{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
		return _fileno(file);
#else
		return fileno(file);
#endif
	}

	int duplicate(int source)
	{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
		return _dup(source);
#else
		return dup(source);
#endif
	}

	int replace_descriptor(int source, int destination)
	{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
		return _dup2(source, destination);
#else
		return dup2(source, destination);
#endif
	}

	void close_descriptor(int file)
	{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
		_close(file);
#else
		close(file);
#endif
	}

	class StderrCapture
	{
	public:
		StderrCapture()
		{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
			fopen_s(&this->file_, this->scratch_.path().c_str(), "w+b");
#else
			this->file_ = std::fopen(this->scratch_.path().c_str(), "w+b");
#endif
			if (this->file_ == nullptr)
			{
				throw std::runtime_error("Could not create the stderr capture file");
			}
			std::fflush(stderr);
			this->saved_ = duplicate(descriptor(stderr));
			if (this->saved_ < 0 ||
				replace_descriptor(descriptor(this->file_), descriptor(stderr)) < 0)
			{
				if (this->saved_ >= 0) close_descriptor(this->saved_);
				std::fclose(this->file_);
				throw std::runtime_error("Could not redirect stderr for the diagnostic test");
			}
		}

		~StderrCapture()
		{
			std::fflush(stderr);
			replace_descriptor(this->saved_, descriptor(stderr));
			close_descriptor(this->saved_);
			std::fclose(this->file_);
		}

		StderrCapture(const StderrCapture&) = delete;
		StderrCapture& operator=(const StderrCapture&) = delete;

		std::string contents()
		{
			std::fflush(stderr);
			std::rewind(this->file_);
			std::string result;
			char bytes[256];
			std::size_t count;
			while ((count = std::fread(bytes, 1, sizeof(bytes), this->file_)) != 0)
			{
				result.append(bytes, count);
			}
			return result;
		}

	private:
		labrador_tests::ScratchFile scratch_{ "diagnostic.stderr", std::string_view{} };
		std::FILE* file_ = nullptr;
		int saved_ = -1;
	};

	thread_local bool reporting = false;

	struct ConcurrentCapture
	{
		std::atomic<int> calls = 0;
		std::atomic<bool> synchronous = true;
	};

	void count_message(std::string_view message, void* context) noexcept
	{
		ConcurrentCapture& capture = *static_cast<ConcurrentCapture*>(context);
		if (!reporting || message != "worker") capture.synchronous = false;
		++capture.calls;
	}
}

TEST_CASE("diagnostics use the installed callback and borrow the exact message bytes")
{
	StderrCapture stderr_capture;
	labrador_tests::DiagnosticCapture capture;
	const char bytes[] = { 'a', '\0', 'b', 'x' };
	labrador::report_diagnostic(std::string_view(bytes, 3));
	CHECK(capture.calls() == 1);
	CHECK(capture.messages() == std::string_view(bytes, 3));
	CHECK_FALSE(capture.overflow());
	CHECK(stderr_capture.contents().empty());
}

TEST_CASE("replacement changes the borrowed context and reset restores stderr")
{
	StderrCapture stderr_capture;
	{
		labrador_tests::DiagnosticCapture first;
		labrador::report_diagnostic("first");
		{
			labrador_tests::DiagnosticCapture second;
			labrador::report_diagnostic("second");
			CHECK(first.messages() == "first");
			CHECK(second.messages() == "second");
		}
		// Destroying the second capture resets to the default, and never
		// reinstalls the first borrowed context.
		labrador::report_diagnostic("default");
		CHECK(first.calls() == 1);
	}
	labrador::report_diagnostic("after context destruction\n");
	CHECK(stderr_capture.contents() == "default\nafter context destruction\n");
}

TEST_CASE("the default writes exact bytes to stderr and terminates each message once")
{
	StderrCapture stderr_capture;
	labrador::set_diagnostic_callback(nullptr);
	const char bytes[] = { 'a', '\0', 'b', 'x' };
	labrador::report_diagnostic(std::string_view(bytes, 3));
	labrador::report_diagnostic("already terminated\n");
	labrador::report_diagnostic("");
	const std::string expected = std::string(bytes, 3) + "\nalready terminated\n\n";
	CHECK(stderr_capture.contents() == expected);
}

TEST_CASE("worker diagnostics call the sink synchronously on their reporting threads")
{
	ConcurrentCapture capture;
	labrador::set_diagnostic_callback(count_message, &capture);
	{
		labrador::ThreadPool pool(1, 4);
		for (int i = 0; i < 32; ++i)
		{
			pool.add_task([]()
				{
					reporting = true;
					labrador::report_diagnostic("worker");
					reporting = false;
				});
		}
		pool.wait_for_tasks_to_complete();
	}
	labrador::set_diagnostic_callback(nullptr);
	CHECK(capture.calls == 32);
	CHECK(capture.synchronous);
}

static_assert(noexcept(labrador::report_diagnostic(std::string_view{})));
static_assert(noexcept(labrador::set_diagnostic_callback(nullptr)));
