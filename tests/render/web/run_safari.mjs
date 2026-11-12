// The six development sample pages and LineSweeper's bundle page in desktop
// Safari, through safaridriver's classic W3C WebDriver (web-to-be-tested.md,
// "Needs Safari"). CI's `safari` job runs it on macos-latest against the
// pages the `wasm` job built; it is not a ctest entry, because no Windows
// configure has a Safari to find.
//
// node tests/render/web/run_safari.mjs --pages <build>/samples --sample minimal
//   [--timeout 60] [--frames 60]
// node tests/render/web/run_safari.mjs --bundle <build>/web-dist/linesweeper [--timeout 120]
// node tests/render/web/run_safari.mjs --renderer
//
// --pages names a folder holding <sample>/<Target>.{html,js,wasm}: a build's
// samples/ folder, or the artifact CI uploads from it. A sample run waits for
// real WebGL2 clears and draws, forces a context loss through
// WEBGL_lose_context and requires draws to resume after restoration, presses
// the sample's quit keys as trusted WebDriver key actions and waits for
// Module.onQuit. --bundle runs bundle_page.mjs's desktop play, forced context
// loss and restore, quit and replay, as the Chromium and Firefox entries do.
// --renderer prints Safari's masked RENDERER and the unmasked string
// WEBGL_debug_renderer_info gives.
//
// Classic WebDriver has no preload script and safaridriver keeps no console
// log, so the runner's own server puts the hooks in a <script> at the top of
// each page's <head>, ahead of the page's own scripts. A console.warn or
// console.error call, an uncaught error or an unhandled rejection in the page
// fails the run; a warning Safari prints itself, without a console call, is
// not seen.
//
// --driver and --browser-name exist so the protocol can be checked where
// there is no Safari: `--driver geckodriver --browser-name firefox` drives a
// headless Firefox the same way. That checks this runner, not Safari.
//
// The WebDriver client, the driver's start and its end are the website
// acceptance suite's (website/acceptance/webdriver.mjs and browsers.mjs), so
// hardening made there for the acceptance suite's Safari and iOS runs reaches
// this runner too.
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { driverArguments, killDriver, safariTabsToLinks, startDriver, targetCapabilities }
	from '../../../website/acceptance/browsers.mjs';
import { WebDriverError, command } from '../../../website/acceptance/webdriver.mjs';
import { BUNDLE_FILES, PRELUDE, READY, bundle_state, bundle_substitute, steps } from './bundle_page.mjs';
import { SAMPLES, WEBDRIVER_OPTION, close_server, deadline_after, frames_expression,
	key_actions as webdriver_key_actions, key_table, module_hook, serve_files } from './page_harness.mjs';

export { SAMPLES };

// The keys a sample run and the bundle's steps press.
const KEY_VALUES = key_table(['Escape', 'Enter', 'Tab', 'KeyS', 'KeyR', 'KeyA',
	'ArrowLeft', 'ArrowUp', 'ArrowRight', 'ArrowDown']);

export function parse_options(arguments_) {
	const options = {};
	const valued = new Set(['pages', 'sample', 'bundle', 'timeout', 'frames', 'driver', 'browser-name']);
	for (let index = 0; index < arguments_.length; index++) {
		const name = arguments_[index].replace(/^--/, '');
		if (!arguments_[index].startsWith('--') || Object.hasOwn(options, name)) {
			throw new Error(`Invalid or duplicate argument ${arguments_[index]}.`);
		}
		if (name === 'renderer') { options.renderer = true; continue; }
		if (!valued.has(name) || index + 1 >= arguments_.length) {
			throw new Error(`Invalid or duplicate argument ${arguments_[index]}.`);
		}
		options[name] = arguments_[++index];
	}
	const modes = [options.sample !== undefined, options.bundle !== undefined, options.renderer === true];
	const valid = modes.filter(Boolean).length === 1 && (
		options.sample !== undefined ? Object.hasOwn(SAMPLES, options.sample) && options.pages !== undefined :
		options.bundle !== undefined ? options.bundle !== '' && options.pages === undefined && options.frames === undefined :
		options.pages === undefined && options.frames === undefined);
	const browser_name = options['browser-name'] ?? 'safari';
	if (!valid || !['safari', 'firefox'].includes(browser_name)) {
		throw new Error(`One of: --pages <samples folder> with --sample (${Object.keys(SAMPLES).join('/')}); ` +
			'--bundle <web-dist/linesweeper folder> (without --pages or --frames); or --renderer. ' +
			'--browser-name is safari (the default) or firefox.');
	}
	options.browser_name = browser_name;
	options.label = browser_name === 'safari' ? 'Safari' : 'Firefox through geckodriver';
	options.driver ??= browser_name === 'safari' ? 'safaridriver' : 'geckodriver';
	options.timeout_ms = Number(options.timeout ?? (options.bundle !== undefined ? 120 : 60)) * 1000;
	options.frames = Number(options.frames ?? 60);
	if (!Number.isFinite(options.timeout_ms) || options.timeout_ms < 1000 || options.timeout_ms > 600000 ||
		!Number.isInteger(options.frames) || options.frames < 1 || options.frames > 36000) {
		throw new Error('Timeout must be 1-600 seconds; frames must be an integer in 1-36000.');
	}
	return options;
}

