// run_safari.mjs's options, page hooks, classic WebDriver transport and run
// control, against fixtures: no Safari, driver or network.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
import { PRELUDE } from './bundle_page.mjs';
import { COMPLAINT_HOOK, EVALUATE_SCRIPT, SAMPLES, SAMPLE_HOOK, WebDriverError, artifact_files, bundle_input,
	capabilities, driver_arguments, drive_sample, evaluator, held_expression, inject_head, key_actions, page_scripts, parse_options,
	safari_tabs_to_links, session_commands, substitute, webdriver_command } from './run_safari.mjs';

test('the options name one mode and refuse every other combination', () => {
	const sample = parse_options(['--pages', 'p', '--sample', 'minimal']);
	assert.equal(sample.browser_name, 'safari');
	assert.equal(sample.driver, 'safaridriver');
	assert.equal(sample.label, 'Safari');
	assert.equal(sample.timeout_ms, 60000);
	assert.equal(sample.frames, 60);
	const bundle = parse_options(['--bundle', 'b']);
	assert.equal(bundle.timeout_ms, 120000);
	assert.equal(parse_options(['--renderer']).renderer, true);
	const firefox = parse_options(['--renderer', '--browser-name', 'firefox']);
	assert.equal(firefox.driver, 'geckodriver');
	assert.equal(firefox.label, 'Firefox through geckodriver');
	for (const refused of [
		[], ['--sample', 'minimal'], ['--pages', 'p', '--sample', 'nothing'], ['--pages', 'p', '--bundle', 'b'],
		['--bundle', 'b', '--frames', '5'], ['--renderer', '--bundle', 'b'], ['--renderer', '--renderer'],
		['--renderer', '--browser-name', 'chrome'], ['--pages', 'p', '--sample', 'minimal', '--timeout', '0'],
		['--pages', 'p', '--sample', 'minimal', '--frames', '1.5'], ['--bundle'], ['bundle', 'b'], ['--other', 'x'],
	]) {
		assert.throws(() => parse_options(refused), Error, refused.join(' '));
	}
});

test('each mode serves only its own files and hooks, and only the bundle substitutes its loader', () => {
	const sample = parse_options(['--pages', 'pages', '--sample', 'collision']);
	assert.deepEqual(artifact_files(sample).map((file) => file.replace(/\\/g, '/').split('/').slice(-2).join('/')),
		['collision/CollisionSample.html', 'collision/CollisionSample.js', 'collision/CollisionSample.wasm']);
	assert.deepEqual(page_scripts(sample), [COMPLAINT_HOOK, SAMPLE_HOOK]);
	assert.equal(substitute(sample, new URL('http://h/linesweeper.js')), null);
	const bundle = parse_options(['--bundle', 'dist']);
	assert.deepEqual(artifact_files(bundle).map((file) => file.replace(/\\/g, '/').split('/').pop()),
		['index.html', 'linesweeper.js', 'linesweeper.wasm', 'poster.png']);
	assert.deepEqual(page_scripts(bundle), [COMPLAINT_HOOK, PRELUDE + ';']);
	assert.match(substitute(bundle, new URL('http://h/linesweeper.js')).body.toString(), /labrador-bundle-source/);
	assert.equal(substitute(bundle, new URL('http://h/linesweeper.js?labrador-bundle-source')), null);
	assert.deepEqual(artifact_files(parse_options(['--renderer'])), []);
});

test('the hooks go at the top of the head, before the page\'s own scripts', () => {
	assert.equal(inject_head('<html><HEAD lang="en"><script>page()</script>', ['a()', 'b()']),
		'<html><HEAD lang="en">\n<script>a()</script>\n<script>b()</script><script>page()</script>');
	assert.throws(() => inject_head('<html><body>', ['a()']), /no <head>/);
	// A <header> element is not the head.
	assert.throws(() => inject_head('<header>', ['a()']), /no <head>/);
});

