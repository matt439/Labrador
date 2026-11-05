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
import { execFile, spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { createServer as createNetServer } from 'node:net';
import { basename, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import { FACTORY_WRAPPER, PRELUDE, READY, bundle_state, steps } from './bundle_page.mjs';
import { SAMPLES } from './run_firefox.mjs';

export { SAMPLES };

const KEY_VALUES = Object.freeze({ Escape: '', Enter: '', Tab: '', KeyS: 's', KeyR: 'r', KeyA: 'a',
	ArrowLeft: '', ArrowUp: '', ArrowRight: '', ArrowDown: '' });
const OPTION_KEY = '';

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
// A classic W3C WebDriver session over HTTP: only the commands this runner
// uses. https://www.w3.org/TR/webdriver2/
export class WebDriverError extends Error {
	constructor(code, message) {
		super(`${code}: ${message}`);
		this.code = code;
	}
}

export async function webdriver_command(base, method, path, body, { timeout_ms = 30000, fetch_ = fetch } = {}) {
	let response;
	try {
		response = await fetch_(`${base}${path}`, {
			method,
			headers: body === undefined ? {} : { 'Content-Type': 'application/json; charset=utf-8' },
			body: body === undefined ? undefined : JSON.stringify(body),
			signal: AbortSignal.timeout(timeout_ms),
		});
	} catch (error) {
		throw new WebDriverError(error.name === 'TimeoutError' ? 'client timeout' : 'connection failed',
			`${method} ${path}: ${error.message}`);
	}
	const text = await response.text();
	let parsed;
	try { parsed = JSON.parse(text); }
	catch { throw new WebDriverError('invalid response', `${method} ${path}: HTTP ${response.status}: ${text.slice(0, 200)}`); }
	const value = parsed?.value;
	if (!response.ok || (value && typeof value === 'object' && typeof value.error === 'string')) {
		throw new WebDriverError(value?.error ?? `HTTP ${response.status}`, `${method} ${path}: ${value?.message ?? text.slice(0, 200)}`);
	}
	return value;
}

export function session_commands(base, session_id, command = webdriver_command) {
	const at = `/session/${session_id}`;
	return {
		id: session_id,
		navigate: (url, timeout_ms = 60000) => command(base, 'POST', `${at}/url`, { url }, { timeout_ms }),
		// The body runs as a function; its last argument is the callback.
		execute_async: (script, args = [], timeout_ms = 30000) =>
			command(base, 'POST', `${at}/execute/async`, { script, args }, { timeout_ms: timeout_ms + 5000 }),
		set_timeouts: (timeouts) => command(base, 'POST', `${at}/timeouts`, timeouts),
		perform: (actions) => command(base, 'POST', `${at}/actions`, { actions }),
		release: () => command(base, 'DELETE', `${at}/actions`),
		close: () => command(base, 'DELETE', at, undefined, { timeout_ms: 10000 }),
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
		raw_api: null, raw_renderer: null, unmasked_renderer: null };
	window.labradorSafari = heard;
	let module;
	Object.defineProperty(window, 'Module', {
		configurable: true, enumerable: true,
		get() { return module; },
		set(value) {
			module = value;
			if (!value || typeof value !== 'object' || value.labradorSafariHooked) return;
			Object.defineProperty(value, 'labradorSafariHooked', { value: true });
			for (const [name, flag] of [['onRuntimeInitialized', 'initialized'], ['onQuit', 'quit']]) {
				const original = value[name];
				if (name === 'onQuit') {
					heard.has_quit = typeof original === 'function';
					if (!heard.has_quit) continue;
				}
				value[name] = function (...args) {
					const result = typeof original === 'function' ? original.apply(this, args) : undefined;
					heard[flag] = true;
					return result;
				};
			}
			const original = value.onError;
			value.onError = function (message, ...args) {
				heard.errors.push(String(message));
				if (typeof original === 'function') return original.call(this, message, ...args);
			};
		}
	});
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
	return [{ type: 'key', id: 'keyboard', actions: keys.flatMap((key) => {
		if (!Object.hasOwn(KEY_VALUES, key)) throw new Error(`Unsupported sample key ${key}.`);
		return [{ type: 'keyDown', value: KEY_VALUES[key] }, { type: 'pause', duration: hold_ms },
			{ type: 'keyUp', value: KEY_VALUES[key] }, { type: 'pause', duration: hold_ms }];
	}) }];
}

// Whether desktop Safari's Tab moves to links and buttons. Off by default,
// when Option-Tab does instead; LABRADOR_SAFARI_TAB_TO_LINKS (1 or 0)
// overrides what the preferences say. The same reading as the website's
// acceptance suite (website/acceptance/browsers.mjs).
export async function safari_tabs_to_links({ env = process.env, read = read_default } = {}) {
	if (env.LABRADOR_SAFARI_TAB_TO_LINKS === '1') return true;
	if (env.LABRADOR_SAFARI_TAB_TO_LINKS === '0') return false;
	const domains = [
		`${env.HOME ?? ''}/Library/Containers/com.apple.Safari/Data/Library/Preferences/com.apple.Safari`,
		'com.apple.Safari',
	];
	const keys = ['WebKitTabToLinksPreferenceKey', 'com.apple.Safari.ContentPageGroupIdentifier.WebKit2TabsToLinks'];
	for (const domain of domains) {
		for (const key of keys) {
			const value = await read(domain, key);
			if (value !== null) return value === '1' || value === 'true';
		}
	}
	return false;
}

async function read_default(domain, key) {
	try {
		const { stdout } = await promisify(execFile)('defaults', ['read', domain, key], { timeout: 10000 });
		return stdout.trim();
	} catch {
		return null;
	}
}

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
			(key_action === 'keyDown' ? [{ type: 'keyDown', value: OPTION_KEY }, { type: 'keyDown', value }] :
				[{ type: 'keyUp', value }, { type: 'keyUp', value: OPTION_KEY }]) :
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

function frames_expression(count) {
	return `new Promise((done) => {
		let left = ${count};
		const next = () => --left <= 0 ? done(true) : requestAnimationFrame(next);
		requestAnimationFrame(next);
	})`;
}

function deadline_after(timeout_ms, what) {
	const deadline = Date.now() + timeout_ms;
	return () => {
		const duration = deadline - Date.now();
		if (duration <= 0) throw new Error(`${what} deadline expired.`);
		return Math.min(duration, 15000);
	};
}

async function complaints_of(evaluate) {
	const complaints = await evaluate('window.labradorComplaints ?? []');
	if (complaints.length > 0) throw new Error(`Page complaints: ${complaints.join('; ')}`);
}

export async function drive_sample(session, url, sample,
	{ timeout_ms = 60000, frames = 60, pause = wait, log = console.log, label = 'Safari' } = {}) {
	const evaluate = evaluator(session, deadline_after(timeout_ms, `${label} sample`));
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
	if (!heard.has_quit) throw new Error('The sample supplied no Module.onQuit callback.');
	const first = await until((value) => value.draw_calls > 0 && value.elements > 0 && value.clears > 0);
	await evaluate(frames_expression(frames));
	heard = await state();
	if (heard.clears <= first.clears || heard.draw_calls <= first.draw_calls) {
		throw new Error('The sample did not continue clearing and submitting real WebGL draws.');
	}

	// A forced loss and its restoration: the engine hears both, and draws
	// on the new context. Draws on a lost context are not counted.
	await evaluate(`(() => {
		const loss = Module.canvas.getContext('webgl2').getExtension('WEBGL_lose_context');
		if (!loss) { throw new Error('WEBGL_lose_context is unavailable'); }
		window.labradorSafariLoss = loss;
		loss.loseContext();
	})()`);
	heard = await until((value) => value.lost === 1);
	await evaluate(frames_expression(5));
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
	const focus_by = Date.now() + 10000;
	while (!(await evaluate(canvas_has_keyboard))) {
		if (Date.now() > focus_by) throw new Error(`The sample's canvas never took the keyboard; page ${await focus_state()}`);
		await pause(50);
	}
	await session.perform(key_actions(sample.keys));
	await session.release();
	try {
		heard = await until((value) => value.quit);
	} catch (error) {
		throw new Error(`${error.message} The keys did not quit the sample; page ${await focus_state().catch(() => 'unreadable')}`);
	}
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
	const page_evaluate = evaluator(session, deadline_after(timeout_ms, `${label} bundle`));
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
	const evaluate = evaluator(session, deadline_after(timeout_ms, `${label} renderer`));
	await session.navigate(url);
	const found = await evaluate(RENDERER_EXPRESSION);
	log(`${label} WebGL2: ${JSON.stringify(found)}`);
	if (!found.webgl2) throw new Error(`${label} created no WebGL2 context.`);
	return found;
}

// ---------------------------------------------------------------------------
// Serving, the driver and the session.

const BUNDLE_FILES = Object.freeze(['index.html', 'linesweeper.js', 'linesweeper.wasm', 'poster.png']);
const CONTENT_TYPES = Object.freeze({ '.wasm': 'application/wasm', '.js': 'text/javascript',
	'.png': 'image/png', '.html': 'text/html; charset=utf-8' });

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
export function substitute(options, url) {
	if (options.bundle !== undefined && url.pathname === '/linesweeper.js' && url.search === '') {
		return { type: 'text/javascript; charset=utf-8', body: Buffer.from(FACTORY_WRAPPER) };
	}
	return null;
}

async function serve(options, files) {
	let fatal;
	const failed = new Promise((_, refuse) => { fatal = refuse; });
	failed.catch(() => {});
	const allowed = new Map(files.map((file) => ['/' + basename(file), file]));
	const scripts = page_scripts(options);
	const server = createServer((request, response) => {
		const requested = new URL(request.url, 'http://127.0.0.1');
		const wanted = requested.pathname;
		if (wanted === '/favicon.ico') { response.writeHead(204).end(); return; }
		if (options.renderer && wanted === '/') {
			response.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
			response.end('<!doctype html><title>renderer</title>');
			return;
		}
		const substituted = substitute(options, requested);
		if (substituted) {
			response.writeHead(200, { 'Cache-Control': 'no-store', 'Content-Type': substituted.type });
			response.end(substituted.body);
			return;
		}
		const file = allowed.get(wanted);
		if (!file) {
			response.writeHead(404).end();
			fatal(new Error(`Unexpected page resource request: ${wanted}`));
			return;
		}
		try {
			let bytes = readFileSync(file);
			const extension = /\.[^.]+$/.exec(file)?.[0] ?? '';
			if (extension === '.html') bytes = Buffer.from(inject_head(bytes.toString('utf8'), scripts));
			response.writeHead(200, { 'Cache-Control': 'no-store',
				'Content-Type': CONTENT_TYPES[extension] ?? 'application/octet-stream' });
			response.end(bytes);
		} catch (error) {
			response.writeHead(500).end();
			fatal(new Error(`${file}: ${error.code ?? error.name}: ${error.message}`));
		}
	});
	await new Promise((ready, refused) => {
		server.once('error', refused);
		server.listen(0, '127.0.0.1', ready);
	});
	const page = options.renderer ? '' : basename(files[0]);
	return { server, failed, url: `http://127.0.0.1:${server.address().port}/${page}` };
}

async function free_port() {
	const probe = createNetServer();
	await new Promise((ready, refused) => { probe.once('error', refused); probe.listen(0, '127.0.0.1', ready); });
	const { port } = probe.address();
	await new Promise((done) => probe.close(done));
	return port;
}

export function driver_arguments(browser_name, port) {
	return browser_name === 'safari' ? ['-p', String(port)] : ['--port', String(port), '--host', '127.0.0.1'];
}

export function capabilities(browser_name) {
	return browser_name === 'safari' ? { browserName: 'safari' } :
		{ browserName: 'firefox', 'moz:firefoxOptions': { args: ['-headless'] } };
}

async function start_driver(options) {
	const port = await free_port();
	const base = `http://127.0.0.1:${port}`;
	const child = spawn(options.driver, driver_arguments(options.browser_name, port),
		{ stdio: ['ignore', 'pipe', 'pipe'], windowsHide: true });
	let output = '';
	const keep = (chunk) => { output = (output + String(chunk)).slice(-16384); };
	child.stdout.on('data', keep);
	child.stderr.on('data', keep);
	let exited = null;
	child.on('error', (error) => { exited = `spawn ${options.driver}: ${error.code ?? error.name}: ${error.message}`; });
	child.on('exit', (code, signal) => { exited ??= `driver exited: code=${code}, signal=${signal ?? 'none'}`; });
	const started = Date.now();
	for (;;) {
		if (exited) throw new Error(`${exited}\n${output}`);
		try {
			const status = await webdriver_command(base, 'GET', '/status', undefined, { timeout_ms: 2000 });
			if (status?.ready !== false) break;
		} catch { /* not listening yet */ }
		if (Date.now() - started > 20000) throw new Error(`${options.driver} did not answer /status in 20 seconds.\n${output}`);
		await wait(200);
	}
	return { base, child, output: () => output,
		stop: async () => {
			if (child.exitCode !== null || child.signalCode !== null) return;
			const gone = new Promise((done) => child.once('exit', done));
			child.kill();
			await Promise.race([gone, wait(5000)]);
		} };
}

async function new_session(base, browser_name) {
	// safaridriver refuses a second session while one is open, and a session
	// it was still closing counts; a few retries cover that.
	for (let attempt = 1; ; attempt++) {
		try {
			const created = await webdriver_command(base, 'POST', '/session',
				{ capabilities: { alwaysMatch: capabilities(browser_name) } }, { timeout_ms: 120000 });
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
	try {
		service = await serve(options, files);
		driver = await start_driver(options);
		const opened = await new_session(driver.base, options.browser_name);
		session = opened.session;
		const { browserName, browserVersion, platformName } = opened.created.capabilities ?? {};
		console.log(`Session ${session.id}: ${browserName} ${browserVersion} on ${platformName}`);
		const option_tab = options.browser_name === 'safari' && !(await safari_tabs_to_links());
		const driven = options.renderer ? report_renderer(session, service.url, options) :
			options.bundle !== undefined ? drive_bundle(session, service.url, { ...options, option_tab }) :
				drive_sample(session, service.url, SAMPLES[options.sample], options);
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
	} finally {
		if (session) {
			try { await session.close(); } catch (error) { console.error(`Session close: ${error.message}`); }
		}
		if (driver) {
			await driver.stop();
			if (failure) console.error(`last driver output (16 KiB maximum):\n${driver.output() || '(empty)'}`);
		}
		if (service) await new Promise((done) => {
			service.server.close(done);
			service.server.closeAllConnections();
		});
	}
	if (failure) throw failure;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try { await main(parse_options(process.argv.slice(2))); }
	catch (error) { console.error(`run_safari: ${error.stack ?? error}`); process.exitCode = 1; }
}