// ---------------------------------------------------------------------------
// The classic W3C WebDriver commands this runner uses, over the acceptance
// suite's client. https://www.w3.org/TR/webdriver2/
export { WebDriverError };
export const webdriver_command = command;

export function session_commands(base, session_id, send = command) {
	const at = `/session/${session_id}`;
	return {
		id: session_id,
		navigate: (url, timeout_ms = 60000) => send(base, 'POST', `${at}/url`, { url }, { timeoutMs: timeout_ms }),
		// The body runs as a function; its last argument is the callback.
		execute_async: (script, args = [], timeout_ms = 30000) =>
			send(base, 'POST', `${at}/execute/async`, { script, args }, { timeoutMs: timeout_ms + 5000 }),
		set_timeouts: (timeouts) => send(base, 'POST', `${at}/timeouts`, timeouts),
		perform: (actions) => send(base, 'POST', `${at}/actions`, { actions }),
		release: () => send(base, 'DELETE', `${at}/actions`),
		close: () => send(base, 'DELETE', at, undefined, { timeoutMs: 10000 }),
	};
}

// Runs `expression` in the page, awaits it, and returns its value through
// JSON. A thrown error or a rejection comes back as an error naming it.
export const EVALUATE_SCRIPT = `const done = arguments[arguments.length - 1];
(async () => JSON.stringify({ value: await (0, eval)(arguments[0]) }))().then(done,
	(error) => done(JSON.stringify({ error: String(error && error.stack || error) })));`;

export function evaluator(session, remaining) {
	return async (expression) => {
		const timeout_ms = remaining();
		await session.set_timeouts({ script: timeout_ms });
		const text = await session.execute_async(EVALUATE_SCRIPT, [`(${expression})`], timeout_ms);
		const result = JSON.parse(text);
		if (Object.hasOwn(result, 'error')) throw new Error(`page evaluation failed: ${result.error}`);
		return result.value;
	};
}

// ---------------------------------------------------------------------------
// What the server puts at the top of each page's <head>.

// Complaints the page makes through its own console or as uncaught errors.
export const COMPLAINT_HOOK = `(() => {
	const complaints = [];
	window.labradorComplaints = complaints;
	for (const level of ['warn', 'error']) {
		const original = console[level];
		console[level] = function (...args) {
			complaints.push(level + ': ' + args.map((value) => String(value)).join(' '));
			return original.apply(this, args);
		};
	}
	addEventListener('error', (event) => complaints.push('uncaught: ' + (event.message || String(event.error))));
	addEventListener('unhandledrejection', (event) => complaints.push('unhandled rejection: ' + String(event.reason)));
})();`;