test('the driver and session are Safari\'s unless Firefox is named', () => {
	assert.deepEqual(driver_arguments('safari', 4444), ['-p', '4444']);
	assert.deepEqual(driver_arguments('firefox', 4444), ['--port', '4444', '--host', '127.0.0.1', '--websocket-port', '0']);
	assert.deepEqual(capabilities('safari'), { browserName: 'safari' });
	assert.equal(capabilities('firefox')['moz:firefoxOptions'].args[0], '-headless');
});

test('key actions hold each key, and an unknown key refuses', () => {
	const [source] = key_actions(['Escape', 'KeyS'], { hold_ms: 7 });
	assert.equal(source.type, 'key');
	assert.deepEqual(source.actions, [
		{ type: 'keyDown', value: '\uE00C' }, { type: 'pause', duration: 7 }, { type: 'keyUp', value: '\uE00C' },
		{ type: 'pause', duration: 7 }, { type: 'keyDown', value: 's' }, { type: 'pause', duration: 7 },
		{ type: 'keyUp', value: 's' }, { type: 'pause', duration: 7 }]);
	for (const sample of Object.values(SAMPLES)) key_actions(sample.keys);
	assert.throws(() => key_actions(['F13']), /Unsupported sample key F13/);
});

test('Safari\'s Tab preference is read from the override, then its preferences, else off', async () => {
	assert.equal(await safari_tabs_to_links({ env: { LABRADOR_SAFARI_TAB_TO_LINKS: '1' }, read: () => '0' }), true);
	assert.equal(await safari_tabs_to_links({ env: { LABRADOR_SAFARI_TAB_TO_LINKS: '0' }, read: () => '1' }), false);
	const asked = [];
	assert.equal(await safari_tabs_to_links({ env: { HOME: '/Users/r' },
		read: async (domain, key) => { asked.push([domain, key]); return asked.length === 3 ? 'true' : null; } }), true);
	assert.match(asked[0][0], /^\/Users\/r\/Library\/Containers\/com\.apple\.Safari\//);
	assert.equal(asked[2][0], 'com.apple.Safari');
	assert.equal(await safari_tabs_to_links({ env: {}, read: async () => null }), false);
});

// Stands in for the global fetch the acceptance suite's client calls.
async function with_fetch(replies, body) {
	const calls = [];
	const real_fetch = globalThis.fetch;
	globalThis.fetch = async (url, init) => {
		calls.push({ url, method: init.method, body: init.body === undefined ? undefined : JSON.parse(init.body) });
		const reply = replies.shift();
		if (reply instanceof Error) throw reply;
		return { ok: reply.status < 400, status: reply.status, text: async () => reply.text };
	};
	try {
		await body(calls);
	} finally {
		globalThis.fetch = real_fetch;
	}
}

test('a WebDriver command returns its value and names each kind of failure', async () => {
	await with_fetch([
		{ status: 200, text: '{"value":{"ready":true}}' },
		{ status: 404, text: '{"value":{"error":"no such window","message":"gone"}}' },
		{ status: 200, text: 'not json' },
		Object.assign(new Error('slow'), { name: 'TimeoutError' }),
		new Error('refused'),
		{ status: 500, text: '{"value":null}' },
	], async (calls) => {
		assert.deepEqual(await webdriver_command('http://d', 'GET', '/status', undefined), { ready: true });
		assert.deepEqual(calls[0], { url: 'http://d/status', method: 'GET', body: undefined });
		await assert.rejects(webdriver_command('http://d', 'POST', '/x', { a: 1 }),
			(error) => error instanceof WebDriverError && error.code === 'no such window' && /gone/.test(error.message));
		assert.deepEqual(calls[1].body, { a: 1 });
		await assert.rejects(webdriver_command('http://d', 'GET', '/x', undefined), /not JSON/);
		await assert.rejects(webdriver_command('http://d', 'GET', '/x', undefined), /client timeout/);
		await assert.rejects(webdriver_command('http://d', 'GET', '/x', undefined), /connection failed/);
		await assert.rejects(webdriver_command('http://d', 'GET', '/x', undefined), /HTTP 500/);
	});
});

test('session commands are the W3C endpoints this runner needs', async () => {
	const seen = [];
	const session = session_commands('http://d', 's1', async (...args) => { seen.push(args.slice(1, 4)); return null; });
	await session.navigate('http://page/');
	await session.execute_async('script', [1], 1000);
	await session.set_timeouts({ script: 9 });
	await session.perform([{ type: 'key' }]);
	await session.release();
	await session.close();
	assert.deepEqual(seen, [
		['POST', '/session/s1/url', { url: 'http://page/' }],
		['POST', '/session/s1/execute/async', { script: 'script', args: [1] }],
		['POST', '/session/s1/timeouts', { script: 9 }],
		['POST', '/session/s1/actions', { actions: [{ type: 'key' }] }],
		['DELETE', '/session/s1/actions'],
		['DELETE', '/session/s1', undefined],
	]);
});

// EVALUATE_SCRIPT as safaridriver runs it: a function body whose last
// argument is the callback.
function run_evaluate_script(expression, context = {}) {
	return new Promise((done) => {
		const sandbox = vm.createContext({ ...context, done });
		vm.runInContext(`(function () { ${EVALUATE_SCRIPT} }).call(null, ${JSON.stringify(expression)}, done)`, sandbox);
	});
}

test('an evaluation awaits its expression and returns JSON, or the error it raised', async () => {
	assert.deepEqual(JSON.parse(await run_evaluate_script('(Promise.resolve({ a: [1, 2] }))')), { value: { a: [1, 2] } });
	assert.deepEqual(JSON.parse(await run_evaluate_script('(answer * 2)', { answer: 21 })), { value: 42 });
	assert.match(JSON.parse(await run_evaluate_script('((() => { throw new Error("bad page"); })())')).error, /bad page/);
	assert.match(JSON.parse(await run_evaluate_script('(Promise.reject(new Error("refused")))')).error, /refused/);
	const sent = [];
	const session = {
		set_timeouts: async (timeouts) => sent.push(['timeouts', timeouts]),
		execute_async: async (script, args, timeout) => {
			sent.push(['execute', args, timeout]);
			return args[0] === '(fails)' ? JSON.stringify({ error: 'Error: no' }) : JSON.stringify({ value: 3 });
		},
	};
	const evaluate = evaluator(session, () => 1234);
	assert.equal(await evaluate('1 + 2'), 3);
	assert.deepEqual(sent, [['timeouts', { script: 1234 }], ['execute', ['(1 + 2)'], 1234]]);
	await assert.rejects(evaluate('fails'), /page evaluation failed: Error: no/);
});

test('the complaint hook records console warnings, errors and uncaught failures', () => {
	const listeners = {};
	const logged = [];
	const sandbox = vm.createContext({
		console: { warn: (...args) => logged.push(['warn', ...args]), error: (...args) => logged.push(['error', ...args]) },
		addEventListener: (name, listener) => { listeners[name] = listener; },
	});
	sandbox.window = sandbox;
	vm.runInContext(COMPLAINT_HOOK, sandbox);
	vm.runInContext('console.warn("careful", 1); console.error("broken")', sandbox);
	listeners.error({ message: 'thrown' });
	listeners.unhandledrejection({ reason: 'refused' });
	assert.deepEqual(Array.from(sandbox.labradorComplaints),
		['warn: careful 1', 'error: broken', 'uncaught: thrown', 'unhandled rejection: refused']);
	assert.deepEqual(logged, [['warn', 'careful', 1], ['error', 'broken']]);
});

test('the sample hook hears Module\'s callbacks and counts live draws on its canvas only', () => {
	class Canvas {
		constructor() { this.listeners = {}; }
		addEventListener(name, listener) { this.listeners[name] = listener; }
	}
	class WebGL2RenderingContext {
		constructor(canvas) { this.canvas = canvas; this.lost = false; this.VERSION = 1; this.RENDERER = 2; }
		isContextLost() { return this.lost; }
		getParameter(name) { return name === 1 ? 'WebGL 2.0' : name === 2 ? 'WebKit WebGL' : `unmasked ${name}`; }
		getExtension(name) { return name === 'WEBGL_debug_renderer_info' ? { UNMASKED_RENDERER_WEBGL: 3 } : null; }
		clear() {}
		drawElements() {}
	}
	const sandbox = vm.createContext({ WebGL2RenderingContext });
	sandbox.window = sandbox;
	vm.runInContext(SAMPLE_HOOK, sandbox);
	const canvas = new Canvas();
	const other = new WebGL2RenderingContext(new Canvas());
	const quits = [];
	sandbox.Module = { canvas, onQuit: () => quits.push('original'), onError: () => 'handled' };
	const heard = sandbox.labradorSafari;
	assert.equal(heard.has_quit, true);
	sandbox.Module.onRuntimeInitialized();
	assert.equal(heard.initialized, true);
	const gl = new WebGL2RenderingContext(canvas);
	gl.clear(); gl.drawElements(4, 6); other.clear(); other.drawElements(4, 600);
	assert.deepEqual([heard.clears, heard.draw_calls, heard.elements], [1, 1, 6]);
	assert.deepEqual([heard.raw_api, heard.raw_renderer, heard.unmasked_renderer], ['WebGL 2.0', 'WebKit WebGL', 'unmasked 3']);
	gl.lost = true;
	gl.clear(); gl.drawElements(4, 6);
	assert.deepEqual([heard.clears, heard.draw_calls], [1, 1]);
	canvas.listeners.webglcontextlost();
	canvas.listeners.webglcontextrestored();
	assert.deepEqual([heard.lost, heard.restored], [1, 1]);
	assert.equal(sandbox.Module.onError('boom'), 'handled');
	assert.deepEqual(Array.from(heard.errors), ['boom']);
	sandbox.Module.onQuit();
	assert.deepEqual([heard.quit, quits], [true, ['original']]);
	// A Module with no onQuit is recorded as such rather than given one.
	sandbox.Module = {};
	assert.equal(heard.has_quit, false);
	assert.equal(sandbox.Module.onQuit, undefined);
});

// A page that answers drive_sample's evaluations as a sample would: it draws
// on every evaluation, loses and restores its context when asked, and quits
// once the keys have been pressed and released, if its canvas has the
// keyboard.
function sample_session({ complaint = null, error = null, draws_after_restore = true, quits = true, focused = true,
	drop = 0, drop_up = 0 } = {}) {
	const heard = { initialized: false, quit: false, errors: [], has_quit: true, clears: 0, draw_calls: 0,
		elements: 0, lost: 0, restored: 0, raw_api: 'WebGL 2.0', raw_renderer: 'WebKit WebGL', unmasked_renderer: 'Apple GPU',
		keys: [] };
	let dropped = 0;
	let dropped_up = 0;
	const log = [];
	let pressed = false;
	let lost = false;
	const draw = () => {
		if (lost || (heard.restored > 0 && !draws_after_restore)) return;
		heard.clears += 1; heard.draw_calls += 2; heard.elements += 12;
	};
	return { log, session: {
		navigate: async (url) => { log.push(['navigate', url]); heard.initialized = true; },
		set_timeouts: async () => {},
		execute_async: async (script, [expression]) => {
			let value = null;
			if (expression.includes('labradorComplaints')) {
				value = complaint && heard.draw_calls > 0 ? [complaint] : [];
			} else if (expression === '(window.labradorSafari.keys.length)') {
				value = heard.keys.length;
			} else if (expression.startsWith('(window.labradorSafari.keys.slice(')) {
				value = heard.keys.slice(Number(/slice\((\d+)\)/.exec(expression)[1]));
			} else if (expression.includes('labradorSafari ??')) {
				draw();
				if (error && heard.draw_calls > 4) heard.errors.push(error);
				value = heard;
			} else if (expression.includes('loseContext()')) {
				log.push(['lose']); lost = true; heard.lost += 1;
			} else if (expression.includes('restoreContext()')) {
				log.push(['restore']); lost = false; heard.restored += 1;
			} else if (expression.includes('requestAnimationFrame')) {
				draw(); value = true;
			} else if (expression.includes('activeElement === Module.canvas')) {
				value = focused;
			} else if (expression.includes('visibilityState')) {
				value = JSON.stringify({ focused: focused ? 'CANVAS' : 'BODY', page_focused: focused, visibility: 'visible' });
			} else {
				throw new Error(`unexpected expression ${expression}`);
			}
			return JSON.stringify({ value });
		},
		perform: async (actions) => {
			log.push(['perform', actions[0].actions.length]);
			pressed = true;
			// The first `drop` presses never reach the page, as safaridriver's once did.
			for (const action of actions[0].actions) {
				if (action.type === 'keyDown' && dropped < drop) { dropped += 1; continue; }
				if (action.type === 'keyUp' && dropped_up < drop_up) { dropped_up += 1; continue; }
				heard.keys.push((action.type === 'keyDown' ? '+' : '-') + action.value);
			}
		},
		release: async () => { log.push(['release']); if (pressed && quits) heard.quit = true; },
	} };
}

test('a sample run draws, survives a forced context loss, quits on its keys and reports the renderer', async () => {
	const { log, session } = sample_session();
	const lines = [];
	const heard = await drive_sample(session, 'http://page/MinimalSample.html', SAMPLES.minimal,
		{ pause: async () => {}, log: (line) => lines.push(line) });
	assert.deepEqual(log, [['navigate', 'http://page/MinimalSample.html'], ['lose'], ['restore'],
		...Array(SAMPLES.minimal.keys.length * 2).fill(['perform', 1]), ['release']]);
	assert.equal(lines.length, 1);
	assert.equal(heard.quit, true);
	assert.match(lines[0], /context lost 1 and restored 1 times; onQuit observed/);
	assert.match(lines[0], /RENDERER=WebKit WebGL; unmasked=Apple GPU/);
});

test('a sample run fails on a complaint, on onError, and on draws that never resume', async () => {
	await assert.rejects(drive_sample(sample_session({ complaint: 'warn: shader' }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {} }), /Page complaints: warn: shader/);
	await assert.rejects(drive_sample(sample_session({ error: 'abort' }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {} }), /Module.onError: abort/);
	await assert.rejects(drive_sample(sample_session({ draws_after_restore: false }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {}, timeout_ms: 200 }), /deadline expired/);
	await assert.rejects(drive_sample(sample_session({ quits: false }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {}, timeout_ms: 200 }), /deadline expired.*did not quit the sample; page .*CANVAS/);
});

test('a sample run sends no keys until its canvas has the keyboard, and names the focus when it never does', async () => {
	const { log, session } = sample_session({ focused: false });
	let now = 0;
	const real_now = Date.now;
	Date.now = () => (now += 1000);
	try {
		await assert.rejects(drive_sample(session, 'u', SAMPLES.minimal,
			{ pause: async () => {}, log: () => {}, timeout_ms: 10_000_000 }),
		/canvas never took the keyboard; page .*BODY/);
	} finally {
		Date.now = real_now;
	}
	assert.equal(log.some(([kind]) => kind === 'perform'), false);
});

test('the bundle steps\' DevTools calls become WebDriver actions, with Option held round Tab on request', async () => {
	const performed = [];
	const navigated = [];
	const session = { perform: async (actions) => performed.push(actions), navigate: async (url) => navigated.push(url) };
	const page = bundle_input(session, async () => 'value', { option_tab: true, pause: async () => {} });
	await page.send('Input.dispatchMouseEvent', { type: 'mousePressed', x: 10.6, y: 20.2 });
	await page.send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: 10.6, y: 20.2 });
	await page.send('Input.dispatchKeyEvent', { type: 'rawKeyDown', code: 'Tab' });
	await page.send('Input.dispatchKeyEvent', { type: 'keyUp', code: 'Tab' });
	await page.send('Input.dispatchKeyEvent', { type: 'rawKeyDown', code: 'ArrowDown' });
	await page.send('Page.navigate', { url: 'http://page/?bundle-case=desktop' });
	assert.deepEqual(performed[0][0].actions, [{ type: 'pointerMove', x: 11, y: 20, origin: 'viewport' },
		{ type: 'pointerDown', button: 0 }]);
	assert.equal(performed[1][0].actions[1].type, 'pointerUp');
	assert.deepEqual(performed[2][0].actions, [{ type: 'keyDown', value: '\uE00A' }, { type: 'keyDown', value: '\uE004' }]);
	assert.deepEqual(performed[3][0].actions, [{ type: 'keyUp', value: '\uE004' }, { type: 'keyUp', value: '\uE00A' }]);
	assert.deepEqual(performed[4][0].actions, [{ type: 'keyDown', value: '\uE015' }]);
	assert.deepEqual(navigated, ['http://page/?bundle-case=desktop']);
	assert.equal(await page.evaluate('x'), 'value');
	await assert.rejects(page.send('Browser.crashGpuProcess', {}), /no WebDriver translation/);
	await assert.rejects(page.send('Input.dispatchKeyEvent', { type: 'keyDown', code: 'F1' }), /Unsupported bundle key F1/);
	const plain = [];
	await bundle_input({ perform: async (actions) => plain.push(actions) }, null)
		.send('Input.dispatchKeyEvent', { type: 'keyDown', code: 'Tab' });
	assert.deepEqual(plain[0][0].actions, [{ type: 'keyDown', value: '\uE004' }]);
});

