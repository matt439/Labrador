#include "samples/linesweeper/web/test_input.h"

#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)

#include "engine/input/keyboard.h"

#include <emscripten/emscripten.h>

#include <array>
#include <cstddef>
#include <exception>

EM_JS_DEPS(linesweeper_test_input, "$UTF8ToString");

EM_JS(double, linesweeper_test_input_begin,
	(int held, int pressed, double completed), {
	const input = Module["labradorTestInput"];
	const active = Module["labradorTestInputSession"];
	if (active && input !== active) {
		try { active.error = "sample input hook disappeared or changed"; } catch (_) {}
		return -1;
	}
	if (input === undefined || input === null) return 0;
	const fail = (message) => {
		try { input.error = message; } catch (_) {}
		return -1;
	};
	try {
		if (typeof input !== "object" || Array.isArray(input) ||
			input.version !== 1 || typeof input.started !== "boolean") {
			return fail("malformed sample input hook");
		}
		if (input.error !== null && typeof input.error !== "string") {
			return fail("malformed sample input error");
		}
		if (input.error !== null) return -1;
		if (!input.started) return completed === 0 && !active ? 0 :
			fail("sample input cannot stop after starting");
		Module["labradorTestInputSession"] = input;
		if (completed === 0 ? input.ack !== null :
			!input.ack || input.ack.id !== completed) {
			return fail("sample input acknowledgement changed");
		}
		const pending = input.pending;
		if (pending === null) return -1;
		if (typeof pending !== "object" || Array.isArray(pending) ||
			!Number.isSafeInteger(pending.id) || pending.id <= 0 ||
			!Number.isInteger(pending.code_index) || pending.code_index < 0 ||
			pending.code_index >= 9 || typeof pending.down !== "boolean") {
			return fail("malformed sample input phase");
		}
		if (pending.id === completed) return -1;
		if (pending.id !== completed + 1) {
			return fail("sample input phase id is stale or out of order");
		}
		const bit = 1 << pending.code_index;
		if ((held & ~bit) !== 0) {
			return fail("sample input has an unexpected held key");
		}
		if (((held & bit) !== 0) !== pending.down) return -1;
		if (pending.down && (pressed & bit) === 0) {
			return fail("sample input down phase has no pressed edge");
		}
		return pending.id;
	} catch (_) {
		return fail("sample input phase could not be read");
	}
});

EM_JS(void, linesweeper_test_input_error, (const char* message), {
	try {
		const input = Module["labradorTestInputSession"] ||
			Module["labradorTestInput"];
		if (input && !input.error) input.error = UTF8ToString(message);
	} catch (_) {}
});

EM_JS(int, linesweeper_test_input_ack,
	(double id, double completed, const char* state, int row, int palette,
		const char* result), {
	const input = Module["labradorTestInput"];
	const active = Module["labradorTestInputSession"];
	const fail = (message) => {
		try { if (active && !active.error) active.error = message; } catch (_) {}
		return 0;
	};
	try {
		if (!input || input !== active || input.version !== 1 || input.started !== true ||
			input.error !== null || !input.pending || input.pending.id !== id ||
			(completed === 0 ? input.ack !== null :
				!input.ack || input.ack.id !== completed)) {
			return fail("sample input changed during its state update");
		}
		input.ack = { id, state: UTF8ToString(state), row, palette,
			result: result ? UTF8ToString(result) : null };
		return 1;
	} catch (_) {
		return fail("sample input acknowledgement failed");
	}
});

namespace linesweeper
{
	namespace
	{
		// Both top states share the completed phase, including a transition
		// applied after update returns and catch-up updates in the same frame.
		double completed_id = 0;
		TestInputStep* current_step = nullptr;

		constexpr std::array<labrador::Key, 9> sampled_keys = {
			labrador::Key::escape, labrador::Key::up, labrador::Key::down,
			labrador::Key::left, labrador::Key::right, labrador::Key::enter,
			labrador::Key::space, labrador::Key::p, labrador::Key::r,
		};
	}

	TestInputStep::TestInputStep(const labrador::Keyboard& keyboard,
		const char* state, const void* source,
		TestInputSnapshot (*snapshot)(const void*))
		: exceptions_(std::uncaught_exceptions()), state_(state),
		  source_(source), snapshot_(snapshot)
	{
		int held = 0;
		int pressed = 0;
		for (std::size_t index = 0; index < sampled_keys.size(); ++index)
		{
			const int bit = 1 << static_cast<unsigned int>(index);
			if (keyboard.held(sampled_keys[index])) held |= bit;
			if (keyboard.pressed(sampled_keys[index])) pressed |= bit;
		}

		this->id_ = linesweeper_test_input_begin(held, pressed, completed_id);
		this->allowed_ = this->id_ >= 0;
		if (this->id_ > 0)
		{
			this->previous_ = current_step;
			current_step = this;
		}
	}

	TestInputStep::~TestInputStep() noexcept
	{
		if (this->id_ <= 0) return;
		current_step = this->previous_;
		if (std::uncaught_exceptions() != this->exceptions_)
		{
			linesweeper_test_input_error("sample input state update threw");
			return;
		}
		try
		{
			const TestInputSnapshot snapshot = this->snapshot_(this->source_);
			if (linesweeper_test_input_ack(this->id_, completed_id,
				this->state_, snapshot.row, snapshot.palette, this->result_) != 0)
			{
				completed_id = this->id_;
			}
		}
		catch (...)
		{
			linesweeper_test_input_error("sample input snapshot threw");
		}
	}

	bool TestInputStep::allowed() const
	{
		return this->allowed_;
	}

	void TestInputStep::result(const char* value)
	{
		this->result_ = value;
	}

	void record_test_input_result(const char* value)
	{
		if (current_step != nullptr) current_step->result(value);
	}
}

#endif
