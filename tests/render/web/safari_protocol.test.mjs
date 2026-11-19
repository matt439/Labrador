// run_safari.mjs's options, page hooks, classic WebDriver transport and run
// control, against fixtures: no Safari, driver or network.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
import { pixel_diagnostics } from './pixel_diagnostics.mjs';
import { PRELUDE } from './bundle_page.mjs';
import { COMPLAINT_HOOK, EVALUATE_SCRIPT, SAMPLES, SAMPLE_HOOK, SAMPLED_SAMPLE_HOOK, WebDriverError, artifact_files, bundle_input,
	assert_collision_keys, assert_test_exit, drive_test, TEST_LOG_HOOK, capabilities, driver_arguments, drive_sample, evaluator, held_expression, inject_head, key_actions, page_scripts, parse_options,
	safari_tabs_to_links, sampled_linesweeper_quit, session_commands, substitute, webdriver_command } from './run_safari.mjs';
import { SAMPLE_INPUT_HOOK } from './sample_key.mjs';
import { SAFARI_WINDOW_HOOK, drive_safari_window, safari_window_input } from './safari_window.mjs';

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
	assert.deepEqual(page_scripts(parse_options(['--pages', 'pages', '--sample', 'linesweeper'])),
		[COMPLAINT_HOOK, SAMPLED_SAMPLE_HOOK]);
	assert.equal(substitute(sample, new URL('http://h/linesweeper.js')), null);
	const bundle = parse_options(['--bundle', 'dist']);
	assert.deepEqual(artifact_files(bundle).map((file) => file.replace(/\\/g, '/').split('/').pop()),
		['index.html', 'linesweeper.js', 'linesweeper.wasm', 'poster.png']);
	assert.deepEqual(page_scripts(bundle), [COMPLAINT_HOOK, PRELUDE + ';']);
	assert.match(substitute(bundle, new URL('http://h/linesweeper.js')).body.toString(), /labrador-bundle-source/);
	assert.equal(substitute(bundle, new URL('http://h/linesweeper.js?labrador-bundle-source')), null);
	assert.deepEqual(artifact_files(parse_options(['--renderer'])), []);
});

test('the WebWindow mode serves its HTML, loader, wasm and input fixture', () => {
	const window = parse_options(['--window-page', 'tests/app/WebWindowTests.html']);
	assert.deepEqual(artifact_files(window).map((file) => file.replace(/\\/g, '/').split('/').pop()),
		['WebWindowTests.html', 'WebWindowTests.js', 'WebWindowTests.wasm']);
	assert.deepEqual(page_scripts(window), [COMPLAINT_HOOK, SAFARI_WINDOW_HOOK]);
	for (const conflict of [['--bundle', 'b'], ['--test-page', 'pixel.html'], ['--frames', '2'], ['--pixel-output', 'p']]) {
		assert.throws(() => parse_options(['--window-page', 'window.html', ...conflict]));
	}
	assert.throws(() => parse_options(['--window-page', 'window.js']));
});

test('classic WebWindow input sends coordinates, wheel axes and Option-Tab as W3C actions', async () => {
	const seen = [];
	const at = { x: 120, y: 70 };
	const session = { perform: async (actions) => seen.push(...actions) };
	const input = safari_window_input(session, undefined, { option_tab: true });
	await input.press(at);
	await input.move({ x: 650, y: 400 }, true);
	await input.release({ x: 650, y: 400 });
	await input.wheel(at, 50, -100);
	await input.key('KeyA', { control: true });
	await input.key('Tab');
	assert.deepEqual(seen.map(({ type, actions }) => [type, actions]), [
		['pointer', [{ type: 'pointerMove', ...at, origin: 'viewport' }, { type: 'pointerDown', button: 0 }]],
		['pointer', [{ type: 'pointerMove', x: 650, y: 400, origin: 'viewport' }]],
		['pointer', [{ type: 'pointerMove', x: 650, y: 400, origin: 'viewport' }, { type: 'pointerUp', button: 0 }]],
		['wheel', [{ type: 'scroll', ...at, deltaX: 50, deltaY: -100, origin: 'viewport' }]],
		['key', [{ type: 'keyDown', value: '\uE009' }, { type: 'keyDown', value: 'a' },
			{ type: 'keyUp', value: 'a' }, { type: 'keyUp', value: '\uE009' }]],
		['key', [{ type: 'keyDown', value: '\uE00A' }, { type: 'keyDown', value: '\uE004' },
			{ type: 'keyUp', value: '\uE004' }, { type: 'keyUp', value: '\uE00A' }]],
	]);
	assert.equal(seen[0].id, seen[2].id);
	assert.equal(seen[0].parameters.pointerType, 'mouse');
});