test('a command line run_safari cannot run exits 2 with the reason and no stack', () => {
	const run = spawnSync(process.execPath, [fileURLToPath(new URL('./run_safari.mjs', import.meta.url)), '--other', 'x'],
		{ encoding: 'utf8', timeout: 30000 });
	assert.equal(run.status, 2);
	assert.match(run.stderr, /^run_safari: Invalid or duplicate argument --other\.\s*$/);
});

test('a held key waits for both its frames and its time, whichever is later', async () => {
	const settle = (frames, ms, frame_ms) => {
		let now = 0;
		let ran = 0;
		const queue = [];
		const sandbox = vm.createContext({
			performance: { now: () => now },
			requestAnimationFrame: (callback) => queue.push(callback),
		});
		let done = false;
		vm.runInContext(held_expression(frames, ms), sandbox).then(() => { done = true; });
		return (async () => {
			while (!done && ran < 1000) {
				now += frame_ms;
				ran += 1;
				queue.shift()(now);
				await Promise.resolve();
			}
			return ran;
		})();
	};
	// Fast frames: the time decides. Slow frames: the count decides.
	assert.equal(await settle(3, 100, 10), 10);
	assert.equal(await settle(3, 100, 644), 3);
});

test('a key press the page never receives is sent again, said aloud, and refused after three tries', async () => {
	const { log, session } = sample_session({ drop: 1 });
	const lines = [];
	const heard = await drive_sample(session, 'u', SAMPLES.minimal, { pause: async () => {}, log: (line) => lines.push(line) });
	assert.equal(heard.quit, true);
	assert.match(lines[0], /did not receive Escape's keyDown; sending it again \(attempt 2\)/);
	assert.deepEqual(log.filter(([kind]) => kind === 'perform' || kind === 'release').slice(0, 3),
		[['perform', 1], ['release'], ['perform', 1]]);
	await assert.rejects(drive_sample(sample_session({ drop: 3 }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {} }), /never received Escape's keyDown in 3 attempts/);
});

test('a key release the page never receives is sent again behind a repeated press', async () => {
	const { log, session } = sample_session({ drop_up: 1 });
	const lines = [];
	const heard = await drive_sample(session, 'u', SAMPLES.minimal, { pause: async () => {}, log: (line) => lines.push(line) });
	assert.equal(heard.quit, true);
	assert.match(lines[0], /did not receive Escape's keyUp; sending it again \(attempt 2\)/);
	const performed = log.filter(([kind]) => kind === 'perform').length;
	// The dropped release costs a repeated press and the release again.
	assert.equal(performed, SAMPLES.minimal.keys.length * 2 + 2);
});