// The sample page's Module callbacks, its real WebGL2 submissions on the
// module's canvas, its context events and the renderer strings it drew with.
export const SAMPLE_HOOK = `(() => {
	const heard = { initialized: false, quit: false, errors: [], has_quit: false,
		clears: 0, draw_calls: 0, elements: 0, lost: 0, restored: 0,
		raw_api: null, raw_renderer: null, unmasked_renderer: null,
		frames: 0, longest_frame_gap_ms: 0, last_frame_ms: null, keys: [], logs: [], audio: [], focus: [] };
	window.labradorSafari = heard;
	${module_hook('labradorSafariHooked')}
	// What a failed run reports: whether frames kept coming and how far apart,
	// which key events the page received, and the page's last console lines.
	if (typeof requestAnimationFrame === 'function') {
		const request = requestAnimationFrame;
		window.requestAnimationFrame = function (callback) {
			return request.call(window, (time) => {
				++heard.frames;
				if (heard.last_frame_ms !== null) {
					heard.longest_frame_gap_ms = Math.max(heard.longest_frame_gap_ms, time - heard.last_frame_ms);
				}
				heard.last_frame_ms = time;
				return callback(time);
			});
		};
	}
	// Each AudioContext's state changes, with the page time of each.
	if (typeof AudioContext === 'function') {
		const Original = AudioContext;
		window.AudioContext = function (...args) {
			const context = new Original(...args);
			const index = heard.audio.length;
			heard.audio.push([context.state + '@' + Math.round(performance.now())]);
			context.addEventListener('statechange', () => {
				if (heard.audio[index].length < 16) heard.audio[index].push(context.state + '@' + Math.round(performance.now()));
			});
			return context;
		};
		window.AudioContext.prototype = Original.prototype;
	}
	// The window's focus and the document's visibility as they change, with
	// the page time of each: the engine drops key edges unless the window is
	// focused on both frames, and clears held keys on a blur.
	if (typeof addEventListener === 'function') {
		for (const type of ['blur', 'focus']) {
			addEventListener(type, (event) => {
				if (event.target === window && heard.focus.length < 64) heard.focus.push(type + '@' + Math.round(performance.now()));
			}, true);
		}
		addEventListener('visibilitychange', () => {
			if (heard.focus.length < 64) heard.focus.push(document.visibilityState + '@' + Math.round(performance.now()));
		}, true);
	}
	if (typeof addEventListener === 'function') {
		for (const type of ['keydown', 'keyup']) {
			addEventListener(type, (event) => {
				if (heard.keys.length < 256) heard.keys.push((type === 'keydown' ? '+' : '-') + event.code + '@' + Math.round(performance.now()));
			}, true);
		}
	}
	if (typeof console === 'object' && typeof console.log === 'function') {
		const log = console.log;
		console.log = function (...args) {
			heard.logs.push(args.map((value) => String(value)).join(' ').slice(0, 200));
			if (heard.logs.length > 8) heard.logs.shift();
			return log.apply(this, args);
		};
	}
	let watched = null;
	const watch = (canvas) => {
		if (watched === canvas) return;
		watched = canvas;
		canvas.addEventListener('webglcontextlost', () => ++heard.lost);
		canvas.addEventListener('webglcontextrestored', () => ++heard.restored);
	};
	const clear = WebGL2RenderingContext.prototype.clear;
	WebGL2RenderingContext.prototype.clear = function (...args) {
		const result = clear.apply(this, args);
		if (this.canvas === module?.canvas && !this.isContextLost()) { watch(this.canvas); ++heard.clears; }
		return result;
	};
	const draw = WebGL2RenderingContext.prototype.drawElements;
	WebGL2RenderingContext.prototype.drawElements = function (...args) {
		const result = draw.apply(this, args);
		if (this.canvas === module?.canvas && !this.isContextLost()) {
			watch(this.canvas);
			++heard.draw_calls;
			heard.elements += args[1];
			if (heard.raw_api === null) {
				heard.raw_api = this.getParameter(this.VERSION);
				heard.raw_renderer = this.getParameter(this.RENDERER);
				const info = this.getExtension('WEBGL_debug_renderer_info');
				heard.unmasked_renderer = info ? this.getParameter(info.UNMASKED_RENDERER_WEBGL) : null;
			}
		}
		return result;
	};
})();`;

export function inject_head(html, scripts) {
	const head = /<head(\s[^>]*)?>/i.exec(html);
	if (!head) throw new Error('The page has no <head> to put the test hooks in.');
	const at = head.index + head[0].length;
	const tags = scripts.map((script) => `\n<script>${script}</script>`).join('');
	return html.slice(0, at) + tags + html.slice(at);
}

// ---------------------------------------------------------------------------
// Input.

export function key_actions(keys, { hold_ms = 150 } = {}) {
	return webdriver_key_actions(keys, { table: KEY_VALUES, hold_ms });
}

// Whether desktop Safari's Tab moves to links and buttons: off by default,
// when Option-Tab does instead. The acceptance suite's reading of Safari's
// preferences and of LABRADOR_SAFARI_TAB_TO_LINKS.
export const safari_tabs_to_links = safariTabsToLinks;

