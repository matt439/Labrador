import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Closely framed screenshots for viewing the proposals directly on a phone.
export default async function ({ evaluate, send }) {
	const repo = fileURLToPath(new URL('../../../', import.meta.url));
	const output = path.join(repo, 'out/labrador-dog-concepts/phone-cards');
	await mkdir(output, { recursive: true });
	const page = await evaluate("location.pathname.split('/').pop().replace('.html', '')");
	await send('Emulation.setDeviceMetricsOverride', { width: 390, height: 900, deviceScaleFactor: 2, mobile: false });
	await evaluate(`(() => {
		const theme = document.getElementById('theme');
		theme.value = 'dark'; theme.dispatchEvent(new Event('change', { bubbles: true }));
		const presence = document.getElementById('presence');
		presence.value = 'asides'; presence.dispatchEvent(new Event('change', { bubbles: true }));
		document.querySelector('input[name=context][value=home]').click();
	})()`);
	await evaluate('document.fonts.ready');
	await evaluate('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))');
	async function capture(filename, selector) {
		await evaluate(`(() => {
			const el = [...document.querySelectorAll(${JSON.stringify(selector)})].find(candidate => candidate.getClientRects().length && !candidate.closest('[hidden]'));
			el.scrollIntoView({ block: 'center' });
		})()`);
		await evaluate('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))');
		const clip = await evaluate(`(() => {
			const el = [...document.querySelectorAll(${JSON.stringify(selector)})].find(candidate => candidate.getClientRects().length && !candidate.closest('[hidden]'));
			const rect = el.getBoundingClientRect();
			return {x: Math.max(0, rect.x - 12), y: rect.y + scrollY - 12, width: rect.width + 24, height: rect.height + 24, scale: 1};
		})()`);
		assert.ok(clip.width >= 250 && clip.width <= 390);
		assert.ok(clip.height < 750, 'A phone card should keep the dialogue readable without a long page');
		const { data } = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false, clip });
		await writeFile(path.join(output, filename), Buffer.from(data, 'base64'));
		console.log(`${filename}: ${Math.round(clip.width)} x ${Math.round(clip.height)} CSS pixels, 2x capture`);
	}
	await capture(page === 'index' ? 'vader-ani-personality-dry.png' : page === 'duo' ? 'vader-ani-personality-playful.png' : 'vader-ani-personality-warm.png', '[data-dog-aside]:not([hidden])');
	if (page === 'guides') {
		await evaluate("document.querySelector('input[name=context][value=examples]').click()");
		await evaluate('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))');
		await capture('vader-ani-personality-my-turn.png', '[data-dog-aside]:not([hidden])');
	}
}
