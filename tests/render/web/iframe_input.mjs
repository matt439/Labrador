// The packaged bundle in a cross-site iframe, served from two disposable
// loopback origins. DevTools probes the child document; every input event is
// sent to the top page and routed by the browser's real focus/hit testing.
import { PRELUDE, FACTORY_WRAPPER, bundle_state, steps } from './bundle_page.mjs';
import { close_server, serve_files } from './page_harness.mjs';
import { wait_for } from './page_wait.mjs';

const state = bundle_state();
const failures = [];
let frame;
let host;

export const ready = "document.readyState === 'complete'";
export const strict_complaints = true;
export function iframe_complaint(run, { source, level, text, url }) {
	if (run.phase !== 'download failure' || !run.loader_refused || level !== 'error') return false;
	const expected = (source === 'browser' && url === run.loader_url &&
		text === 'Failed to load resource: the server responded with a status of 404 (Not Found)') ||
		(source === 'console' && text === 'LineSweeper failure: TypeError: Failed to fetch dynamically imported module: ' + run.loader_url);
	if (expected) run.expected_diagnostics.push(text);
	return expected;
}
export const expected_complaint = (complaint) => iframe_complaint(state, complaint);

export function host_page(game_url, delegated) {
	const url = new URL(game_url);
	if (url.hostname !== '127.0.0.1' || url.protocol !== 'http:') {
		throw new Error('the iframe fixture needs its own loopback game origin');
	}
	const escaped = url.href.replaceAll('&', '&amp;').replaceAll('"', '&quot;');
	return `<!doctype html><html><head><meta charset="utf-8"><title>Iframe regression host</title>
	<style>body { margin: 0; } #outside { height: 40px; } iframe { display: block; border: 0; width: 1100px; height: 850px; }</style>
	</head><body><button id="outside">Host keyboard target</button>
	<iframe id="game" title="LineSweeper regression" src="${escaped}"${delegated ? ' allow="fullscreen; gamepad; autoplay"' : ''}></iframe>
	<script>window.labradorHost = { keys: [], fullscreen_errors: 0 };
	addEventListener('keydown', (event) => window.labradorHost.keys.push(event.code), true);
	addEventListener('fullscreenerror', () => window.labradorHost.fullscreen_errors++);
	</script></body></html>`;
}

const FRAME_PRELUDE = PRELUDE + `;
window.labradorIframe = { keys: [], fullscreen_errors: 0 };
addEventListener('keydown', (event) => window.labradorIframe.keys.push(event.code), true);
addEventListener('fullscreenerror', () => window.labradorIframe.fullscreen_errors++);`;

export async function prepare(page) {
	await page.send('Emulation.setDeviceMetricsOverride',
		{ width: 1280, height: 1000, deviceScaleFactor: 1, mobile: false });
	page.on((message) => {
		if (message.method === 'Target.detachedFromTarget' && frame?.id === message.params.sessionId) {
			frame = undefined;
		}
		if (message.method !== 'Target.attachedToTarget') return;
		const { sessionId, targetInfo } = message.params;
		const child = page.child_session(sessionId);
		const setup = async () => {
			if (targetInfo.type !== 'iframe') {
				await child.send('Runtime.runIfWaitingForDebugger');
				return;
			}
			frame = { id: sessionId, page: child };
			await child.send('Runtime.enable');
			await child.send('Log.enable');
			await child.send('Inspector.enable');
			await child.send('Page.enable');
			await child.send('Page.addScriptToEvaluateOnNewDocument', { source: FRAME_PRELUDE });
			child.on((event) => {
				if (event.method !== 'Fetch.requestPaused') return;
				const { requestId, request } = event.params;
				const url = new URL(request.url);
				const loader = url.pathname.endsWith('/linesweeper.js') && url.search === '';
				const failing = loader && state.phase === 'download failure';
				const command = loader ? child.send('Fetch.fulfillRequest', {
					requestId, responseCode: failing ? 404 : 200,
					responseHeaders: [{ name: 'Content-Type', value: failing ? 'text/plain' : 'text/javascript' }],
					body: Buffer.from(failing ? 'iframe test missing loader' : FACTORY_WRAPPER).toString('base64'),
				}) : child.send('Fetch.continueRequest', { requestId });
				if (failing) {
					state.loader_refused = true;
					state.loader_url = request.url;
				}
				command.catch((error) => failures.push(error));
			});
			await child.send('Fetch.enable', { patterns: [{ urlPattern: '*linesweeper.js*', requestStage: 'Request' }] });
			await child.send('Runtime.runIfWaitingForDebugger');
		};
		setup().catch((error) => failures.push(error));
	});
	await page.send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: true, flatten: true });
}