// bundle_page.mjs's steps speak DevTools: these are its mouse, key and
// navigation calls as WebDriver actions. Any other method refuses rather
// than being skipped. `option_tab` holds Option around Tab, which is how
// Safari's default preferences reach a button or link.
export function bundle_input(session, evaluate, { option_tab = false, pause = wait } = {}) {
	const mouse = (button_action, { x, y }) => session.perform([{ type: 'pointer', id: 'mouse',
		parameters: { pointerType: 'mouse' }, actions: [
			{ type: 'pointerMove', x: Math.round(x), y: Math.round(y), origin: 'viewport' },
			{ type: button_action, button: 0 }] }]);
	const key = (key_action, code) => {
		if (!Object.hasOwn(KEY_VALUES, code)) throw new Error(`Unsupported bundle key ${code}.`);
		const value = KEY_VALUES[code];
		const actions = code === 'Tab' && option_tab ?
			(key_action === 'keyDown' ? [{ type: 'keyDown', value: WEBDRIVER_OPTION }, { type: 'keyDown', value }] :
				[{ type: 'keyUp', value }, { type: 'keyUp', value: WEBDRIVER_OPTION }]) :
			[{ type: key_action, value }];
		return session.perform([{ type: 'key', id: 'keyboard', actions }]);
	};
	const send = async (method, params) => {
		if (method === 'Input.dispatchMouseEvent' && params.type === 'mousePressed') return mouse('pointerDown', params);
		if (method === 'Input.dispatchMouseEvent' && params.type === 'mouseReleased') return mouse('pointerUp', params);
		if (method === 'Input.dispatchKeyEvent' && (params.type === 'rawKeyDown' || params.type === 'keyDown')) {
			return key('keyDown', params.code);
		}
		if (method === 'Input.dispatchKeyEvent' && params.type === 'keyUp') return key('keyUp', params.code);
		if (method === 'Page.navigate') return session.navigate(params.url);
		throw new Error(`${method} has no WebDriver translation in the Safari bundle run.`);
	};
	return { evaluate, send, wait: pause };
}

// ---------------------------------------------------------------------------
// The runs. Each takes a session, so the control fixtures drive them without
// a browser.

const wait = (milliseconds) => new Promise((done) => setTimeout(done, milliseconds));

async function complaints_of(evaluate) {
	const complaints = await evaluate('window.labradorComplaints ?? []');
	if (complaints.length > 0) throw new Error(`Page complaints: ${complaints.join('; ')}`);
}

// How long each key level is held before the next action: at least this many
// of the page's animation frames, and at least this long.
const KEY_HOLD_FRAMES = 3;
const KEY_HOLD_MS = 100;

// Sends one key level and waits for the page's key log to show it, sending it
// again (after releasing the keyboard, for a press) when the page reports
// nothing. The log is the sample hook's, which records the event's code; the
// match is on press or release only, because Safari reports WebDriver's Enter
// as NumpadEnter.
async function send_key(session, evaluate, key, type, { pause, log, label }) {
	const mark = type === 'keyDown' ? '+' : '-';
	for (let attempt = 1; ; attempt++) {
		const before = await evaluate('window.labradorSafari.keys.length');
		await session.perform([{ type: 'key', id: 'keyboard', actions: [{ type, value: KEY_VALUES[key] }] }]);
		const heard_by = Date.now() + KEY_HEARD_MS;
		while (Date.now() < heard_by) {
			const since = await evaluate(`window.labradorSafari.keys.slice(${before})`);
			if (since.some((entry) => entry.startsWith(mark))) return;
			await pause(25);
		}
		if (attempt >= KEY_ATTEMPTS) {
			throw new Error(`The page never received ${key}'s ${type} in ${KEY_ATTEMPTS} attempts.`);
		}
		log(`${label}: the page did not receive ${key}'s ${type}; sending it again (attempt ${attempt + 1}).`);
		if (type === 'keyDown') {
			await session.release();
		} else {
			// The driver counts the key as released already, so a lone keyUp
			// would dispatch nothing: press it again first. The page, which never
			// heard the release, sees a repeated press, which adds none.
			await session.perform([{ type: 'key', id: 'keyboard', actions: [{ type: 'keyDown', value: KEY_VALUES[key] }] }]);
		}
	}
}

// How long a page has to report one key level, and how many sends it gets.
const KEY_HEARD_MS = 2000;
const KEY_ATTEMPTS = 3;

