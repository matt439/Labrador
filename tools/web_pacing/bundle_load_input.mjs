// The packaged LineSweeper page's load under run_page.mjs's --network, for
// the throttled desktop rehearsal (docs/port/pacing-method.md section 5).
// Not a ctest entry: it measures rather than accepts. Each fresh trigger
// loads web-dist/linesweeper/index.html with a desktop pointer, clicks Play
// through browser input and waits for the first WebGL clear, then reads
// Resource Timing for what arrived when. The page is served as built, with
// no substituted response, so the throttle shapes every file. Nothing it
// writes is phone evidence: the radio, the CPU and the cache of a phone are
// not modelled.
//
// Environment: LABRADOR_THROTTLE_OUT names a folder for the summaries
// (required); LABRADOR_THROTTLE_LABEL names the run, such as "slow-4g";
// LABRADOR_THROTTLE_TRIGGERS (default 3) sets the fresh loads.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { wait_for } from '../../tests/render/web/page_wait.mjs';
import { summarize_load } from './throttle.mjs';

const PLAY_READY = `document.readyState === 'complete' && !document.getElementById('start').hidden &&
	!document.getElementById('play').hidden && !document.getElementById('play').disabled`;

export const ready = PLAY_READY;
export const strict_complaints = true;

// The first WebGL clear's time from navigation start: the game's first frame.
const FIRST_CLEAR = `(() => {
	const clear = WebGL2RenderingContext.prototype.clear;
	WebGL2RenderingContext.prototype.clear = function (...args) {
		if (window.labradorFirstClearMs === undefined) window.labradorFirstClearMs = performance.now();
		this.canvas.labradorFrames = (this.canvas.labradorFrames ?? 0) + 1;
		return clear.apply(this, args);
	};
})()`;

export async function prepare(page) {
	await page.send('Emulation.setFocusEmulationEnabled', { enabled: true });
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: FIRST_CLEAR });
}

const TIMINGS = `(() => ({
	navigation: performance.getEntriesByType('navigation')[0].toJSON(),
	resources: performance.getEntriesByType('resource').map((entry) => entry.toJSON()),
	first_clear_ms: window.labradorFirstClearMs ?? null,
}))()`;

export default async function (page) {
	const out = process.env.LABRADOR_THROTTLE_OUT;
	if (!out) throw new Error('LABRADOR_THROTTLE_OUT names no folder for the summaries.');
	const label = process.env.LABRADOR_THROTTLE_LABEL ?? 'unlabelled';
	const triggers = Number(process.env.LABRADOR_THROTTLE_TRIGGERS ?? 3);
	mkdirSync(out, { recursive: true });
	const base = new URL(await page.evaluate('location.href'));
	const summaries = [];
	for (let trigger = 1; trigger <= triggers; trigger++) {
		if (trigger > 1) {
			const next = new URL(base);
			next.searchParams.set('load', String(trigger));
			await page.send('Page.navigate', { url: next.href });
			await wait_for(page.evaluate, `location.search === '?load=${trigger}' && (${PLAY_READY})`,
				{ timeout_ms: 180000, interval_ms: 50, navigating: true, wait: page.wait,
					message: `Play did not appear for load ${trigger}.` });
		}
		const at = await page.evaluate(`(() => {
			const box = document.getElementById('play').getBoundingClientRect();
			return { x: box.x + box.width / 2, y: box.y + box.height / 2, now: performance.now() };
		})()`);
		await page.send('Input.dispatchMouseEvent', { type: 'mousePressed', x: at.x, y: at.y,
			button: 'left', buttons: 1, clickCount: 1 });
		await page.send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: at.x, y: at.y,
			button: 'left', buttons: 0, clickCount: 1 });
		await wait_for(page.evaluate, `(document.querySelector('canvas')?.labradorFrames ?? 0) >= 5`,
			{ timeout_ms: 180000, interval_ms: 20, wait: page.wait, message: `No game frames after Play, load ${trigger}.` });
		const timings = await page.evaluate(TIMINGS);
		const summary = { label, trigger, ...summarize_load({ ...timings, play_click_ms: at.now }) };
		summaries.push(summary);
		console.log(`throttle ${label} load ${trigger}: ${JSON.stringify(summary)}`);
	}
	writeFileSync(join(out, `${label}-summary.json`), JSON.stringify(summaries, null, 2));
}
