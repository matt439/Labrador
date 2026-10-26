// Protocol tests with synthetic snapshots and event targets, without a browser,
// device, Application loop or claim that any fixture was actually rendered.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

const source = readFileSync(new URL('./page.js', import.meta.url), 'utf8');

function instrument(search = '?pacing=topout&pacing_duration_ms=250', callbacks = {}) {
	const canvas = new EventTarget();
	canvas.width = 1280;
	canvas.height = 720;
	canvas.getBoundingClientRect = () => ({ width: 1280, height: 720 });
	const document = new EventTarget();
	document.visibilityState = 'visible';
	const window = new EventTarget();
	const Module = { canvas, ...callbacks };
	const logs = [];
	let now = 0;
	const context = vm.createContext({
		Module, document, window, URLSearchParams,
		location: { search, href: 'https://example.invalid/synthetic' + search },
		navigator: { userAgent: 'synthetic protocol only' },
		innerWidth: 1280, innerHeight: 720, devicePixelRatio: 1,
		performance: { now: () => now++ }, console: { log: (value) => logs.push(value) },
	});
	vm.runInContext(source, context, { filename: 'page.js' });
	return {
		Module, document, window, canvas, logs,
		frame(timestamp, frame, { updates = 1, interval = 16666667, marker = 0,
			live = 0, dropped = 0 } = {}) {
			return Module.labradorPacingFrame(timestamp, frame, updates, interval,
				100, 200, 300, 400, 500, marker, live, dropped);
		},
		result() { return JSON.parse(JSON.stringify(Module.labradorPacingResult)); },
	};
}

test('without the query flag the development pre-js leaves ordinary callbacks and Module unchanged', () => {
	const original = () => 'ordinary callback';
	const { Module, logs } = instrument('', { onQuit: original, onError: original });
	assert.deepEqual(Object.keys(Module).sort(), ['canvas', 'onError', 'onQuit']);
	assert.equal(Module.onQuit, original);
	assert.equal(Module.onError, original);
	assert.equal(logs.length, 0);
});

test('only explicit modes and bounded requested durations are admitted', () => {
	assert.equal(instrument('?pacing=play').Module.labradorPacingMode, 1);
	assert.equal(instrument('?pacing=topout').Module.labradorPacingMode, 2);
	assert.equal(instrument('?pacing=topout-frozen').Module.labradorPacingMode, 3);
	assert.throws(() => instrument('?pacing=other'), /pacing must be/);
	assert.throws(() => instrument('?pacing=play&pacing_duration_ms=0'), /between/);
	assert.throws(() => instrument('?pacing=play&pacing_duration_ms=NaN'), /between/);
});

test('deduplication retains zero-update completed frames and reports the exact marked CPU snapshot', () => {
	const capture = instrument();
	capture.Module.labradorPacingBuild('synthetic-dirty', 'Release', 'gl', 'synthetic API', 'synthetic device');
	assert.equal(capture.frame(0, 0), true);
	assert.equal(capture.frame(16, 1), true);
	assert.equal(capture.frame(20, 1), true);
	assert.equal(capture.frame(5000, 2, { marker: 2, live: 9600 }), true);
	assert.equal(capture.frame(5016, 3, { updates: 0, interval: 8000000, marker: 2, live: 9600 }), true);
	assert.equal(capture.frame(5250, 4, { marker: 2, live: 9500 }), false);
	const report = capture.result();
	assert.equal(report.status, 'complete');
	assert.deepEqual(report.samples.map((row) => row.frame), [1, 2, 3, 4]);
	assert.equal(report.samples[2].updates, 0);
	assert.equal(report.duplicate_observations, 1);
	assert.equal(report.raf_capture.samples.length, 6);
	assert.equal(report.top_out.frame, 2);
	assert.equal(report.top_out.counts_match, true);
	assert.equal(report.top_out.cpu_total_ns, 1500);
	assert.equal(report.top_out.draw_submit_ns, 700);
	assert.equal(report.top_out.next_interval_ns, 8000000);
	assert.equal(report.run.source_commit, 'synthetic');
	assert.equal(report.run.source_dirty, true);
	assert.equal(report.run.build, 'Release');
	assert.equal(report.run.render_backend, 'gl');
	assert.equal(report.run.touch_controls, false);
	assert.equal(report.run.presentation_profile, 'benchmark_board_fixed_hints_no_touch_v1');
	assert.equal(capture.logs.length, 1);
	assert.equal(capture.frame(5300, 5), false);
	assert.equal(capture.logs.length, 1);
});

test('a skipped marked frame is incomplete rather than borrowing counts from a later frame', () => {
	const capture = instrument();
	capture.frame(5000, 2, { marker: 1, live: 9600 });
	capture.frame(5250, 3, { marker: 1, live: 9600 });
	const report = capture.result();
	assert.equal(report.status, 'incomplete_fixture');
	assert.equal(report.top_out, null);
	assert.deepEqual(report.gaps, [{ after_frame: 0, before_frame: 2, observer_timestamp_ms: 5000 }]);
	assert.equal(report.raf_capture.status, 'truncated');
});

test('observed gaps and regressions are preserved rather than inventing completed frames', () => {
	const capture = instrument('?pacing=play');
	capture.frame(0, 1);
	capture.frame(20, 3);
	assert.equal(capture.frame(30, 2), false);
	const report = capture.result();
	assert.equal(report.status, 'frame_regression');
	assert.deepEqual(report.samples.map((row) => row.frame), [1, 3]);
	assert.deepEqual(report.gaps, [{ after_frame: 1, before_frame: 3, observer_timestamp_ms: 20 }]);
	assert.equal(report.raf_capture.samples.length, 3);
});

