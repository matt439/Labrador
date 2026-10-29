#pragma once

#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)

namespace labrador
{
	class Keyboard;
}

namespace linesweeper
{
	// Diagnostics for the state which consumed a real sampled key phase.
	struct TestInputSnapshot
	{
		int row = -1;
		int palette = 0;
	};

	// Opt-in development-page control at the fixed-update boundary. Without
	// the page hook, state updates proceed normally. An armed phase permits
	// one real update after Keyboard has sampled its genuine key event.
	class TestInputStep
	{
	public:
		TestInputStep(const labrador::Keyboard& keyboard, const char* state,
			const void* source, TestInputSnapshot (*snapshot)(const void*));
		~TestInputStep() noexcept;

		TestInputStep(const TestInputStep&) = delete;
		TestInputStep& operator=(const TestInputStep&) = delete;

		bool allowed() const;
		void result(const char* value);

	private:
		double id_ = 0;
		bool allowed_ = true;
		int exceptions_ = 0;
		const char* state_ = nullptr;
		const char* result_ = nullptr;
		const void* source_ = nullptr;
		TestInputSnapshot (*snapshot_)(const void*) = nullptr;
		TestInputStep* previous_ = nullptr;
	};

	// Pause actions run inside the active step, before StateContext applies
	// their deferred pop. No active sampled step means no diagnostic to write.
	void record_test_input_result(const char* value);
}

#endif