test('the Safari window fixture handles trusted activation, DPR and touch state', async () => {
	const emitted = [];
	const listeners = {};
	const seen = [];
	const context = vm.createContext({
		devicePixelRatio: 2, labradorComplaints: ['error: web window test failure'],
		addEventListener: (type, listener) => { listeners[type] = listener; },
		PointerEvent: class {
			constructor(type, options) { Object.assign(this, { type }, options); }
		},
		Module: { canvas: { dispatchEvent: (event) => emitted.push(event) } },
	});
	context.window = context;
	vm.runInContext(SAFARI_WINDOW_HOOK, context);
	const evaluate = (expression) => vm.runInContext(expression, context);
	const session = {
		navigate: async (...parameters) => seen.push(parameters),
		perform: async ([source]) => {
			for (const { type } of source.actions) {
				const dom_type = type.toLowerCase();
				for (const isTrusted of [false, true]) {
					const called = [];
					listeners[dom_type]({ type: dom_type, code: 'ArrowLeft', isTrusted,
						preventDefault: () => called.push('prevented'), stopImmediatePropagation: () => called.push('stopped') });
					assert.deepEqual(called, isTrusted ? ['stopped', 'prevented'] : []);
				}
			}
		},
		release: async () => seen.push('release'),
	};
	const fixture = { timeout_ms: 123, log: () => {}, drive: async (page, input) => {
		assert.equal(await page.evaluate('devicePixelRatio'), 1);
		await input.device_pixel_ratio(2);
		assert.equal(await page.evaluate('devicePixelRatio'), 2);
		assert.equal(await page.evaluate('Promise.resolve(45)', { user_gesture: true }), 45);
		assert.equal(await page.evaluate('labradorWindowFixture.activation'), null);
		await input.touch('touchStart', [[10, 20, 1]]);
		await input.touch('touchStart', [[10, 20, 1], [20, 30, 2]]);
		await input.touch('touchMove', [[21, 32, 2]]);
		await input.touch('touchEnd', [[10, 20, 1]]);
		await input.touch('touchCancel', []);
		await input.touch_end();
	} };
	await drive_safari_window(session, 'http://window/', evaluate, fixture);
	assert.deepEqual(seen, [['http://window/', 123], 'release']);
	assert.deepEqual(emitted.map(({ pointerType, type, pointerId, clientX, clientY }) =>
		[pointerType, type, pointerId, clientX, clientY]), [
		['touch', 'pointerdown', 1, 10, 20], ['touch', 'pointerdown', 2, 20, 30],
		['touch', 'pointermove', 2, 21, 32], ['touch', 'pointerup', 1, 10, 20],
		['touch', 'lostpointercapture', 1, 10, 20], ['touch', 'pointercancel', 2, 21, 32],
		['touch', 'lostpointercapture', 2, 21, 32],
	]);
	context.labradorComplaints.push('warn: an unexpected warning');
	await assert.rejects(drive_safari_window(session, 'http://window/', evaluate,
		{ ...fixture, drive: async () => {} }), /unexpected warning/);
	context.labradorExit = Promise.resolve({ abort: 'loader failed' });
	await assert.rejects(drive_safari_window(session, 'http://window/', evaluate,
		{ ...fixture, drive: async () => {} }), /Early WebWindow exit.*loader failed/);
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
	sandbox.Module = { canvas, onQuit: () => {
		sandbox.labradorSafari.keys.push('+Escape@9');
		quits.push('original');
	}, onError: () => 'handled' };
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
	assert.equal(heard.quit_key_count, 0);
	heard.keys.push('+KeyO@1', '-KeyO@2');
	sandbox.Module.onQuit();
	assert.equal(heard.quit_key_count, 0, 'a later callback cannot replace the first quit boundary');
	// A Module with no onQuit is recorded as such rather than given one.
	sandbox.Module = {};
	assert.equal(heard.has_quit, false);
	assert.equal(sandbox.Module.onQuit, undefined);
});

