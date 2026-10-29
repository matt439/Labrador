import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { runInNewContext } from 'node:vm';
import { SAMPLE_INPUT_HOOK } from './sample_key.mjs';

// Exercise the handwritten bridge's actual JavaScript with fake module data.
// No generated game module, browser, DOM, WebGL or Emscripten runtime is loaded.
const source = readFileSync(new URL('../../../samples/linesweeper/web/test_input.cpp', import.meta.url), 'utf8');
function body(name) {
	const match = source.match(new RegExp(`EM_JS\\([^,]+, ${name},[\\s\\S]*?\\), \\{([\\s\\S]*?)\\n\\}\\);`));
	assert.ok(match, `missing handwritten EM_JS body: ${name}`);
	return match[1];
}

function fixture(input = runInNewContext(SAMPLE_INPUT_HOOK)) {
	const context = { Module: { labradorTestInput: input }, UTF8ToString: (text) => text };
	return {
		input,
		module: context.Module,
		begin: runInNewContext(`(function(held, pressed, completed) {${body('linesweeper_test_input_begin')} })`, context),
		ack: runInNewContext(`(function(id, completed, state, row, palette, result) {${body('linesweeper_test_input_ack')} })`, context),
		error: runInNewContext(`(function(message) {${body('linesweeper_test_input_error')} })`, context),
	};
}

function arm(input, id, down, code_index = 2) {
	input.started = true;
	input.pending = { id, code_index, down };
}

test('absent and inactive hooks leave ordinary development updates alone', () => {
	const absent = fixture();
	absent.module.labradorTestInput = undefined;
	assert.equal(absent.begin(0, 0, 0), 0);
	const bridge = fixture();
	assert.equal(bridge.begin(0, 0, 0), 0);
	assert.equal(bridge.input.error, null);
});

test('one sampled phase stays consumed across catch-up and top-state transitions', () => {
	const bridge = fixture();
	arm(bridge.input, 1, true);
	assert.equal(bridge.begin(0, 0, 0), -1);
	assert.equal(bridge.begin(4, 4, 0), 1);
	assert.equal(bridge.ack(1, 0, 'play', -1, 0, 'pause'), 1);
	for (let update = 0; update < 80; ++update) {
		assert.equal(bridge.begin(4, 0, 1), -1);
	}
	arm(bridge.input, 2, false);
	assert.equal(bridge.begin(4, 0, 1), -1);
	assert.equal(bridge.begin(0, 0, 1), 2);
	assert.equal(bridge.ack(2, 1, 'pause', 0, 0, null), 1);
	assert.deepEqual(JSON.parse(JSON.stringify(bridge.input.ack)),
		{ id: 2, state: 'pause', row: 0, palette: 0, result: null });
	arm(bridge.input, 3, true);
	assert.equal(bridge.begin(4, 4, 2), 3);
});

test('a held level without a fresh down edge fails instead of acknowledging an action', () => {
	const bridge = fixture();
	arm(bridge.input, 1, true);
	assert.equal(bridge.begin(4, 0, 0), -1);
	assert.match(bridge.input.error, /pressed edge/);
	assert.equal(bridge.input.ack, null);
});

test('stale, skipped and malformed phase ids cannot advance the game', () => {
	for (const id of [0, -1, 2, 1.5, NaN]) {
		const bridge = fixture();
		arm(bridge.input, id, true);
		assert.equal(bridge.begin(4, 4, 0), -1);
		assert.ok(bridge.input.error);
		assert.equal(bridge.input.ack, null);
	}
});

test('extra held keys cannot turn an up phase into a non-neutral update', () => {
	const bridge = fixture();
	arm(bridge.input, 1, false);
	assert.equal(bridge.begin(1, 0, 0), -1);
	assert.ok(bridge.input.error);
	assert.equal(bridge.input.ack, null);
});

test('removing or replacing an armed hook cannot restore ordinary updates', () => {
	for (const replacement of [undefined, null, runInNewContext(SAMPLE_INPUT_HOOK)]) {
		const bridge = fixture();
		arm(bridge.input, 1, true);
		assert.equal(bridge.begin(4, 4, 0), 1);
		assert.equal(bridge.ack(1, 0, 'pause', 1, 0, null), 1);
		bridge.module.labradorTestInput = replacement;
		assert.equal(bridge.begin(0, 0, 1), -1);
		assert.ok(bridge.input.error);
	}
});

test('a changed phase or acknowledgement and a throwing update retain failure', () => {
	const bridge = fixture();
	arm(bridge.input, 1, true);
	assert.equal(bridge.begin(4, 4, 0), 1);
	bridge.input.pending.id = 2;
	assert.equal(bridge.ack(1, 0, 'pause', 1, 0, null), 0);
	assert.ok(bridge.input.error);
	assert.equal(bridge.input.ack, null);
	const throwing = fixture();
	arm(throwing.input, 1, true);
	throwing.error('sample input state update threw');
	assert.equal(throwing.begin(4, 4, 0), -1);
	assert.equal(throwing.input.ack, null);
	assert.match(throwing.input.error, /state update threw/);
});

test('malformed error state cannot silently park the test without a diagnostic', () => {
	const bridge = fixture();
	bridge.input.error = false;
	assert.equal(bridge.begin(0, 0, 0), -1);
	assert.equal(typeof bridge.input.error, 'string');
	assert.ok(bridge.input.error);
});
