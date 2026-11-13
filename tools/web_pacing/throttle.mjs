// Offline summaries for the throttled desktop rehearsal (pacing-method.md
// section 5): one exported engine-frame capture, or one packaged-page load,
// reduced to the figures web-as-built.md records. Reads no browser, clock or
// display. A summary of a throttled run describes this desktop under
// emulation, never a phone.
import { nearestRank } from './analyse.mjs';

// Nanoseconds to milliseconds, to the nanosecond, without float residue.
function ms_of(ns) {
	return Math.round(ns) / 1e6;
}

function spread(values) {
	if (values.length === 0) return { count: 0, p50_ms: null, p99_ms: null, max_ms: null };
	return {
		count: values.length,
		p50_ms: nearestRank(values, 0.5),
		p99_ms: nearestRank(values, 0.99),
		max_ms: values.reduce((largest, value) => Math.max(largest, value), 0),
	};
}

function cpu_ns(row) {
	return row.update_ns + row.begin_frame_ns + row.draw_ns + row.submit_ns + row.end_frame_ns;
}

// How many completed frames ran each number of fixed updates, as
// { "0": n, "1": n, ... }.
export function updates_histogram(rows) {
	const counts = {};
	for (const row of rows) counts[row.updates] = (counts[row.updates] ?? 0) + 1;
	return counts;
}

// One `labrador-web-engine-pacing-v1` export, summarized over its measured
// rows (after warmup and the first burst), plus the marked top-out frame.
export function summarize_capture(report) {
	if (report?.schema !== 'labrador-web-engine-pacing-v1') {
		throw new Error('Not a labrador-web-engine-pacing-v1 export.');
	}
	const measured = report.samples.filter((row) => row.phase === 'measure');
	const ms = (field) => measured.map((row) => ms_of(row[field]));
	const burst = report.top_out;
	return {
		status: report.status,
		fixture_mode: report.run?.fixture_mode ?? null,
		rows: report.samples.length,
		measured_rows: measured.length,
		gaps: report.gaps?.length ?? null,
		updates_per_frame: updates_histogram(measured),
		warmup_updates_per_frame: updates_histogram(report.samples.filter((row) => row.phase !== 'measure')),
		interval: spread(ms('interval_ns')),
		update: spread(ms('update_ns')),
		draw: spread(ms('draw_ns')),
		submit: spread(ms('submit_ns')),
		cpu_total: spread(measured.map((row) => ms_of(cpu_ns(row)))),
		top_out: burst ? {
			frame: burst.frame, updates: burst.updates, live: burst.live_particles,
			dropped: burst.dropped_particles,
			update_ms: ms_of(burst.update_ns), draw_ms: ms_of(burst.draw_ns),
			submit_ms: ms_of(burst.submit_ns), cpu_total_ms: ms_of(burst.cpu_total_ns),
			interval_ms: ms_of(burst.interval_ns),
			next_interval_ms: burst.next_interval_ns === null ? null : ms_of(burst.next_interval_ns),
		} : null,
	};
}

// The packaged page's load, from Resource Timing entries and the page's own
// marks, all in ms from navigation start: the time each named file finished
// arriving and the bytes it took, then the time from the Play click to the
// first WebGL clear.
export function summarize_load({ resources, navigation, play_click_ms, first_clear_ms }) {
	const files = {};
	for (const entry of resources) {
		const name = new URL(entry.name).pathname.split('/').at(-1);
		files[name] = { transfer_bytes: entry.transferSize, body_bytes: entry.decodedBodySize,
			start_ms: entry.startTime, end_ms: entry.responseEnd };
	}
	return {
		page_ready_ms: navigation.loadEventEnd,
		files,
		play_click_ms,
		first_clear_ms,
		click_to_first_frame_ms: first_clear_ms - play_click_ms,
		navigation_to_first_frame_ms: first_clear_ms,
	};
}