// An expression that settles once `frames` animation frames have run and
// `ms` milliseconds have passed, whichever is later.
export function held_expression(frames, ms) {
	return `new Promise((done) => {
		const until = performance.now() + ${ms};
		let left = ${frames};
		const next = (time) => --left <= 0 && time >= until ? done(true) : requestAnimationFrame(next);
		requestAnimationFrame(next);
	})`;
}

export async function drive_sample(session, url, sample,
	{ timeout_ms = 60000, frames = 60, pause = wait, log = console.log, label = 'Safari', progress = {} } = {}) {
	const evaluate = evaluator(session, deadline_after(timeout_ms, `${label} sample`, 15000));
	progress.phase = 'loading the page';
	const state = async () => {
		await complaints_of(evaluate);
		const heard = await evaluate('window.labradorSafari ?? null');
		if (heard?.errors.length > 0) throw new Error(`Module.onError: ${heard.errors.join('; ')}`);
		return heard;
	};
	const until = async (test) => {
		let heard = await state();
		while (!test(heard)) { await pause(25); heard = await state(); }
		return heard;
	};
	await session.navigate(url);
	let heard = await until((value) => value?.initialized);
	progress.phase = 'waiting for the first draw';
	if (!heard.has_quit) throw new Error('The sample supplied no Module.onQuit callback.');
	const first = await until((value) => value.draw_calls > 0 && value.elements > 0 && value.clears > 0);
	progress.phase = 'drawing the first frames';
	await evaluate(frames_expression(frames));
	heard = await state();
	if (heard.clears <= first.clears || heard.draw_calls <= first.draw_calls) {
		throw new Error('The sample did not continue clearing and submitting real WebGL draws.');
	}

	// A forced loss and its restoration: the engine hears both, and draws
	// on the new context. Draws on a lost context are not counted.
	progress.phase = 'losing the context';
	await evaluate(`(() => {
		const loss = Module.canvas.getContext('webgl2').getExtension('WEBGL_lose_context');
		if (!loss) { throw new Error('WEBGL_lose_context is unavailable'); }
		window.labradorSafariLoss = loss;
		loss.loseContext();
	})()`);
	heard = await until((value) => value.lost === 1);
	await evaluate(frames_expression(5));
	progress.phase = 'restoring the context';
	await evaluate('window.labradorSafariLoss.restoreContext()');
	const restored = await until((value) => value.restored === 1);
	await evaluate(frames_expression(frames));
	heard = await until((value) => value.draw_calls > restored.draw_calls + 3 && value.clears > restored.clears + 3);
	if (heard.quit) throw new Error('The sample quit before trusted key input.');

	// Keys reach the sample only while its canvas has the keyboard, which the
	// shell gives it at start-up; keys sent while it does not are lost and the
	// sample never quits. The page's focus is reported if the quit is missing.
	// Read with its own allowance, so it can still explain a run whose
	// deadline has expired.
	const focus_state = () => evaluator(session, () => 15000)(`JSON.stringify({
		focused: document.activeElement?.tagName ?? null, page_focused: document.hasFocus(),
		visibility: document.visibilityState })`);
	const canvas_has_keyboard = 'document.hasFocus() && document.activeElement === Module.canvas';
	progress.phase = 'giving the canvas the keyboard';
	const focus_by = Date.now() + 10000;
	while (!(await evaluate(canvas_has_keyboard))) {
		if (Date.now() > focus_by) throw new Error(`The sample's canvas never took the keyboard; page ${await focus_state()}`);
		await pause(50);
	}
	// Two ways of sending the keys, each chosen by what CI's hosted Mac did
	// (REV#159, docs/review/2026-11-12-work-audit/STATUS.md):
	// - Each press and each release alone, sent until the page reports it,
	//   then held for frames past it. One action list held a fixed time lets
	//   Safari stretch an arrow key's hold to about 550 ms, past the 400 ms
	//   before a held menu direction repeats, so LineSweeper's pause menu
	//   moved two rows a press; this form passed LineSweeper in every run.
	// - The audio sample's keys as one action list: once its first key starts
	//   Web Audio, keys sent in separate actions went missing, three runs of
	//   three in two forms, while one list passed three of three. Its keys
	//   move no menu, so a stretched hold costs nothing there.
	// Every resend is logged, so none is silent.
	progress.phase = 'pressing the quit keys';
	if (sample === SAMPLES.audio) {
		await session.perform(key_actions(sample.keys));
	} else {
		for (const key of sample.keys) {
			for (const type of ['keyDown', 'keyUp']) {
				await send_key(session, evaluate, key, type, { pause, log, label });
				await evaluate(held_expression(KEY_HOLD_FRAMES, KEY_HOLD_MS));
			}
		}
	}
	await session.release();
	progress.phase = 'waiting for the quit';
	try {
		heard = await until((value) => value.quit);
	} catch (error) {
		throw new Error(`${error.message} The keys did not quit the sample; page ${await focus_state().catch(() => 'unreadable')}`);
	}
	progress.phase = 'drawing after the quit';
	await evaluate(frames_expression(10));
	heard = await state();
	log(`${label} sample: ${heard.clears} clears, ${heard.draw_calls} drawElements calls, ` +
		`${heard.elements} submitted elements; context lost ${heard.lost} and restored ${heard.restored} times; ` +
		`onQuit observed. VERSION=${heard.raw_api}; RENDERER=${heard.raw_renderer}; ` +
		`unmasked=${heard.unmasked_renderer ?? '(no WEBGL_debug_renderer_info)'}`);
	return heard;
}

