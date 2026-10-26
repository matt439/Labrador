// The acceptance driver's initialization protocol, without loading a page,
// generated wasm/JS or a real WebGL context. This is not browser acceptance.
import assert from 'node:assert/strict';
import test from 'node:test';
import vm from 'node:vm';
import { prepare, ready } from './page_input.mjs';

test('driver readiness preserves initialization callbacks at the original URL without calledRun', async () => {
	let source = null;
	await prepare({ send: async (method, params) => {
		assert.equal(method, 'Page.addScriptToEvaluateOnNewDocument');
		source = params.source;
	} });
	assert.doesNotMatch(source, /replaceState|calledRun/);
	const clearCalls = [];
	const drawCalls = [];
	class SyntheticContext {
		clear(...args) { clearCalls.push({ receiver: this, args }); return 7; }
		drawElements(...args) { drawCalls.push({ receiver: this, args }); return 23; }
	}
	const context = vm.createContext({ WebGL2RenderingContext: SyntheticContext });
	context.window = context;
	vm.runInContext(source, context);
	assert.equal(vm.runInContext(ready, context), false);
	const initialized = [];
	const snapshots = [];
	const canvas = {};
	const module = { canvas, labradorPacingFrame: function (...args) {
		snapshots.push({ receiver: this, args }); return 'preserved frame result';
	}, onRuntimeInitialized: function (...args) {
		initialized.push({ receiver: this, args }); return 42;
	} };
	context.Module = module;
	context.Module = module; // The loader may assign the same Module again.
	assert.equal(vm.runInContext(ready, context), false);
	assert.equal(module.onRuntimeInitialized('synthetic'), 42);
	assert.equal(vm.runInContext(ready, context), true);
	assert.equal(initialized.length, 1);
	assert.equal(initialized[0].receiver, module);
	assert.deepEqual(initialized[0].args, ['synthetic']);
	const receiver = new SyntheticContext();
	receiver.canvas = canvas;
	assert.equal(receiver.clear(1), 7);
	assert.equal(receiver.canvas.labradorPacingClears, 1);
	assert.equal(clearCalls[0].receiver, receiver);
	assert.deepEqual(clearCalls[0].args, [1]);
	assert.equal(receiver.drawElements(4, 9600 * 6, 5123, 0), 23);
	assert.equal(drawCalls[0].receiver, receiver);
	assert.deepEqual(drawCalls[0].args, [4, 9600 * 6, 5123, 0]);
	assert.equal(canvas.labradorPacingDrawCalls, 1);
	assert.equal(canvas.labradorPacingElements, 9600 * 6);
	const snapshot = [5000, 2, 1, 16, 1, 2, 3, 4, 5, 3, 9600, 0];
	assert.equal(module.labradorPacingFrame(...snapshot), 'preserved frame result');
	assert.equal(canvas.labradorPacingMarkedDraw, undefined); // Completed frame 2 is not marker 3.
	assert.equal(snapshots[0].receiver, module);
	assert.deepEqual(snapshots[0].args, snapshot);
	receiver.clear(1); // A different frame must not borrow earlier drawing.
	assert.equal(canvas.labradorPacingDrawCalls, 0);
	assert.equal(canvas.labradorPacingElements, 0);
	receiver.drawElements(4, 600, 5123, 0);
	receiver.drawElements(4, 9600 * 6, 5123, 0);
	snapshot[1] = 3;
	assert.equal(module.labradorPacingFrame(...snapshot), 'preserved frame result');
	const marked = JSON.parse(JSON.stringify(canvas.labradorPacingMarkedDraw));
	assert.deepEqual(marked, { frame: 3, marker: 3, observer_timestamp_ms: 5000,
		draw_calls: 2, elements: 600 + 9600 * 6 });
	receiver.clear(1);
	module.labradorPacingFrame(...snapshot); // Duplicate observation preserves the first marked receipt.
	assert.deepEqual(JSON.parse(JSON.stringify(canvas.labradorPacingMarkedDraw)), marked);
});