test('only the sampled development hook wires Module to the shared input protocol', () => {
	for (const sampled of [false, true]) {
		class WebGL2RenderingContext { clear() {} drawElements() {} }
		const sandbox = vm.createContext({ WebGL2RenderingContext });
		sandbox.window = sandbox;
		vm.runInContext(sampled ? SAMPLED_SAMPLE_HOOK : SAMPLE_HOOK, sandbox);
		sandbox.Module = { onQuit() {} };
		if (sampled) {
			assert.equal(sandbox.Module.labradorTestInput, sandbox.labradorSafari.input);
			assert.equal(sandbox.labradorRun, sandbox.labradorSafari);
			assert.equal(sandbox.Module.labradorTestInput.started, false);
		} else {
			assert.equal(sandbox.Module.labradorTestInput, undefined);
			assert.equal(sandbox.labradorRun, undefined);
		}
	}
});

// A page that answers drive_sample's evaluations as a sample would: it draws
// on every evaluation, loses and restores its context when asked, and quits
// once the keys have been pressed and released, if its canvas has the
// keyboard.
function sample_session({ complaint = null, error = null, draws_after_restore = true, quits = true, focused = true,
	drop = 0, drop_up = 0, quit_after = 1 } = {}) {
	const heard = { initialized: false, quit: false, errors: [], has_quit: true, clears: 0, draw_calls: 0,
		elements: 0, lost: 0, restored: 0, raw_api: 'WebGL 2.0', raw_renderer: 'WebKit WebGL', unmasked_renderer: 'Apple GPU',
		keys: [] };
	let dropped = 0;
	let dropped_up = 0;
	const log = [];
	// How many key performs the page needs before a release quits it: two for
	// a gallery whose first list of Escapes Safari spent leaving full screen.
	let performs = 0;
	let pressed = false;
	let lost = false;
	const draw = () => {
		if (lost || (heard.restored > 0 && !draws_after_restore)) return;
		heard.clears += 1; heard.draw_calls += 2; heard.elements += 12;
	};
	return { heard, log, session: {
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
			} else if (expression.includes('fullscreenElement')) {
				value = true;
			} else if (expression.includes('visibilityState')) {
				value = JSON.stringify({ focused: focused ? 'CANVAS' : 'BODY', page_focused: focused, visibility: 'visible' });
			} else {
				throw new Error(`unexpected expression ${expression}`);
			}
			return JSON.stringify({ value });
		},
		perform: async (actions) => {
			log.push(['perform', actions[0].actions.length]);
			performs += 1;
			pressed = performs >= quit_after;
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

// One W3C command can quit while its action list is still executing. Events
// here are DOM codes, not WebDriver values; pauses emit no keyboard event.
function collision_session({ events, stale = [], quit_after = 5, second_quit = false,
	reply = async () => {}, error = null, complaint = null } = {}) {
	const run = sample_session({ quits: false });
	run.heard.quit_key_count = null;
	run.heard.keys.push(...stale);
	let performed = false;
	const evaluate = run.session.execute_async;
	run.session.execute_async = async (script, args) => {
		if (performed && complaint && args[0].includes('labradorComplaints')) {
			return JSON.stringify({ value: [complaint] });
		}
		return evaluate(script, args);
	};
	const quit = () => {
		if (run.heard.quit_key_count === null) run.heard.quit_key_count = run.heard.keys.length;
		run.heard.quit = true;
	};
	run.session.perform = async (sources) => {
		performed = true;
		const actions = sources[0].actions;
		run.log.push(['perform', actions.length]);
		assert.equal(actions.length, 12, 'collision must remain one complete action command');
		assert.deepEqual(actions.filter((action) => action.type === 'pause').map((action) => action.duration),
			Array(6).fill(150));
		const requested = actions.filter((action) => action.type !== 'pause').map((action) => {
			const code = action.value === 'o' ? 'KeyO' : action.value === '\uE00C' ? 'Escape' : null;
			assert.notEqual(code, null);
			return (action.type === 'keyDown' ? '+' : '-') + code;
		});
		assert.deepEqual(requested, ['+KeyO', '-KeyO', '+KeyO', '-KeyO', '+Escape', '-Escape']);
		for (const [index, event] of (events ?? requested).entries()) {
			run.heard.keys.push(event + '@' + index);
			if (index + 1 === quit_after) quit();
		}
		if (second_quit) quit();
		if (error) run.heard.errors.push(error);
		await reply();
	};
	return run;
}

test('collision uses one action list and proves both O cycles before a mid-list quit, despite a delayed reply', async () => {
	let acknowledge;
	let entered;
	const waiting = new Promise((resolve) => { acknowledge = resolve; });
	const executing = new Promise((resolve) => { entered = resolve; });
	const run = collision_session({ reply: async () => { entered(); await waiting; } });
	const lines = [];
	const pending = drive_sample(run.session, 'u', SAMPLES.collision,
		{ pause: async () => {}, log: (line) => lines.push(line) });
	await executing;
	assert.equal(run.heard.keys.length, 6);
	assert.equal(run.heard.quit_key_count, 5, 'Escape release can follow the real quit');
	assert.equal(run.log.some(([kind]) => kind === 'release'), false, 'wait for the one action reply before cleanup');
	acknowledge();
	const heard = await pending;
	assert.equal(heard.quit, true);
	assert.deepEqual(run.log.filter(([kind]) => kind === 'perform'), [['perform', 12]]);
	assert.match(lines[0], /collision keys before first onQuit: \+KeyO,-KeyO,\+KeyO,-KeyO,\+Escape/);
	assert.deepEqual(assert_collision_keys({ ...heard, quit_key_count: 6 }, 0),
		['+KeyO', '-KeyO', '+KeyO', '-KeyO', '+Escape', '-Escape']);
});

test('collision refuses missing, reordered, wrong or stale O edges without resending its list', async () => {
	const complete = ['+KeyO', '-KeyO', '+KeyO', '-KeyO', '+Escape', '-Escape'];
	for (const options of [
		{ events: ['+KeyO', '+KeyO', '-KeyO', '+Escape', '-Escape'], quit_after: 4 },
		{ events: ['+KeyO', '-KeyO', '+KeyO', '+Escape', '-Escape'], quit_after: 4 },
		{ events: ['-KeyO', '+KeyO', '+KeyO', '-KeyO', '+Escape'], quit_after: 5 },
		{ events: ['+KeyA', '-KeyA', '+KeyA', '-KeyA', '+Escape'], quit_after: 5 },
		{ stale: complete.map((event) => event + '@0'), events: ['+Escape', '-Escape'], quit_after: 1 },
		{ stale: ['+KeyO@0'] },
		{ quit_after: 1, second_quit: true },
	]) {
		const run = collision_session(options);
		await assert.rejects(drive_sample(run.session, 'u', SAMPLES.collision,
			{ pause: async () => {}, log: () => {} }), /Collision/);
		assert.deepEqual(run.log.filter(([kind]) => kind === 'perform'), [['perform', 12]]);
	}
});

test('collision accepts held-key repeats only within two complete release-separated O cycles', () => {
	const keys = ['+KeyO', '+KeyO', '-KeyO', '+KeyO', '+KeyO', '-KeyO', '+Escape'];
	assert.deepEqual(assert_collision_keys({ quit: true, keys: keys.map((key) => key + '@0'), quit_key_count: 7 }, 0),
		['+KeyO', '-KeyO', '+KeyO', '-KeyO', '+Escape']);
	const balanced = ['+KeyO@0', '-KeyO@0', ...keys.map((key) => key + '@0')];
	assert.deepEqual(assert_collision_keys({ quit: true, keys: balanced, quit_key_count: balanced.length }, 2),
		['+KeyO', '-KeyO', '+KeyO', '-KeyO', '+Escape']);
	assert.throws(() => assert_collision_keys({ quit: true, keys: [], quit_key_count: null }, 0), /boundary/);
});

test('collision cannot accept complete keys without genuine quit, or hide late complaints and onError', async () => {
	await assert.rejects(drive_sample(collision_session({ quit_after: null }).session, 'u', SAMPLES.collision,
		{ pause: async () => {}, log: () => {}, timeout_ms: 200 }), /deadline expired.*did not quit the sample/);
	await assert.rejects(drive_sample(collision_session({ error: 'late abort' }).session, 'u', SAMPLES.collision,
		{ pause: async () => {}, log: () => {} }), /Module.onError: late abort/);
	await assert.rejects(drive_sample(collision_session({ complaint: 'warn: after keys' }).session, 'u', SAMPLES.collision,
		{ pause: async () => {}, log: () => {} }), /Page complaints: warn: after keys/);
});

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

// The production handwritten Emscripten bridge, with a tiny menu standing in
// for C++ state updates. Classic WebDriver actions feed real key levels to
// that bridge; eighty catch-up updates run before each command returns.
function sampled_session({ wrong_row = false, failure = null, missing_hook = false } = {}) {
	const source = readFileSync(new URL('../../../samples/linesweeper/web/test_input.cpp', import.meta.url), 'utf8');
	const body = (name) => {
		const found = source.match(new RegExp(`EM_JS\\([^,]+, ${name},[\\s\\S]*?\\), \\{([\\s\\S]*?)\\n\\}\\);`));
		assert.ok(found, name);
		return found[1];
	};
	const heard = { quit: false, errors: [], input: vm.runInNewContext(SAMPLE_INPUT_HOOK) };
	const module = { labradorTestInput: heard.input };
	const bridge = vm.createContext({ Module: module, UTF8ToString: (text) => text });
	const begin = vm.runInContext(`(function(held, pressed, completed) { const key_count = 9; ${body('linesweeper_test_input_begin')} })`, bridge);
	const ack = vm.runInContext(`(function(id, completed, state, row, palette, result) { ${body('linesweeper_test_input_ack')} })`, bridge);
	if (missing_hook) heard.input = undefined;
	const actions = [];
	const updates = [];
	let held = 0;
	let previous = 0;
	let completed = 0;
	let state = 'play';
	let row = -1;
	const frame = () => {
		const pressed = held & ~previous;
		previous = held;
		const id = begin(held, pressed, completed);
		if (id <= 0) return;
		const down = held !== 0;
		updates.push({ id, down });
		let result = null;
		if (state === 'play' && held === 1) result = 'pause';
		if (state === 'pause' && held === 4) ++row;
		if (state === 'pause' && held === 32 && row === 3) result = 'quit';
		if (ack(id, completed, state, wrong_row ? 9 : row, 0, result)) completed = id;
		if (result === 'pause') { state = 'pause'; row = 0; }
		if (result === 'quit') heard.quit = true;
	};
	const context = vm.createContext({ window: { labradorRun: heard }, requestAnimationFrame: (callback) => {
		setImmediate(() => { frame(); callback(); });
	} });
	const session = {
		set_timeouts: async () => {},
		execute_async: async (script, [expression]) => {
			assert.equal(script, EVALUATE_SCRIPT);
			try { return JSON.stringify({ value: await vm.runInContext(expression, context) }); }
			catch (error) { return JSON.stringify({ error: error.message }); }
		},
		perform: async ([source]) => {
			assert.equal(source.type, 'key');
			assert.equal(source.id, 'keyboard');
			assert.equal(source.actions.length, 1);
			const [action] = source.actions;
			actions.push(action);
			const code = { '\uE00C': 0, '\uE015': 2, '\uE007': 5 }[action.value];
			assert.ok(Number.isInteger(code), 'the exact sample quit keys are translated');
			held = action.type === 'keyDown' ? 1 << code : 0;
			if (failure) module.labradorTestInput.error = failure;
			for (let tick = 0; tick < 80; ++tick) frame();
		},
	};
	return { session, actions, updates, heard };
}

test('classic WebDriver sampled quit keeps exact rows across delayed replies and catch-up', async () => {
	const run = sampled_session();
	const logs = [];
	await sampled_linesweeper_quit(run.session, evaluator(run.session, () => 1000), { log: (line) => logs.push(line) });
	assert.equal(run.heard.quit, true);
	assert.deepEqual(run.actions.map((action) => action.type), Array(5).fill(['keyDown', 'keyUp']).flat());
	assert.deepEqual(run.actions.filter((action) => action.type === 'keyDown').map((action) => action.value),
		['\uE00C', '\uE015', '\uE015', '\uE015', '\uE007']);
	assert.deepEqual(run.updates.map((update) => update.down), [true, false, true, false, true, false, true, false, true]);
	assert.equal(logs.length, 8);
	assert.match(logs.at(-1), /"row":3/);
});

test('sampled quit refuses a wrong menu row, a protocol failure and an unavailable hook', async () => {
	for (const [options, expected] of [
		[{ wrong_row: true }, /unexpected sampled menu state/],
		[{ failure: 'bridge failed' }, /bridge failed/],
		[{ missing_hook: true }, /protocol is unavailable/],
	]) {
		const run = sampled_session(options);
		await assert.rejects(sampled_linesweeper_quit(run.session, evaluator(run.session, () => 1000),
			{ log: () => {} }), expected);
		assert.equal(run.heard.quit, false);
		if (options.missing_hook) assert.equal(run.actions.length, 0);
		else assert.equal(run.actions.at(-1).type, 'keyUp');
	}
});

test('a sample run fails on a complaint, on onError, and on draws that never resume', async () => {
	await assert.rejects(drive_sample(sample_session({ complaint: 'warn: shader' }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {} }), /Page complaints: warn: shader/);
	await assert.rejects(drive_sample(sample_session({ error: 'abort' }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {} }), /Module.onError: abort/);
	await assert.rejects(drive_sample(sample_session({ draws_after_restore: false }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {}, timeout_ms: 200 }), /deadline expired/);
	// Long enough for every key's confirmation queries on a slow runner.
	await assert.rejects(drive_sample(sample_session({ quits: false }).session, 'u', SAMPLES.minimal,
		{ pause: async () => {}, log: () => {}, timeout_ms: 3000 }), /deadline expired.*did not quit the sample; page .*CANVAS/);
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

test('audio and components retain one action list while minimal confirms each level', async () => {
	for (const sample of [SAMPLES.audio, SAMPLES.components]) {
		const run = sample_session();
		await drive_sample(run.session, 'u', sample, { pause: async () => {}, log: () => {} });
		assert.deepEqual(run.log.filter(([kind]) => kind === 'perform'), [['perform', sample.keys.length * 4]]);
	}
	// A gallery still running after its list, once out of full screen, gets
	// one more Escape, and says so.
	const spent = sample_session({ quit_after: 2 });
	const lines = [];
	const heard = await drive_sample(spent.session, 'u', SAMPLES.components, { pause: async () => {}, log: (line) => lines.push(line) });
	assert.equal(heard.quit, true);
	assert.deepEqual(spent.log.filter(([kind]) => kind === 'perform'), [['perform', SAMPLES.components.keys.length * 4], ['perform', 4]]);
	assert.match(lines[0], /still running after its keys; pressing Escape once more/);
	const minimal = sample_session();
	await drive_sample(minimal.session, 'u', SAMPLES.minimal, { pause: async () => {}, log: () => {} });
	assert.deepEqual(minimal.log.filter(([kind]) => kind === 'perform'),
		Array(SAMPLES.minimal.keys.length * 2).fill(['perform', 1]));
});

test('test-page mode is exclusive and requires a named html page', () => {
	const options = parse_options(['--test-page', 'RenderPixelTests.html']);
	assert.deepEqual(page_scripts(options), [COMPLAINT_HOOK, TEST_LOG_HOOK]);
	assert.equal(artifact_files(options).length, 3);
	assert.equal(parse_options(['--test-page', 'p.html', '--pixel-output', 'rejected']).pixel_output,
		resolve('rejected'));
	for (const args of [
		['--test-page', 'p.js'], ['--test-page', 'p.html', '--renderer'],
		['--test-page', 'p.html', '--bundle', 'b'],
		['--test-page', 'p.html', '--pages', 'p'],
		['--test-page', 'p.html', '--frames', '5'],
		['--test-page', 'p.html', '--pixel-output', ''],
		['--renderer', '--pixel-output', 'rejected'],
	]) assert.throws(() => parse_options(args));
});
test('test exit requires integer zero with no abort', () => {
	assert_test_exit({ code: 0 });
	for (const value of [null, {}, {code: '0'}, {code: 1}, {code: 0, abort: 'bad'}]) {
		assert.throws(() => assert_test_exit(value), /Test page failed/);
	}
});
test('test page driver waits on exit, emits output and refuses complaints', async () => {
	for (const complaints of [[], ['error: broken']]) {
		const lines = [];
		const session = {
			navigate: async () => {},
			set_timeouts: async ({script}) => assert.ok(Number.isFinite(script)),
			execute_async: async (script, [expression]) => {
				let value = expression.includes('labradorExit') ? {code: 0} :
					expression.includes('labradorTestLog') ? ['pixel case passed'] : complaints;
				return JSON.stringify({ value });
			},
		};
		const run = drive_test(session, 'http://fixture', {log: line => lines.push(line)});
		if (complaints.length) await assert.rejects(run);
		else await run;
		assert.ok(lines.some(line => line.includes('pixel case passed')));
	}
});

const mismatch_fixture = 'golden mismatch: alpha\n  2 of 64 pixels differ by more than 1 (3 differ at all), worst channel by 9\n  first at (2, 4): golden rgba(0, 0, 0, 255), frame rgba(9, 0, 0, 255)';
const pass_fixture = 'golden pass: beta-2, 128 pixels, 1 differing, allowance 1, worst channel 1';

test('pixel diagnostics preserve measured mismatch fields, duplicates and CRLF', () => {
	const parsed = pixel_diagnostics([mismatch_fixture, mismatch_fixture.replaceAll('\n', '\r\n')]);
	assert.deepEqual(parsed.frames, Array(2).fill({frame: 'alpha', pixels: 64,
		differing: 3, beyond: 2, allowance: 1, worst: 9, first: {x: 2, y: 4}}));
	assert.equal(parsed.unparsed_mismatches, 0);
});

test('empty, malformed and unrelated diagnostics never claim passing frames', () => {
	for (const lines of [[], ['doctest: all passed'], ['golden mismatch: truncated']]) {
		const parsed = pixel_diagnostics(lines);
		assert.deepEqual(parsed.frames, []);
		assert.deepEqual(parsed.passing_frames, []);
		assert.equal(parsed.suite_verdict, 'use exit protocol and page complaints');
	}
	assert.equal(pixel_diagnostics(['golden mismatch: truncated']).unparsed_mismatches, 1);
});

test('passing comparisons preserve exact and within-allowance drift separately from failures', () => {
	const exact = 'golden pass: beta-2, 128 pixels, 0 differing, allowance 1, worst channel 0';
	const parsed = pixel_diagnostics([mismatch_fixture, pass_fixture + '\r', exact, 'golden pass: truncated']);
	assert.deepEqual(parsed.passing_frames, [
		{frame: 'beta-2', pixels: 128, differing: 1, beyond: 0, allowance: 1, worst: 1},
		{frame: 'beta-2', pixels: 128, differing: 0, beyond: 0, allowance: 1, worst: 0},
	]);
	assert.equal(parsed.frames.length, 1);
	assert.equal(parsed.unparsed_passes, 1);
});

test('failed test exit writes rejected frames and emits diagnostics before rejecting', async (t) => {
	const output = mkdtempSync(join(tmpdir(), 'labrador-safari-pixel-'));
	t.after(() => rmSync(output, { recursive: true, force: true }));
	const lines = [];
	const bytes = Buffer.alloc(0x9012, 0xe3);
	const context = {
		btoa,
		window: { labradorExit: Promise.resolve({code: 1}),
			labradorTestLog: [pass_fixture, mismatch_fixture], labradorComplaints: [] },
		Module: { FS: {
			readdir: (path) => path === '/' ? ['golden-actual'] : ['.', '..', 'alpha.png'],
			readFile: (path) => { assert.equal(path, '/golden-actual/alpha.png'); return bytes; },
		} },
	};
	const session = {
		navigate: async () => {},
		set_timeouts: async () => {},
		execute_async: async (script, [expression]) => run_evaluate_script(expression, context),
	};
	await assert.rejects(drive_test(session, 'http://fixture', {
		pixel_output: output, log: line => lines.push(line),
	}),
		/Test page failed/);
	const evidence = lines.find(line => line.startsWith('Safari pixel diagnostics: '));
	const parsed = JSON.parse(evidence.slice('Safari pixel diagnostics: '.length));
	assert.equal(parsed.frames[0].worst, 9);
	assert.equal(parsed.passing_frames[0].worst, 1);
	assert.ok(lines.some(line => line.includes(mismatch_fixture)));
	assert.deepEqual(readFileSync(join(output, 'alpha.png')), bytes);
});
