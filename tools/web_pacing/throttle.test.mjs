// The throttled rehearsal's summaries on synthetic exports, with no browser.
import assert from 'node:assert/strict';
import test from 'node:test';
import { summarize_capture, summarize_load, updates_histogram } from './throttle.mjs';

function row(frame, phase, updates, ms) {
	const ns = ms * 1e6;
	return { frame, phase, updates, interval_ns: 16.7e6, update_ns: ns, begin_frame_ns: 0,
		draw_ns: ns, submit_ns: ns, end_frame_ns: 0, live_particles: 9600, dropped_particles: 0 };
}

test('updates per frame are counted for every update count a frame ran', () => {
	assert.deepEqual(updates_histogram([row(1, 'measure', 1, 1), row(2, 'measure', 0, 1),
		row(3, 'measure', 6, 1), row(4, 'measure', 1, 1)]), { 0: 1, 1: 2, 6: 1 });
});

test('a capture is summarized over its measured rows, with the marked burst apart', () => {
	const samples = [row(1, 'warmup', 2, 9), row(2, 'measure', 1, 1), row(3, 'measure', 3, 2)];
	const report = { schema: 'labrador-web-engine-pacing-v1', status: 'complete', run: { fixture_mode: 'topout' },
		samples, gaps: [], top_out: { ...samples[1], frame: 2, cpu_total_ns: 3e6, next_interval_ns: 33.4e6 } };
	const summary = summarize_capture(report);
	assert.equal(summary.measured_rows, 2);
	assert.deepEqual(summary.updates_per_frame, { 1: 1, 3: 1 });
	assert.deepEqual(summary.warmup_updates_per_frame, { 2: 1 });
	assert.deepEqual(summary.cpu_total, { count: 2, p50_ms: 3, p99_ms: 6, max_ms: 6 });
	assert.equal(summary.top_out.cpu_total_ms, 3);
	assert.equal(summary.top_out.next_interval_ms, 33.4);
	assert.throws(() => summarize_capture({ schema: 'other' }), /Not a labrador-web-engine-pacing-v1/);
});

test('a load names each file by its last path segment and times the first frame from the click', () => {
	const summary = summarize_load({
		navigation: { loadEventEnd: 1200 },
		resources: [{ name: 'http://127.0.0.1:1/linesweeper.wasm?x', transferSize: 500, decodedBodySize: 480,
			startTime: 1500, responseEnd: 4000 }],
		play_click_ms: 1400, first_clear_ms: 4600,
	});
	assert.equal(summary.page_ready_ms, 1200);
	assert.deepEqual(summary.files['linesweeper.wasm'], { transfer_bytes: 500, body_bytes: 480, start_ms: 1500, end_ms: 4000 });
	assert.equal(summary.click_to_first_frame_ms, 3200);
	assert.equal(summary.navigation_to_first_frame_ms, 4600);
});