test('a marked frame with incorrect particle counts cannot finish as a complete fixture', () => {
	const capture = instrument();
	capture.frame(5000, 1, { marker: 1, live: 9552 });
	capture.frame(5250, 2, { marker: 1, live: 9552 });
	assert.equal(capture.result().status, 'incomplete_fixture');
	assert.equal(capture.result().top_out.counts_match, false);
});

test('warmup is distinct from the measured play span and physical identity is never inferred', () => {
	const capture = instrument('?pacing=play');
	capture.frame(0, 1);
	capture.frame(4999, 2);
	capture.frame(5000, 3);
	assert.equal(capture.frame(125000, 4), false);
	const report = capture.result();
	assert.deepEqual(report.samples.map((row) => row.phase), ['warmup', 'warmup', 'measure', 'measure']);
	assert.equal(report.run.refresh_hz, null);
	assert.equal(report.run.source_dirty, null);
	assert.equal(report.top_out, null);
	assert.equal(report.samples[0].live_particles, null);
});

test('visibility, resize and context events survive export; listeners detach when stopped', () => {
	const capture = instrument('?pacing=play');
	capture.frame(0, 1);
	capture.document.visibilityState = 'hidden';
	capture.document.dispatchEvent(new Event('visibilitychange'));
	capture.frame(10, 2);
	capture.document.visibilityState = 'visible';
	capture.document.dispatchEvent(new Event('visibilitychange'));
	capture.window.dispatchEvent(new Event('resize'));
	capture.canvas.dispatchEvent(new Event('webglcontextlost'));
	capture.canvas.dispatchEvent(new Event('webglcontextrestored'));
	capture.frame(20, 3);
	capture.Module.labradorPacingStop();
	const report = capture.result();
	assert.equal(report.status, 'truncated');
	assert.equal(report.raf_capture.samples[1].visible, false);
	assert.deepEqual(report.raf_capture.samples.map((row) => row.segment), [0, 1, 5]);
	assert.equal(report.raf_capture.events.filter((event) => event.event !== 'geometry').length, 5);
	const eventsBefore = report.raf_capture.events.length;
	capture.document.dispatchEvent(new Event('visibilitychange'));
	assert.equal(capture.result().raf_capture.events.length, eventsBefore);
});

test('geometry is measured after the first completed frame and changes retain their own discontinuity', () => {
	const capture = instrument('?pacing=play');
	capture.canvas.width = 1920;
	capture.canvas.height = 1080;
	capture.frame(0, 0);
	capture.frame(16, 1);
	capture.canvas.width = 960;
	capture.canvas.height = 540;
	capture.frame(32, 2);
	capture.Module.labradorPacingStop();
	const report = capture.result();
	assert.deepEqual(report.run.initial_geometry.drawing_buffer, [1920, 1080]);
	assert.deepEqual(report.run.drawing_buffer, [960, 540]);
	assert.equal(report.run.buffer_orientation, 'landscape');
	const geometries = report.raf_capture.events.filter((event) => event.event === 'geometry');
	assert.deepEqual(geometries.map((event) => event.frame), [1, 2]);
	assert.equal(report.samples[1].segment, 1);
	assert.equal(report.raf_capture.samples[2].segment, 1);
});

test('manual identity survives export and cannot rewrite a completed receipt', () => {
	const capture = instrument();
	capture.Module.labradorPacingSetIdentity({ id: 'synthetic-run', device: 'synthetic-only',
		refresh_hz: 144, source_commit: 'must not override build stamp' });
	capture.Module.labradorPacingStop();
	assert.equal(capture.result().run.id, 'synthetic-run');
	assert.equal(capture.result().run.refresh_hz, 144);
	assert.equal(capture.result().run.source_commit, 'unknown');
	assert.throws(() => capture.Module.labradorPacingSetIdentity({ id: 'rewritten' }), /finished/);
});

test('the once-only renderer stamp preserves raw null/synthetic strings and cannot be forged by operator fields', () => {
	for (const [backend, api, device] of [
		['null', 'synthetic null API', 'synthetic null device'],
		['gl', 'WebGL masked synthetic API', 'WebKit WebGL'],
		['synthetic', '', 'unknown'],
	]) {
		const capture = instrument();
		capture.Module.labradorPacingBuild('synthetic-dirty', 'Release', backend, api, device);
		capture.Module.labradorPacingSetIdentity({ render_backend: 'forged', render_api: 'forged',
			render_device_name: 'forged', device: 'operator-provided physical description' });
		assert.throws(() => capture.Module.labradorPacingBuild('other', 'Debug', 'forged', 'forged', 'forged'),
			/already recorded/);
		capture.Module.labradorPacingStop();
		const report = capture.result();
		assert.equal(report.run.render_backend, backend);
		assert.equal(report.run.render_api, api);
		assert.equal(report.run.render_device_name, device);
		assert.equal(report.run.device, 'operator-provided physical description');
		assert.equal(report.run.source_commit, 'synthetic');
		assert.equal(Object.getOwnPropertyDescriptor(capture.Module.labradorPacingResult.run,
			'render_backend').writable, false);
		assert.throws(() => { capture.Module.labradorPacingResult.run.render_backend = 'forged'; }, TypeError);
	}
});

test('opt-in capture wraps quit/error once while preserving original callbacks and their receiver', () => {
	for (const name of ['onQuit', 'onError']) {
		const calls = [];
		const capture = instrument('?pacing=play', { [name]: function (...args) {
			calls.push({ receiver: this, args }); return 42;
		} });
		assert.equal(capture.Module[name]('test-message'), 42);
		assert.equal(calls[0].receiver, capture.Module);
		assert.deepEqual(calls[0].args, ['test-message']);
		assert.equal(capture.result().status, name === 'onQuit' ? 'quit' : 'error');
		assert.equal(capture.logs.length, 1);
	}
});