export async function drive_bundle(session, url,
	{ timeout_ms = 120000, pause = wait, log = console.log, option_tab = false, label = 'Safari' } = {}) {
	const page_evaluate = evaluator(session, deadline_after(timeout_ms, `${label} bundle`, 15000));
	const evaluate = async (expression) => {
		await complaints_of(page_evaluate);
		const value = await page_evaluate(expression);
		await complaints_of(page_evaluate);
		return value;
	};
	await session.navigate(url);
	const state = bundle_state();
	state.phase = 'play and replay';
	const page = bundle_input(session, evaluate, { option_tab, pause });
	const { until, navigate, desktop_instructions, play_restore_replay } = steps(page, state);
	await until(READY, 'the bundle page did not settle');
	await navigate('desktop');
	await desktop_instructions();
	await play_restore_replay();
	await session.release();
	await complaints_of(page_evaluate);
	log(`${label} bundle: desktop play, forced context loss and restore, quit and replay passed. ` +
		'Not run here: the touch and controller gates, unsupported-browser and failure pages, ' +
		'a real GPU loss, a real tab switch and repeated-play memory, which Chromium runs.');
}

export const RENDERER_EXPRESSION = `(() => {
	const gl = document.createElement('canvas').getContext('webgl2');
	if (!gl) { return { webgl2: false }; }
	const info = gl.getExtension('WEBGL_debug_renderer_info');
	return { webgl2: true, version: gl.getParameter(gl.VERSION), renderer: gl.getParameter(gl.RENDERER),
		vendor: gl.getParameter(gl.VENDOR),
		unmasked_renderer: info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL) : null,
		unmasked_vendor: info ? gl.getParameter(info.UNMASKED_VENDOR_WEBGL) : null,
		user_agent: navigator.userAgent };
})()`;

export async function report_renderer(session, url, { timeout_ms = 60000, log = console.log, label = 'Safari' } = {}) {
	const evaluate = evaluator(session, deadline_after(timeout_ms, `${label} renderer`, 15000));
	await session.navigate(url);
	const found = await evaluate(RENDERER_EXPRESSION);
	log(`${label} WebGL2: ${JSON.stringify(found)}`);
	if (!found.webgl2) throw new Error(`${label} created no WebGL2 context.`);
	return found;
}

// ---------------------------------------------------------------------------
// Serving, the driver and the session.

export function artifact_files(options) {
	if (options.renderer) return [];
	if (options.bundle !== undefined) return BUNDLE_FILES.map((name) => join(resolve(options.bundle), name));
	const directory = resolve(options.pages, options.sample);
	const target = SAMPLES[options.sample].target;
	return ['html', 'js', 'wasm'].map((extension) => join(directory, `${target}.${extension}`));
}

// The page's hooks, per mode.
export function page_scripts(options) {
	return options.bundle !== undefined ? [COMPLAINT_HOOK, PRELUDE + ';'] : [COMPLAINT_HOOK, SAMPLE_HOOK];
}

// The bundle's loader, asked for without a query, is bundle_page.mjs's
// wrapper; the wrapper's own import of it, with the query, is the real file.
// The renderer run's page is an empty document at the root.
export function substitute(options, url) {
	if (options.bundle !== undefined) return bundle_substitute(url);
	if (options.renderer && url.pathname === '/') {
		return { type: 'text/html; charset=utf-8', body: Buffer.from('<!doctype html><title>renderer</title>') };
	}
	return null;
}

