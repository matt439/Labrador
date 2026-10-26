// Offline reduction of one browser rAF capture. This does not run a browser,
// infer scan-out or replace the operator's display and visible-judder record.
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

function requireValue(condition, message) {
	if (!condition) throw new Error(message);
}

export function nearestRank(values, quantile) {
	requireValue(quantile > 0 && quantile <= 1, 'Quantile must be in (0, 1].');
	requireValue(values.every((value) => Number.isFinite(value) && value >= 0),
		'Quantile input must contain finite nonnegative durations.');
	if (values.length === 0) return null;
	const sorted = [...values].sort((a, b) => a - b);
	return sorted[Math.ceil(quantile * sorted.length) - 1];
}

function summary(values) {
	if (values.length === 0) return { count: 0, p50_ms: null, p99_ms: null, max_ms: null };
	return {
		count: values.length,
		p50_ms: nearestRank(values, 0.5),
		p99_ms: nearestRank(values, 0.99),
		max_ms: values.reduce((largest, value) => Math.max(largest, value), 0),
	};
}

export function analyse(capture) {
	requireValue(capture?.schema === 'labrador-web-pacing-v1', 'Unsupported capture schema.');
	const { run, samples } = capture;
	const identity = ['id', 'source_commit', 'build', 'page_url', 'device', 'browser',
		'display', 'refresh_source', 'workload'];
	for (const field of identity) {
		requireValue(typeof run?.[field] === 'string' && run[field].trim().length > 0,
			`run.${field} must identify this run.`);
	}
	requireValue(typeof run.source_dirty === 'boolean', 'run.source_dirty must be recorded.');
	requireValue(Number.isFinite(run.refresh_hz) && run.refresh_hz > 0,
		'run.refresh_hz must be a positive, operator-recorded refresh rate.');
	requireValue(capture.instrument === 'requestAnimationFrame',
		'This reducer accepts rAF delivery timestamps, not CPU phases or GPU time.');
	requireValue(capture.status === 'complete' || capture.status === 'truncated',
		'Capture status must be complete or truncated.');
	requireValue(Array.isArray(samples) && samples.length > 0, 'Capture has no raw samples.');
	requireValue(Array.isArray(capture.events), 'Capture must retain its event list.');

	const period = 1000 / run.refresh_hz;
	const intervals = [];
	const excluded = {};
	const classes = { short: 0, within_one_period: 0, late_callback: 0 };
	const periods = {};
	const values = [];
	let previous = null;
	let segmentDuration = 0;
	let longestSegment = 0;
	for (const sample of samples) {
		requireValue(Number.isSafeInteger(sample.frame) && sample.frame >= 0,
			'Each frame id must be a nonnegative safe integer.');
		requireValue(Number.isFinite(sample.timestamp_ms) && sample.timestamp_ms >= 0,
			'Each timestamp_ms must be finite and nonnegative.');
		requireValue(Number.isSafeInteger(sample.segment) && sample.segment >= 0,
			'Each segment must be a nonnegative safe integer.');
		requireValue(sample.phase === 'warmup' || sample.phase === 'measure',
			'Each phase must be warmup or measure.');
		requireValue(typeof sample.visible === 'boolean', 'Each row must record page visibility.');
		const interval = previous ? sample.timestamp_ms - previous.timestamp_ms : null;
		let reason = null;
		if (!previous) reason = 'first_sample';
		else if (sample.phase === 'warmup') reason = 'warmup';
		else if (previous.phase !== 'measure') reason = 'measurement_boundary';
		else if (!sample.visible || !previous.visible) reason = 'hidden_interval';
		else if (sample.segment !== previous.segment) reason = 'discontinuity';
		else if (sample.frame !== previous.frame + 1) reason = 'sequence_gap';
		else if (interval <= 0) reason = 'non_positive_interval';

		const decision = { frame: sample.frame, interval_ms: interval, excluded: reason };
		if (reason) {
			excluded[reason] = (excluded[reason] ?? 0) + 1;
			segmentDuration = 0;
		} else {
			// This is an observation against the declared reference period.
			// A late callback alone proves neither judder nor a dropped scan-out.
			decision.classification = interval < period * 0.5 ? 'short' :
				interval > period * 1.5 ? 'late_callback' : 'within_one_period';
			classes[decision.classification]++;
			decision.nearest_reference_periods = Math.max(1, Math.round(interval / period));
			periods[decision.nearest_reference_periods] =
				(periods[decision.nearest_reference_periods] ?? 0) + 1;
			values.push(interval);
			segmentDuration += interval;
			longestSegment = Math.max(longestSegment, segmentDuration);
		}
		intervals.push(decision);
		previous = sample;
	}
	return {
		schema: 'labrador-web-pacing-report-v1',
		// Preserve the original rows and metadata, including every excluded row.
		raw: capture,
		reference_period_ms: period,
		intervals,
		summary: summary(values),
		excluded,
		classifications: classes,
		nearest_reference_periods: periods,
		longest_contiguous_measurement_ms: longestSegment,
		continuous_two_minutes: capture.status === 'complete' && longestSegment >= 120000,
		scope: 'rAF delivery cadence only; visible judder, CPU phases, GPU time and scan-out are not measured',
	};
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		if (process.argv.length !== 3) throw new Error('Usage: node tools/web_pacing/analyse.mjs <capture.json>');
		const report = analyse(JSON.parse(readFileSync(process.argv[2], 'utf8')));
		console.log(JSON.stringify(report, null, 2));
	} catch (error) {
		console.error(`web_pacing: ${error.message}`);
		process.exitCode = 2;
	}
}