export async function cleanup() {
	if (host) {
		await close_server(host.server);
		host = undefined;
	}
}

export default async function drive(page) {
	const game_url = await page.evaluate('location.href');
	host = await serve_files([], { page: 'host.html', substitute: (url) =>
		url.pathname === '/host.html' ? { type: 'text/html; charset=utf-8',
			body: host_page(game_url, url.searchParams.get('allow') === 'yes') } : null });
	const host_url = new URL(host.url);
	host_url.hostname = 'localhost';
	const assert = (condition, message) => { if (!condition) throw new Error(`${state.phase}: ${message}`); };
	const check = () => {
		if (failures.length) throw failures[0];
	};
	const until = (evaluate, expression, message, navigating = false) => wait_for(evaluate, expression,
		{ wait: page.wait, timeout_ms: 20000, navigating, check, message: `${state.phase}: ${message}` });
	const host_click = async () => {
		const at = await page.evaluate("(() => { const button = document.getElementById('outside'); button.scrollIntoView(); const r = button.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })()");
		await page.send('Input.dispatchMouseEvent', { type: 'mousePressed', ...at, button: 'left', buttons: 1, clickCount: 1 });
		await page.send('Input.dispatchMouseEvent', { type: 'mouseReleased', ...at, button: 'left', buttons: 0, clickCount: 1 });
		assert(await page.evaluate("document.activeElement === document.getElementById('outside')"), 'outside click did not focus the host button');
	};
	for (const delegated of [false, true]) {
		state.phase = delegated ? 'delegated iframe' : 'default iframe';
		frame = undefined;
		host_url.searchParams.set('allow', delegated ? 'yes' : 'no');
		await page.send('Page.navigate', { url: host_url.href });
		await until(page.evaluate, "typeof window.labradorHost === 'object'", 'host did not load', true);
		await wait_for(async () => frame !== undefined, 'unused', { timeout_ms: 20000,
			wait: page.wait, check, message: 'cross-site iframe did not acquire its own session' });
		const child = frame.page;
		await until(child.evaluate, "document.readyState === 'complete' && typeof window.labradorBundle === 'object'",
			'iframe did not load', true);
		const input_page = { ...child, send: async (method, params) => {
			if (method === 'Input.dispatchMouseEvent') {
				const offset = await page.evaluate("(() => { const r = document.getElementById('game').getBoundingClientRect(); return { x: r.left, y: r.top }; })()");
				return page.send(method, { ...params, x: params.x + offset.x, y: params.y + offset.y });
			}
			return method.startsWith('Input.') ? page.send(method, params) : child.send(method, params);
		} };
		const { click_play, key, loaded, quit, frames } = steps(input_page, state);
		assert(await child.evaluate('location.origin !== ' + JSON.stringify(host_url.origin)), 'origins are equal');
		assert(await page.evaluate("(() => { try { return document.getElementById('game').contentWindow.document === null; } catch (e) { return e.name === 'SecurityError'; } })()"),
			'the host could read the cross-origin child document');
		await page.evaluate("localStorage.setItem('labrador-sm93-host', 'host-only')");
		assert(await child.evaluate("localStorage.getItem('labrador-sm93-host') === null"), 'host storage leaked into the game origin');
		assert(await child.evaluate('document.fullscreenEnabled') === delegated, 'fullscreen delegation was not applied');
		assert(await child.evaluate("!document.getElementById('fullscreen').hidden") === delegated, 'fullscreen button disagrees with policy');
		assert(await child.evaluate('window.labradorBundle.runs.length === 0'), 'the iframe started before Play');
		await host_click();
		await key('KeyA', 65);
		assert(await page.evaluate("window.labradorHost.keys.join(',') === 'KeyA'"), 'host did not receive its key');
		assert(await child.evaluate('window.labradorIframe.keys.length === 0'), 'an unfocused iframe received the host key');
		await click_play();
		await loaded(0);
		assert(await page.evaluate("document.activeElement === document.getElementById('game')"), 'the host did not focus its iframe');
		assert(await child.evaluate("document.activeElement === document.querySelector('canvas')"), 'Play did not focus the game canvas');
		console.log(`iframe renderer - ${state.phase}: ` + await child.evaluate(`(() => {
			const gl = document.querySelector('canvas').getContext('webgl2');
			const info = gl.getExtension('WEBGL_debug_renderer_info');
			return info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER);
		})()`));
		if (delegated) {
			const button = await child.evaluate("(() => { const r = document.getElementById('fullscreen').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })()");
			await input_page.send('Input.dispatchMouseEvent', { type: 'mousePressed', ...button, button: 'left', buttons: 1, clickCount: 1 });
			await input_page.send('Input.dispatchMouseEvent', { type: 'mouseReleased', ...button, button: 'left', buttons: 0, clickCount: 1 });
			await until(child.evaluate, "document.fullscreenElement === document.getElementById('stage')", 'delegated stage did not enter fullscreen');
			assert(await page.evaluate("document.fullscreenElement === document.getElementById('game')"), 'host did not report its fullscreen iframe');
			await child.evaluate('document.exitFullscreen()');
			await until(page.evaluate, 'document.fullscreenElement === null', 'fullscreen did not exit');
			await until(child.evaluate, 'document.fullscreenElement === null', 'child fullscreen did not exit');
			await frames(5);
		}
		await host_click();
		await frames(5);
		const before = await child.evaluate('window.labradorIframe.keys.length');
		for (const [code, vk] of [['Escape', 27], ['ArrowDown', 40], ['Enter', 13]]) await key(code, vk);
		assert(await child.evaluate('window.labradorIframe.keys.length') === before, 'host-focused keys reached the game');
		assert(await child.evaluate('!window.labradorBundle.runs[0].quit'), 'host keys quit the game');
		const canvas = await child.evaluate("(() => { const r = document.querySelector('canvas').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })()");
		await input_page.send('Input.dispatchMouseEvent', { type: 'mousePressed', ...canvas, button: 'left', buttons: 1, clickCount: 1 });
		await input_page.send('Input.dispatchMouseEvent', { type: 'mouseReleased', ...canvas, button: 'left', buttons: 0, clickCount: 1 });
		// Losing the canvas paused the actual game. This walk can quit only
		// from that pause menu; no synthetic visibility/focus event is used.
		await quit(0, true);
		await click_play();
		await loaded(1);
		assert(await child.evaluate('window.labradorBundle.runs[0].canvas !== window.labradorBundle.runs[1].canvas'), 'replay reused the old canvas');
		await quit(1);
		assert(await child.evaluate('window.labradorIframe.fullscreen_errors === 0'), 'iframe reported a fullscreen error');
		assert(await page.evaluate('window.labradorHost.fullscreen_errors === 0'), 'host reported a fullscreen error');
		console.log(`ok - ${state.phase}: policy, click-to-start, host/frame focus, pause, quit and fresh replay`);
	}
	state.phase = 'download failure';
	await frame.page.evaluate('location.reload()');
	await until(frame.page.evaluate, "document.readyState === 'complete' && typeof window.labradorBundle === 'object' && window.labradorBundle.runs.length === 0",
		'failure page did not reload', true);
	const failed_page = frame.page;
	// The failure's Play click is still routed through the host viewport.
	const button = await failed_page.evaluate("(() => { const r = document.getElementById('play').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })()");
	const offset = await page.evaluate("document.getElementById('game').getBoundingClientRect().top");
	for (const type of ['mousePressed', 'mouseReleased']) await page.send('Input.dispatchMouseEvent',
		{ type, x: button.x, y: button.y + offset, button: 'left', buttons: type === 'mousePressed' ? 1 : 0, clickCount: 1 });
	await until(failed_page.evaluate, "document.getElementById('status').classList.contains('error') && document.getElementById('play').textContent === 'Try again'",
		'the iframe download failure offered no recovery');
	assert(state.loader_refused && state.expected_diagnostics.length > 0, 'the injected loader failure was not observed');
	assert(await failed_page.evaluate('window.labradorBundle.runs.length === 0'), 'failed import created a game');
	assert(await failed_page.evaluate("document.getElementById('status').textContent === 'The game could not download. Check your connection and try again.'"), 'failure advice changed');
	check();
	console.log('ok - iframe loader failure retains visible connection advice and Try again');
}