export function driver_arguments(browser_name, port) {
	return driverArguments(browser_name, port);
}

export function capabilities(browser_name) {
	return targetCapabilities(browser_name);
}

// The acceptance suite's start: a port race is retried, readiness is /status
// answering ready, and a driver that never gets there is ended before the
// error is thrown.
async function start_driver(options) {
	const started = await startDriver(options.driver, options.browser_name, { log: (line) => console.log(line) });
	return { base: started.url, output: started.output, stop: () => killDriver(started.child) };
}

async function new_session(base, browser_name) {
	// safaridriver refuses a second session while one is open, and a session
	// it was still closing counts; a few retries cover that.
	for (let attempt = 1; ; attempt++) {
		try {
			const created = await command(base, 'POST', '/session',
				{ capabilities: { alwaysMatch: capabilities(browser_name) } }, { timeoutMs: 120000 });
			return { created, session: session_commands(base, created.sessionId) };
		} catch (error) {
			if (attempt >= 3 || error.code !== 'session not created') throw error;
			console.error(`Session not created (attempt ${attempt}): ${error.message}`);
			await wait(3000);
		}
	}
}

export async function main(options) {
	const files = artifact_files(options);
	for (const file of files) {
		let bytes;
		try { bytes = readFileSync(file); }
		catch (error) { throw new Error(`${file}: ${error.code ?? error.name}: ${error.message}`); }
		console.log(`${options.label} artifact ${file}: ${bytes.length} bytes, sha256=${createHash('sha256').update(bytes).digest('hex')}`);
	}
	let service;
	let driver;
	let session;
	let failure = null;
	const progress = { phase: 'starting' };
	try {
		const scripts = page_scripts(options);
		service = await serve_files(files, { substitute: (url) => substitute(options, url),
			transform: (html) => inject_head(html, scripts), page: options.renderer ? '' : undefined });
		driver = await start_driver(options);
		const opened = await new_session(driver.base, options.browser_name);
		session = opened.session;
		const { browserName, browserVersion, platformName } = opened.created.capabilities ?? {};
		console.log(`Session ${session.id}: ${browserName} ${browserVersion} on ${platformName}`);
		const option_tab = options.browser_name === 'safari' && !(await safari_tabs_to_links());
		const driven = options.renderer ? report_renderer(session, service.url, options) :
			options.bundle !== undefined ? drive_bundle(session, service.url, { ...options, option_tab }) :
				drive_sample(session, service.url, SAMPLES[options.sample], { ...options, progress });
		let timer;
		try {
			await Promise.race([service.failed, driven, new Promise((_, refuse) => {
				timer = setTimeout(() => refuse(new Error(`${options.label} page timed out.`)), options.timeout_ms);
			})]);
		} finally { clearTimeout(timer); }
		const { label } = options;
		console.log(options.renderer ? `${label} renderer: reported.` :
			options.bundle !== undefined ? `${label} bundle: page protocol passed. No listening or physical-device acceptance.` :
				`${label} ${options.sample}: page protocol passed. No WebWindow, listening or physical-device acceptance.`);
	} catch (error) {
		failure = error;
		if (session && !options.renderer && options.bundle === undefined) {
			console.error(`last phase: ${progress.phase}`);
			const page = await evaluator(session, () => 10000)(`JSON.stringify({ heard: window.labradorSafari ?? null,
				now_ms: performance.now(), visibility: document.visibilityState,
				focused: document.activeElement?.tagName ?? null, page_focused: document.hasFocus() })`)
				.catch((reason) => `unreadable (${reason.message})`);
			console.error(`page state: ${page}`);
		}
	} finally {
		if (session) {
			try { await session.close(); } catch (error) { console.error(`Session close: ${error.message}`); }
		}
		if (driver) {
			await driver.stop();
			if (failure) console.error(`last driver output (8,192 characters maximum):\n${driver.output() || '(empty)'}`);
		}
		if (service) await close_server(service.server);
	}
	if (failure) throw failure;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	// A command line it cannot run exits 2 with the reason; a run that
	// fails exits 1 with its stack.
	let options = null;
	try { options = parse_options(process.argv.slice(2)); }
	catch (error) { console.error(`run_safari: ${error.message}`); process.exitCode = 2; }
	if (options) {
		try { await main(options); }
		catch (error) { console.error(`run_safari: ${error.stack ?? error}`); process.exitCode = 1; }
	}
}
