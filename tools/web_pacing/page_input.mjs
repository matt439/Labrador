// Runtime acceptance driver for the wasm-release development page.
// Node protocol tests and C++ headless counts do not substitute for this
// actual page run.
import { wait_for } from '../../tests/render/web/page_wait.mjs';

export const ready = "window.labradorPacingInitialized === true";
export const strict_complaints = true;

// The fixture's particle count: pacing_particle_count in
// samples/linesweeper/web/pacing_fixture.h. Each particle is one quad, six
// indices.
const FIXTURE_PARTICLES = 9600;
// labradorPacingFrame's argument positions (tools/web_pacing/page.js) for the
// top-out marker frame, the live particles and the refused emissions.
const MARKER = 9;
const LIVE = 10;
const DROPPED = 11;

export async function prepare(page) {
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: `(() => {
		window.labradorPacingInitialized = false;
		let module;
		Object.defineProperty(window, 'Module', {
			configurable: true,
			get() { return module; },
			set(value) {
				module = value;
				if (!value || typeof value !== 'object' || value.labradorPacingReadyHooked) return;
				Object.defineProperty(value, 'labradorPacingReadyHooked', { value: true });
				const original = value.onRuntimeInitialized;
				value.onRuntimeInitialized = function (...args) {
					const result = typeof original === 'function' ? original.apply(this, args) : undefined;
					const pacingFrame = value.labradorPacingFrame;
					if (typeof pacingFrame === 'function' && !pacingFrame.labradorPacingDrawHooked) {
						const heard = function (...snapshot) {
							const [timestamp, frame] = snapshot;
							const canvas = value.canvas;
							if (frame > 0 && frame === snapshot[${MARKER}] && snapshot[${LIVE}] === ${FIXTURE_PARTICLES} &&
								snapshot[${DROPPED}] === 0 && !canvas.labradorPacingMarkedDraw) {
								canvas.labradorPacingMarkedDraw = { frame, marker: snapshot[${MARKER}], observer_timestamp_ms: timestamp,
									draw_calls: canvas.labradorPacingDrawCalls ?? 0,
									elements: canvas.labradorPacingElements ?? 0 };
							}
							return pacingFrame.apply(this, snapshot);
						};
						heard.labradorPacingDrawHooked = true;
						value.labradorPacingFrame = heard;
					}
					window.labradorPacingInitialized = true;
					return result;
				};
			}
		});
		const clear = WebGL2RenderingContext.prototype.clear;
		WebGL2RenderingContext.prototype.clear = function (...args) {
			const result = clear.apply(this, args);
			this.canvas.labradorPacingClears = (this.canvas.labradorPacingClears ?? 0) + 1;
			this.canvas.labradorPacingDrawCalls = 0;
			this.canvas.labradorPacingElements = 0;
			return result;
		};
		const drawElements = WebGL2RenderingContext.prototype.drawElements;
		WebGL2RenderingContext.prototype.drawElements = function (...args) {
			const result = drawElements.apply(this, args);
			this.canvas.labradorPacingDrawCalls = (this.canvas.labradorPacingDrawCalls ?? 0) + 1;
			this.canvas.labradorPacingElements = (this.canvas.labradorPacingElements ?? 0) + args[1];
			return result;
		};
	})()` });
}

function requireValue(condition, message) {
	if (!condition) throw new Error(message);
}

function check(report, mode, progress) {
	requireValue(report.schema === 'labrador-web-engine-pacing-v1', 'Missing engine timing receipt.');
	requireValue(report.status === 'complete', `Pacing capture status was ${report.status}.`);
	requireValue(report.run.build === 'Release', 'SM-08 runtime acceptance requires wasm-release.');
	requireValue(report.run.source_commit !== 'unknown', 'Missing built source revision.');
	requireValue(report.run.render_backend === 'gl', 'Browser rendering acceptance requires the GL backend.');
	requireValue(report.run.fixture_mode === mode, 'The query flag did not select the requested fixture.');
	requireValue(progress.clears > 0, 'The instrumented page did not clear a WebGL frame.');
	requireValue(report.samples.length > 2, 'Too few completed engine frames.');
	requireValue(report.gaps.length === 0, 'Observer skipped completed frames; cannot verify the marked frame.');
	let previous = 0;
	for (const row of report.samples) {
		requireValue(row.frame === previous + 1, 'Completed frame ids were not contiguous.');
		previous = row.frame;
		for (const field of ['interval_ns', 'update_ns', 'begin_frame_ns', 'draw_ns', 'submit_ns', 'end_frame_ns']) {
			requireValue(Number.isFinite(row[field]) && row[field] >= 0, `Invalid ${field}.`);
		}
	}
	const burst = report.top_out;
	requireValue(burst?.counts_match === true && burst.live_particles === FIXTURE_PARTICLES && burst.dropped_particles === 0,
		`The marked completed browser frame did not report exactly ${FIXTURE_PARTICLES} live particles and zero dropped.`);
	requireValue(burst.frame === burst.top_out_frame, 'The top-out marker belongs to a different frame.');
	requireValue(progress.marked?.frame === burst.frame && progress.marked.marker === burst.top_out_frame,
		'WebGL drawing was not observed for the marked completed frame.');
	requireValue(progress.marked.draw_calls > 0 && progress.marked.elements >= FIXTURE_PARTICLES * 6,
		`The marked frame submitted fewer than ${FIXTURE_PARTICLES} quads through WebGL drawElements.`);
	requireValue(Number.isFinite(burst.next_interval_ns), 'The interval after the top-out frame is absent.');
	if (mode === 'topout-frozen') {
		requireValue(report.samples.filter((row) => row.frame >= burst.frame)
			.every((row) => row.live_particles === FIXTURE_PARTICLES && row.dropped_particles === 0),
			'The frozen workload did not keep its exact live count.');
	}
}

export default async function (page) {
	for (const mode of ['topout', 'topout-frozen']) {
		// Let run_page establish readiness at its original URL, then navigate
		// explicitly. Changing history in prepare would defeat its URL guard.
		const url = await page.evaluate('location.href');
		const next = new URL(url);
		next.searchParams.set('pacing', mode);
		next.searchParams.set('pacing_duration_ms', '250');
		await page.send('Page.navigate', { url: next.href });
		const report = await wait_for(page.evaluate,
			"typeof Module === 'object' ? Module['labradorPacingResult'] : null",
			{ done: (value) => value?.run?.fixture_mode === mode, timeout_ms: 30000, interval_ms: 25,
				navigating: true, wait: page.wait, message: `No ${mode} capture completed.` });
		const progress = await page.evaluate(`(() => {
			const canvas = document.querySelector('canvas');
			return { clears: canvas.labradorPacingClears ?? 0, marked: canvas.labradorPacingMarkedDraw ?? null };
		})()`);
		check(report, mode, progress);
		console.log(`SM-08 ${mode}: ${report.samples.length} completed frames; marked frame ${report.top_out.frame}, ` +
			`${FIXTURE_PARTICLES} live, 0 dropped, CPU total ${report.top_out.cpu_total_ns} ns. Headless browser evidence only.`);
	}
}
