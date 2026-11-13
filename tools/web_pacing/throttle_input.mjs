// The R6 fixture under run_page.mjs's --cpu-throttle, for the throttled
// desktop rehearsal (docs/port/pacing-method.md section 5). Not a ctest
// entry: it measures rather than accepts. Each fresh trigger loads the
// development page with ?pacing=topout, waits for its export, checks it as
// LineSweeperPacingPageTests does, and records a failed check as a result
// rather than throwing, since a throttled run that drops observer frames is
// what the rehearsal is for. Nothing it writes is phone evidence.
//
// Environment: LABRADOR_THROTTLE_OUT names a folder for each trigger's raw
// export and the summaries (required); LABRADOR_THROTTLE_LABEL names the run
// in those files, such as "cpu-4x"; LABRADOR_THROTTLE_TRIGGERS (default 3)
// and LABRADOR_THROTTLE_DURATION_MS (default 3000) set the triggers and the
// retained duration after the first burst.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { wait_for } from '../../tests/render/web/page_wait.mjs';
import { check, prepare as prepare_fixture } from './page_input.mjs';
import { summarize_capture } from './throttle.mjs';

export { ready, strict_complaints } from './page_input.mjs';

// The first WebGL clear's time from navigation start: the page's first frame.
const FIRST_CLEAR = `(() => {
	const clear = WebGL2RenderingContext.prototype.clear;
	WebGL2RenderingContext.prototype.clear = function (...args) {
		if (window.labradorFirstClearMs === undefined) window.labradorFirstClearMs = performance.now();
		return clear.apply(this, args);
	};
})()`;

export async function prepare(page) {
	await prepare_fixture(page);
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: FIRST_CLEAR });
}

export default async function (page) {
	const out = process.env.LABRADOR_THROTTLE_OUT;
	if (!out) throw new Error('LABRADOR_THROTTLE_OUT names no folder for the captures.');
	const label = process.env.LABRADOR_THROTTLE_LABEL ?? 'unlabelled';
	const triggers = Number(process.env.LABRADOR_THROTTLE_TRIGGERS ?? 3);
	const duration = Number(process.env.LABRADOR_THROTTLE_DURATION_MS ?? 3000);
	mkdirSync(out, { recursive: true });
	const base = new URL(await page.evaluate('location.href'));
	const summaries = [];
	for (let trigger = 1; trigger <= triggers; trigger++) {
		const next = new URL(base);
		next.searchParams.set('pacing', 'topout');
		next.searchParams.set('pacing_id', `${label}-${trigger}`);
		next.searchParams.set('pacing_duration_ms', String(duration));
		await page.send('Page.navigate', { url: next.href });
		const report = await wait_for(page.evaluate,
			"typeof Module === 'object' ? Module['labradorPacingResult'] : null",
			{ done: (value) => value?.run?.id === `${label}-${trigger}`, timeout_ms: 180000, interval_ms: 100,
				navigating: true, wait: page.wait, message: `No topout capture completed for ${label}-${trigger}.` });
		const progress = await page.evaluate(`(() => {
			const canvas = document.querySelector('canvas');
			return { clears: canvas.labradorPacingClears ?? 0, marked: canvas.labradorPacingMarkedDraw ?? null,
				first_clear_ms: window.labradorFirstClearMs ?? null };
		})()`);
		let check_error = null;
		try {
			check(report, 'topout', progress);
		} catch (error) {
			check_error = error.message;
		}
		const summary = { label, trigger, check_error, first_clear_ms: progress.first_clear_ms,
			...summarize_capture(report) };
		writeFileSync(join(out, `${label}-${trigger}.json`), JSON.stringify(report));
		summaries.push(summary);
		console.log(`throttle ${label} trigger ${trigger}: ${JSON.stringify(summary)}`);
	}
	writeFileSync(join(out, `${label}-summary.json`), JSON.stringify(summaries, null, 2));
}
