// Synthetic arithmetic/protocol tests only; these rows are not device evidence.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import { analyse, nearestRank } from './analyse.mjs';

function fixture(timestamps, overrides = {}) {
	return {
		schema: 'labrador-web-pacing-v1', instrument: 'requestAnimationFrame', status: 'complete',
		run: {
			id: 'synthetic-only', source_commit: 'synthetic-fixture', source_dirty: true,
			build: 'synthetic', page_url: 'https://example.invalid/synthetic', device: 'none',
			browser: 'none', display: 'none', refresh_hz: 100, refresh_source: 'synthetic',
			workload: 'synthetic',
		},
		events: [],
		samples: timestamps.map((timestamp_ms, frame) => ({ frame, timestamp_ms, segment: 0,
			phase: 'measure', visible: true })),
		...overrides,
	};
}

test('nearest rank uses ceil(q*N), with numeric sorting and no interpolation', () => {
	assert.equal(nearestRank([100, 2, 1, 3], 0.5), 2);
	assert.equal(nearestRank([100, 2, 1, 3], 0.99), 100);
	assert.equal(nearestRank(Array.from({ length: 100 }, (_, index) => index + 1), 0.99), 99);
	assert.equal(nearestRank([], 0.99), null);
	assert.equal(nearestRank([7], 0.5), 7);
	assert.throws(() => nearestRank([NaN], 0.5));
	assert.throws(() => nearestRank([1], 0));
});

test('raw values, not averaged percentiles or rounded bins, determine p50 and p99', () => {
	const capture = fixture([0, 10, 20, 40, 70, 80]);
	const original = JSON.stringify(capture);
	const report = analyse(capture);
	assert.deepEqual(report.summary, { count: 5, p50_ms: 10, p99_ms: 30, max_ms: 30 });
	assert.deepEqual(report.classifications, { short: 0, within_one_period: 3, late_callback: 2 });
	assert.deepEqual(report.nearest_reference_periods, { 1: 3, 2: 1, 3: 1 });
	assert.equal(JSON.stringify(report.raw), original);
	assert.equal(JSON.stringify(capture), original);
});

test('warmup, boundary, hidden/resume segment, gaps and zero intervals remain recorded', () => {
	const capture = fixture([0, 10, 20, 30, 1000, 1010, 1020, 1020, 1030]);
	capture.samples[0].phase = capture.samples[1].phase = 'warmup';
	for (let i = 4; i < capture.samples.length; ++i) capture.samples[i].segment = 1;
	for (let i = 6; i < capture.samples.length; ++i) capture.samples[i].frame++;
	capture.events.push({ event: 'visibilitychange', state: 'visible', at_ms: 999 });
	const report = analyse(capture);
	assert.deepEqual(report.excluded, {
		first_sample: 1, warmup: 1, measurement_boundary: 1,
		discontinuity: 1, sequence_gap: 1, non_positive_interval: 1,
	});
	assert.equal(report.summary.count, 3);
	assert.equal(report.intervals[4].interval_ms, 970);
	assert.equal(report.intervals.length, capture.samples.length);
	assert.deepEqual(report.raw.events, capture.events);
	assert.equal(report.longest_contiguous_measurement_ms, 10);
});

test('a coarsened or reversed timestamp is excluded, never turned into a positive duration', () => {
	const report = analyse(fixture([10, 10, 9, 20]));
	assert.equal(report.excluded.non_positive_interval, 2);
	assert.deepEqual(report.summary, { count: 1, p50_ms: 11, p99_ms: 11, max_ms: 11 });
	assert.equal(report.intervals[2].interval_ms, -1);
});

test('callbacks delivered while hidden and both visibility boundaries stay out of cadence statistics', () => {
	const capture = fixture([0, 60000, 120000, 180000, 180010]);
	capture.samples[1].visible = capture.samples[2].visible = false;
	const report = analyse(capture);
	assert.equal(report.excluded.hidden_interval, 3);
	assert.deepEqual(report.summary, { count: 1, p50_ms: 10, p99_ms: 10, max_ms: 10 });
	assert.equal(report.longest_contiguous_measurement_ms, 10);
	assert.equal(report.continuous_two_minutes, false);
	assert.deepEqual(report.raw.samples, capture.samples);
});

test('classification thresholds are explicit and independent of the fixed 60 Hz simulation', () => {
	const capture = fixture([0, 2.5, 7.5, 22.5, 37.501]);
	const report = analyse(capture); // 10 ms reference, including threshold equalities.
	assert.deepEqual(report.classifications, { short: 1, within_one_period: 2, late_callback: 1 });
	const highRefresh = fixture([0, 1000 / 144, 2000 / 144]);
	highRefresh.run.refresh_hz = 144;
	assert.equal(analyse(highRefresh).classifications.within_one_period, 2);
});

test('separate segments cannot be pooled to manufacture a continuous two-minute run', () => {
	const capture = fixture([0, 60000, 60010, 120010]);
	capture.samples[2].segment = capture.samples[3].segment = 1;
	assert.equal(analyse(capture).longest_contiguous_measurement_ms, 60000);
	assert.equal(analyse(capture).continuous_two_minutes, false);
	const continuous = fixture([0, 60000, 120000]);
	assert.equal(analyse(continuous).continuous_two_minutes, true);
	continuous.status = 'truncated';
	assert.equal(analyse(continuous).continuous_two_minutes, false);
});

test('empty or excluded-only captures never acquire invented percentile values', () => {
	const report = analyse(fixture([0]));
	assert.deepEqual(report.summary, { count: 0, p50_ms: null, p99_ms: null, max_ms: null });
	assert.equal(report.continuous_two_minutes, false);
	assert.throws(() => analyse(fixture([])), /no raw samples/);
});

test('identity, clock and protocol errors are loud', () => {
	for (const mutate of [
		(capture) => { delete capture.run.source_dirty; },
		(capture) => { capture.run.display = ''; },
		(capture) => { capture.run.refresh_hz = 0; },
		(capture) => { capture.instrument = 'GPU'; },
		(capture) => { capture.samples[1].timestamp_ms = Infinity; },
		(capture) => { capture.samples[1].frame = 1.5; },
		(capture) => { capture.samples[1].segment = -1; },
		(capture) => { capture.samples[1].phase = 'unknown'; },
		(capture) => { delete capture.samples[1].visible; },
	]) {
		const capture = fixture([0, 10]);
		mutate(capture);
		assert.throws(() => analyse(capture));
	}
});

test('CLI preserves the capture in its JSON report and reports invalid input with exit 2', () => {
	const directory = mkdtempSync(path.join(tmpdir(), 'labrador-web-pacing-'));
	try {
		const file = path.join(directory, 'capture.json');
		const script = fileURLToPath(new URL('./analyse.mjs', import.meta.url));
		const capture = fixture([0, 10, 20]);
		writeFileSync(file, JSON.stringify(capture));
		const success = spawnSync(process.execPath, [script, file], { encoding: 'utf8' });
		assert.equal(success.status, 0, success.stderr);
		assert.deepEqual(JSON.parse(success.stdout).raw, capture);
		writeFileSync(file, '{bad json');
		const failure = spawnSync(process.execPath, [script, file], { encoding: 'utf8' });
		assert.equal(failure.status, 2);
		assert.match(failure.stderr, /web_pacing:/);
		assert.equal(failure.stdout, '');
	} finally {
		assert.equal(path.dirname(path.resolve(directory)), path.resolve(tmpdir()));
		assert.match(path.basename(directory), /^labrador-web-pacing-/);
		rmSync(directory, { recursive: true, force: true });
	}
});
