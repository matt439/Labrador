import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Input module for the repository's existing tests/render/web/run_page.mjs.
export default async function ({ evaluate, send }) {
	const repo = fileURLToPath(new URL('../../../', import.meta.url));
	const output = path.join(repo, 'out/labrador-dog-concepts/screenshots');
	await mkdir(output, { recursive: true });
	const page = await evaluate("location.pathname.split('/').pop().replace('.html', '')");
	const report = { page, browser: await evaluate('navigator.userAgent'), checks: [], screenshots: [] };
	const ready = () => evaluate('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))');
	const choose = (id, value) => evaluate(`(() => { const el = document.getElementById(${JSON.stringify(id)}); el.value = ${JSON.stringify(value)}; el.dispatchEvent(new Event('change', { bubbles: true })); })()`);
	const context = (value) => evaluate(`document.querySelector('input[name="context"][value="${value}"]').click()`);
	const key = async (key, code, vk) => {
		const text = key === 'Enter' ? '\r' : key === ' ' ? ' ' : undefined;
		await send('Input.dispatchKeyEvent', { type: 'keyDown', key, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk, ...(text ? { text, unmodifiedText: text } : {}) });
		await send('Input.dispatchKeyEvent', { type: 'keyUp', key, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk });
	};
	async function screenshot(name) {
		await evaluate('window.scrollTo(0,0)');
		await ready();
		const { cssContentSize } = await send('Page.getLayoutMetrics');
		const { data } = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true, clip: { x: 0, y: 0, width: cssContentSize.width, height: cssContentSize.height, scale: 1 } });
		await writeFile(path.join(output, name), Buffer.from(data, 'base64'));
		report.screenshots.push(name);
	}
	await send('Emulation.setFocusEmulationEnabled', { enabled: true });
	await send('Accessibility.enable');
	await send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
	await evaluate('document.fonts.ready');

	for (const width of [320, 360, 768, 1440]) {
		await send('Emulation.setDeviceMetricsOverride', { width, height: 1000, deviceScaleFactor: 1, mobile: false });
		for (const theme of ['light', 'dark']) {
			await choose('theme', theme);
			for (const selectedContext of ['home', 'examples']) {
				await context(selectedContext);
				for (const presence of ['asides', 'footer', 'off']) {
					await choose('presence', presence);
					await ready();
					const state = await evaluate(`(() => {
						const visible = el => {
							const closed = el.closest('details:not([open])');
							return !!el.getClientRects().length && (!closed || closed === el || closed.querySelector('summary')?.contains(el));
						};
						const preview = document.getElementById('site-preview').getBoundingClientRect();
						const clipped = [...document.querySelectorAll('.bubble, .mock-header, .portrait, .placeholder-tag, .mock-hero h2, .guide-heading h2')].filter(visible).filter(el => {
							const rect = el.getBoundingClientRect();
							return rect.x < preview.x - 1 || rect.right > preview.right + 1 || el.scrollWidth > el.clientWidth + 1;
						}).map(el => el.className || el.tagName);
						return { width: innerWidth, scroll: document.documentElement.scrollWidth, clipped,
							contexts: [...document.querySelectorAll('[data-context]')].filter(visible).map(el => el.dataset.context),
							asides: [...document.querySelectorAll('[data-dog-aside]')].filter(visible).length,
							footer: visible(document.querySelector('[data-dog-footer]')),
							animations: document.getAnimations().length,
							images: [...document.images].every(image => image.complete && image.naturalWidth > 0) };
					})()`);
					if (state.scroll > width + 1 || state.clipped.length) await screenshot(`${page}-layout-failure.png`);
					assert.ok(state.scroll <= width + 1, JSON.stringify({ width, theme, selectedContext, presence, state }));
					assert.deepEqual(state.clipped, [], JSON.stringify({ width, theme, selectedContext, presence, state }));
					assert.deepEqual(state.contexts, [selectedContext]);
					assert.equal(state.asides, presence === 'asides' ? 1 : 0);
					assert.equal(state.footer, presence !== 'off');
					assert.equal(state.animations, 0);
					assert.equal(state.images, true);
					report.checks.push({ width, theme, context: selectedContext, presence });
				}
			}
		}
	}
	await send('Emulation.setDeviceMetricsOverride', { width: 1440, height: 1100, deviceScaleFactor: 1, mobile: false });
	await choose('theme', 'light');
	await choose('presence', 'asides');
	await context('home');
	await screenshot(`${page}-desktop-light.png`);
	await send('Emulation.setDeviceMetricsOverride', { width: 360, height: 1000, deviceScaleFactor: 1, mobile: false });
	await choose('theme', 'dark');
	await context('home');
	await screenshot(`${page}-phone-dark.png`);

	// Native keyboard events, independent of the programmatic state matrix.
	await evaluate("document.querySelector('input[name=context][value=home]').focus()");
	await key('ArrowRight', 'ArrowRight', 39);
	assert.equal(await evaluate("document.querySelector('input[name=context]:checked').value"), 'examples');
	await choose('presence', 'footer');
	await evaluate("document.querySelector('.meet-dogs summary').focus()");
	await key('Enter', 'Enter', 13);
	assert.equal(await evaluate("document.querySelector('.meet-dogs').open"), true);
	await key(' ', 'Space', 32);
	assert.equal(await evaluate("document.querySelector('.meet-dogs').open"), false);
	await evaluate("document.querySelector('.skip-link').focus()");
	await key('Tab', 'Tab', 9);
	assert.equal(await evaluate("document.activeElement.matches('.study-header .brand')"), true);
	assert.equal(await evaluate("getComputedStyle(document.activeElement).outlineStyle"), 'solid');
	await choose('presence', 'off');
	const { nodes } = await send('Accessibility.getFullAXTree');
	assert.ok(!nodes.some(node => !node.ignored && node.name?.value === 'Proposed fictional dialogue from Vader and Ani'));
	assert.ok(!nodes.some(node => !node.ignored && node.name?.value === 'Meet Vader & Ani'));
	assert.ok(nodes.some(node => !node.ignored && node.role?.value === 'heading' && node.name?.value === 'Pick a sample'));
	report.keyboard = 'Radio arrow key, footer Enter/Space, Tab and visible focus passed';
	report.accessibilityTree = 'Hidden dog dialogue and footer absent; active Examples heading present';
	if (page === 'guides') {
		await choose('presence', 'footer');
		await evaluate("document.querySelector('.meet-dogs').open = true");
		await screenshot('guides-footer-only-phone-dark.png');
	}
	await writeFile(path.join(output, `${page}-report.json`), JSON.stringify(report, null, 2));
	console.log(`${page}: ${report.checks.length} rendered states, native keyboard checks and accessibility-tree checks passed`);
}
