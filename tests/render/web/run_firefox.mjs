// The six development sample pages, WebWindowTests' page and LineSweeper's
// bundle page in Firefox over WebDriver BiDi, separate from Chromium's runner
// (run_page.mjs). No geckodriver, browser preferences, shared profile or
// GPU/security flags. The root CMakeLists registers each as a ctest entry
// when configure finds a Firefox.
//
// node tests/render/web/run_firefox.mjs --browser <firefox.exe>
//   --build-dir out/build/wasm-release --sample minimal [--timeout 60] [--frames 60]
// node tests/render/web/run_firefox.mjs --browser <firefox.exe>
//   --build-dir out/build/wasm-release --test WebWindowTests [--timeout 60]
// node tests/render/web/run_firefox.mjs --browser <firefox.exe>
//   --bundle out/build/wasm-release/web-dist/linesweeper [--timeout 120]
//
// --test WebWindowTests runs tests/app/web/window_input.mjs's sequence against
// tests/app/web/window_page.cpp's page with window_input below: the same
// steps and expected logs Chromium's run has, through BiDi's trusted input.
//
// --bundle serves the packaged folder and runs bundle_page.mjs's desktop
// play, forced context loss and restore, quit and replay, with the same
// assertions LineSweeperBundlePageTests makes in Chromium. The runner's own
// server stands in for Chromium's response interception, and bundle_input
// below translates the steps' DevTools input calls to BiDi actions.
//
// Protocol/bootstrap sources (Mozilla's current WebDriver documentation):
// https://developer.mozilla.org/en-US/docs/Web/WebDriver/How_to/Create_BiDi_connection
// https://developer.mozilla.org/en-US/docs/Web/WebDriver/Reference/BiDi/Modules/session/new
// https://developer.mozilla.org/en-US/docs/Web/WebDriver/Reference/BiDi/Modules/input/performActions
// https://w3c.github.io/webdriver-bidi/#command-browsingContext-setViewport
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { lstatSync, mkdtempSync, readFileSync, readdirSync, realpathSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { drive_window, expected_complaint as window_expected_complaint } from '../../app/web/window_input.mjs';
import { BUNDLE_FILES, BUNDLE_KEYS, PRELUDE, READY, bundle_state, bundle_substitute, steps } from './bundle_page.mjs';
import { FirefoxProtocol } from './firefox_protocol.mjs';
import { SAMPLES, WEBDRIVER_CONTROL, close_server, deadline_after, exited, frames_expression, key_actions,
	key_table, module_hook, serve_files } from './page_harness.mjs';

export { SAMPLES, bundle_substitute, key_actions };

export function parse_options(arguments_) {
	const options = {};
	const allowed = new Set(['browser', 'build-dir', 'sample', 'test', 'bundle', 'timeout', 'frames']);
	for (let index = 0; index < arguments_.length; index += 2) {
		const name = arguments_[index].replace(/^--/, '');
		if (!arguments_[index].startsWith('--') || !allowed.has(name) ||
			index + 1 >= arguments_.length || Object.hasOwn(options, name)) {
			throw new Error(`Invalid or duplicate argument ${arguments_[index]}.`);
		}
		options[name] = arguments_[index + 1];
	}
	const named = ['sample', 'test', 'bundle'].filter((name) => options[name] !== undefined);
	const page_named = named.length === 1 && (
		named[0] === 'sample' ? Object.hasOwn(SAMPLES, options.sample) && options['build-dir'] !== undefined :
		named[0] === 'test' ? options.test === 'WebWindowTests' && options['build-dir'] !== undefined &&
			options.frames === undefined :
		options.bundle !== '' && options['build-dir'] === undefined && options.frames === undefined);
	if (!options.browser || !page_named) {
		throw new Error('--browser and one of: --build-dir with --sample ' +
			`(${Object.keys(SAMPLES).join('/')}) or --test WebWindowTests (without --frames); ` +
			'or --bundle <web-dist/linesweeper folder> (without --build-dir or --frames).');
	}
	options.timeout_ms = Number(options.timeout ?? 60) * 1000;
	options.frames = Number(options.frames ?? 60);
	if (!Number.isFinite(options.timeout_ms) || options.timeout_ms < 1000 || options.timeout_ms > 600000 ||
		!Number.isInteger(options.frames) || options.frames < 1 || options.frames > 36000) {
		throw new Error('Timeout must be 1-600 seconds; frames must be an integer in 1-36000.');
	}
	return options;
}

export function firefox_arguments(profile) {
	return ['--headless', '--profile', profile, '--remote-debugging-port=0', 'about:blank'];
}

export function bidi_endpoint(text) {
	// stderr chunks can end in the middle of the port or URL. Only a complete
	// announcement line establishes an endpoint.
	const found = /WebDriver BiDi listening on (ws:\/\/127\.0\.0\.1:\d+(?:\/[^\s]*)?)[ \t]*\r?\n/.exec(text);
	if (!found) return null;
	const endpoint = new URL(found[1]);
	if (!Number(endpoint.port)) throw new Error('Firefox reported an invalid BiDi port.');
	endpoint.pathname = '/session';
	endpoint.search = '';
	endpoint.hash = '';
	return endpoint.href;
}

// Executed by script.addPreloadScript in the page's own realm before its
// scripts. Counters observe real submissions, not pixels or GPU completion.
export const SAMPLE_HOOK = `() => {
	const heard = { initialized: false, quit: false, errors: [], has_quit: false,
		clears: 0, draw_calls: 0, elements: 0, raw_api: null, raw_renderer: null };
	window.labradorFirefox = heard;
	${module_hook('labradorFirefoxHooked')}
	const clear = WebGL2RenderingContext.prototype.clear;
	WebGL2RenderingContext.prototype.clear = function (...args) {
		const result = clear.apply(this, args);
		if (this.canvas === module?.canvas) ++heard.clears;
		return result;
	};
	const draw = WebGL2RenderingContext.prototype.drawElements;
	WebGL2RenderingContext.prototype.drawElements = function (...args) {
		const result = draw.apply(this, args);
		if (this.canvas === module?.canvas) {
			++heard.draw_calls;
			heard.elements += args[1];
			if (heard.raw_api === null) {
				heard.raw_api = this.getParameter(this.VERSION);
				heard.raw_renderer = this.getParameter(this.RENDERER);
			}
		}
		return result;
	};
}`;

const wait = (milliseconds) => new Promise((done) => setTimeout(done, milliseconds));

// What each drive below keeps while it runs one page in `context`: its
// deadline, each call of which answers how long one command may take, at
// most 10 s; every entry the tab logs, written to `log`, with each warning
// and error kept as a complaint unless `expected_complaint` recognises it;
// and a protocol failure if the tab itself is destroyed. assert_healthy
// throws the complaints so far; unsubscribe stops listening.
function watched_context(protocol, context, { timeout_ms, log, what, expected_complaint = () => false }) {
	const remaining = deadline_after(timeout_ms, `Firefox ${what}`, 10000);
	const complaints = [];
	const unsubscribe = protocol.on((message) => {
		if (message.method === 'log.entryAdded') {
			const entry = message.params;
			log(`Firefox ${entry.level}: ${entry.text ?? '(no text)'}`);
			if ((entry.level === 'warn' || entry.level === 'error') &&
				expected_complaint({ level: entry.level, text: entry.text }) !== true) {
				complaints.push(`${entry.level}: ${entry.text}`);
			}
		} else if (message.method === 'browsingContext.contextDestroyed' && message.params.context === context) {
			protocol.fail('the tested Firefox context was destroyed');
		}
	});
	const assert_healthy = () => {
		if (complaints.length > 0) throw new Error(`Firefox complaints: ${complaints.join('; ')}`);
	};
	return { remaining, assert_healthy, unsubscribe };
}

// Subscribes to the tab's log and its destruction, installs `preload` to run
// before the page's own scripts, brings the tab forward, sets its device
// pixel ratio when one is given, and loads `url`.
async function open_page(protocol, context, url, preload, { device_pixel_ratio } = {}) {
	await protocol.send('session.subscribe', { events: ['log.entryAdded', 'browsingContext.contextDestroyed'], contexts: [context] });
	await protocol.send('script.addPreloadScript', { functionDeclaration: preload, contexts: [context] });
	await protocol.send('browsingContext.activate', { context });
	if (device_pixel_ratio !== undefined) {
		await protocol.send('browsingContext.setViewport', { context, devicePixelRatio: device_pixel_ratio });
	}
	await protocol.send('browsingContext.navigate', { context, url, wait: 'complete' });
}

// Exported for offline control fixtures; this never launches a browser itself.
export async function drive_sample(protocol, context, url, sample,
	{ timeout_ms = 60000, frames = 60, pause = wait, log = console.log } = {}) {
	const { remaining, assert_healthy, unsubscribe } = watched_context(protocol, context, { timeout_ms, log, what: 'sample' });
	const evaluate = (expression) => protocol.evaluate(context, expression, remaining());
	const state = async () => {
		assert_healthy();
		const heard = JSON.parse(await evaluate('JSON.stringify(window.labradorFirefox ?? null)'));
		assert_healthy();
		if (heard?.errors.length > 0) throw new Error(`Module.onError: ${heard.errors.join('; ')}`);
		return heard;
	};
	try {
		await open_page(protocol, context, url, SAMPLE_HOOK);
		let heard = await state();
		while (!heard?.initialized) { await pause(25); heard = await state(); }
		if (!heard.has_quit) throw new Error('The sample supplied no Module.onQuit callback.');
		while (!(heard.draw_calls > 0 && heard.elements > 0 && heard.clears > 0)) {
			await pause(25); heard = await state();
		}
		const first = heard;
		await evaluate(frames_expression(frames));
		heard = await state();
		if (heard.clears <= first.clears || heard.draw_calls <= first.draw_calls) {
			throw new Error('The sample did not continue clearing and submitting real WebGL draws.');
		}
		if (heard.quit) throw new Error('The sample quit before trusted key input.');
		await protocol.send('input.performActions', { context, actions: key_actions(sample.keys) });
		await protocol.send('input.releaseActions', { context });
		heard = await state();
		while (!heard?.quit) { await pause(25); heard = await state(); }
		await evaluate(frames_expression(10));
		heard = await state();
		log(`Firefox sample: ${heard.clears} clears, ${heard.draw_calls} drawElements calls, ` +
			`${heard.elements} submitted elements; onQuit observed. API=${heard.raw_api}; renderer=${heard.raw_renderer}`);
		return heard;
	} finally {
		unsubscribe();
	}
}

// WebWindowTests' input over BiDi, in the shape window_input.mjs documents.
// The mouse, the wheel, each finger and the keyboard are input sources of
// input.performActions, whose state - a held button, a finger down - lasts
// between calls until it is released, as a DevTools session's does.
const WINDOW_KEYS = key_table(['ArrowLeft', 'Space', 'KeyA', 'Tab']);

export function window_input(protocol, context) {
	const perform = (actions) => protocol.send('input.performActions', { context, actions });
	const mouse = (actions) => perform([{ type: 'pointer', id: 'mouse',
		parameters: { pointerType: 'mouse' }, actions }]);
	const finger = (id, actions) => ({ type: 'pointer', id: `finger${id}`,
		parameters: { pointerType: 'touch' }, actions });
	const fingers = new Set();
	// One finger per tick. Firefox 158 makes one touchmove naming every
	// finger that moved in a tick but a pointermove for only the first of
	// them, and the window listens to pointer events.
	const staggered = (id, index, action) => finger(id,
		[...Array.from({ length: index }, () => ({ type: 'pause', duration: 0 })), action]);
	const key = (name, { control = false } = {}) => {
		if (!Object.hasOwn(WINDOW_KEYS, name)) throw new Error(`Unsupported window key ${name}.`);
		const value = WINDOW_KEYS[name];
		const pressed = [{ type: 'keyDown', value }, { type: 'keyUp', value }];
		return perform([{ type: 'key', id: 'keyboard', actions: control ?
			[{ type: 'keyDown', value: WEBDRIVER_CONTROL }, ...pressed, { type: 'keyUp', value: WEBDRIVER_CONTROL }] : pressed }]);
	};
	const touch = (type, points) => {
		let sources;
		if (type === 'touchStart') {
			sources = points.filter(([, , id]) => !fingers.has(id)).map(([x, y, id]) => {
				fingers.add(id);
				return finger(id, [{ type: 'pointerMove', x, y }, { type: 'pointerDown', button: 0 }]);
			});
		} else if (type === 'touchMove') {
			sources = points.map(([x, y, id], index) => {
				if (!fingers.has(id)) throw new Error(`touchMove of finger ${id}, which is not down.`);
				return staggered(id, index, { type: 'pointerMove', x, y });
			});
		} else if (type === 'touchEnd') {
			const lifting = points.length > 0 ? points.map(([, , id]) => id) : [...fingers];
			sources = lifting.map((id, index) => {
				if (!fingers.delete(id)) throw new Error(`touchEnd of finger ${id}, which is not down.`);
				return staggered(id, index, { type: 'pointerUp', button: 0 });
			});
		} else {
			throw new Error(`WebDriver BiDi has no ${type}.`);
		}
		return perform(sources);
	};
	return {
		// BiDi holds Control by pressing it as a key, and has no action that
		// makes the browser cancel a pointer.
		presses_modifiers: true,
		cancels_touch: false,
		// Firefox puts a synthesised touch on the nearest device pixel of its
		// window, whose scale includes the desktop's text scaling rather than
		// the ratio the page was given, and a touch's pointer event keeps the
		// fraction that leaves: within half a pixel, which can floor to the
		// next one. A mouse event's position is whole and needs no slack.
		touch_slack: 1,
		// drive_window_page has already activated the tab, which makes it the
		// focused one, and set its ratio before the page measured anything.
		prepare: async () => {},
		move: (at) => mouse([{ type: 'pointerMove', x: at.x, y: at.y }]),
		press: (at) => mouse([{ type: 'pointerMove', x: at.x, y: at.y }, { type: 'pointerDown', button: 0 }]),
		release: (at) => mouse([{ type: 'pointerMove', x: at.x, y: at.y }, { type: 'pointerUp', button: 0 }]),
		wheel: (at, delta_x, delta_y) => perform([{ type: 'wheel', id: 'wheel',
			actions: [{ type: 'scroll', x: at.x, y: at.y, deltaX: delta_x, deltaY: delta_y }] }]),
		key,
		device_pixel_ratio: (ratio) => protocol.send('browsingContext.setViewport',
			{ context, devicePixelRatio: ratio ?? 1 }),
		touch_begin: async () => {},
		touch,
		touch_end: async () => {},
	};
}

// Executed by script.addPreloadScript before the shell's own script. The
// shell settles window.labradorExit only when the page fails
// (tests/app/web/shell.html); the settled value is kept where an evaluation
// can read it.
export const WINDOW_HOOK = `() => {
	const heard = { exit: null };
	window.labradorFirefoxWindow = heard;
	let exit;
	Object.defineProperty(window, 'labradorExit', {
		configurable: true, enumerable: true,
		get() { return exit; },
		set(value) {
			exit = value;
			Promise.resolve(value).then((settled) => { heard.exit = JSON.stringify(settled ?? null); },
				(error) => { heard.exit = JSON.stringify({ abort: String(error) }); });
		},
	});
}`;

// Exported for offline control fixtures; `drive` is window_input.mjs's
// drive_window in a real run. A page that ends, a warning or error the tab
// logs, or a destroyed context fails the run while the sequence goes on,
// except the error the sequence's last step causes on purpose, which
// window_input.mjs's expected_complaint names for run_page.mjs too.
export async function drive_window_page(protocol, context, url, drive,
	{ timeout_ms = 60000, pause = wait, log = console.log, expected_complaint = () => false } = {}) {
	const { remaining, assert_healthy, unsubscribe } = watched_context(protocol, context,
		{ timeout_ms, log, what: 'page', expected_complaint });
	const evaluate = (expression, { user_gesture = false } = {}) =>
		protocol.evaluate(context, expression, remaining(), { user_activation: user_gesture });
	const healthy = async () => {
		assert_healthy();
		const exit = await evaluate('window.labradorFirefoxWindow?.exit ?? null');
		if (exit !== null) throw new Error(`The page ended before its input sequence did: ${exit}`);
		assert_healthy();
	};
	let stopped = false;
	try {
		// Headless Firefox takes its device pixel ratio from the desktop's
		// scale, which need not be one.
		await open_page(protocol, context, url, WINDOW_HOOK, { device_pixel_ratio: 1 });
		const watched = (async () => {
			while (!stopped) {
				await healthy();
				await pause(100);
			}
		})();
		watched.catch(() => {});
		try {
			await Promise.race([drive({ evaluate, wait: pause }, window_input(protocol, context)), watched]);
		} finally {
			stopped = true;
		}
		await protocol.send('input.releaseActions', { context });
		await healthy();
	} finally {
		stopped = true;
		unsubscribe();
	}
}

// bundle_page.mjs's steps speak DevTools: they click Play, press keys and
// navigate with Input.dispatchMouseEvent, Input.dispatchKeyEvent and
// Page.navigate, and read whole values back. These are those calls as BiDi
// commands, and an evaluation that returns any JSON value rather than only a
// primitive. Any other DevTools method refuses rather than being skipped.
export function bundle_input(protocol, context, evaluate_primitive, pause = wait) {
	const perform = (actions) => protocol.send('input.performActions', { context, actions });
	const mouse = (button_action, { x, y }) => perform([{ type: 'pointer', id: 'mouse',
		parameters: { pointerType: 'mouse' }, actions: [
			{ type: 'pointerMove', x: Math.round(x), y: Math.round(y) }, { type: button_action, button: 0 }] }]);
	const key = (key_action, code) => {
		if (!Object.hasOwn(BUNDLE_KEYS, code)) throw new Error(`Unsupported bundle key ${code}.`);
		return perform([{ type: 'key', id: 'keyboard', actions: [{ type: key_action, value: BUNDLE_KEYS[code] }] }]);
	};
	const send = async (method, params) => {
		if (method === 'Input.dispatchMouseEvent' && params.type === 'mousePressed') return mouse('pointerDown', params);
		if (method === 'Input.dispatchMouseEvent' && params.type === 'mouseReleased') return mouse('pointerUp', params);
		if (method === 'Input.dispatchKeyEvent' && (params.type === 'rawKeyDown' || params.type === 'keyDown')) {
			return key('keyDown', params.code);
		}
		if (method === 'Input.dispatchKeyEvent' && params.type === 'keyUp') return key('keyUp', params.code);
		if (method === 'Page.navigate') {
			return protocol.send('browsingContext.navigate', { context, url: params.url, wait: 'complete' });
		}
		throw new Error(`${method} has no BiDi translation in the Firefox bundle run.`);
	};
	const evaluate = async (expression) => JSON.parse(await evaluate_primitive(
		`(async () => JSON.stringify({ value: await (${expression}) }))()`)).value;
	return { evaluate, send, wait: pause };
}

// Exported for offline control fixtures. The page is served by main with
// FACTORY_WRAPPER in place of the loader, as Chromium's interception does,
// so the same hooks hear each instance's callbacks.
export async function drive_bundle(protocol, context, url,
	{ timeout_ms = 120000, pause = wait, log = console.log } = {}) {
	const { remaining, assert_healthy, unsubscribe } = watched_context(protocol, context, { timeout_ms, log, what: 'bundle' });
	const evaluate_primitive = async (expression) => {
		assert_healthy();
		const value = await protocol.evaluate(context, expression, remaining());
		assert_healthy();
		return value;
	};
	try {
		await open_page(protocol, context, url, `() => ${PRELUDE}`);
		const state = bundle_state();
		state.phase = 'play and replay';
		const page = bundle_input(protocol, context, evaluate_primitive, pause);
		const { until, navigate, desktop_instructions, play_restore_replay } = steps(page, state);
		await until(READY, 'the bundle page did not settle');
		await navigate('desktop');
		await desktop_instructions();
		await play_restore_replay();
		await protocol.send('input.releaseActions', { context });
		assert_healthy();
		log('Firefox bundle: desktop play, forced context loss and restore, quit and replay passed. ' +
			'Not run here: the touch and controller gates, unsupported-browser and failure pages, ' +
			'a real GPU loss, a real tab switch and repeated-play memory, which Chromium runs.');
	} finally {
		unsubscribe();
	}
}

function artifact_files(options) {
	if (options.bundle !== undefined) return BUNDLE_FILES.map((name) => join(resolve(options.bundle), name));
	const [directory, target] = options.test !== undefined ?
		[resolve(options['build-dir'], 'tests', 'app'), options.test] :
		[resolve(options['build-dir'], 'samples', options.sample), SAMPLES[options.sample].target];
	return ['html', 'js', 'wasm'].map((extension) => join(directory, `${target}.${extension}`));
}

export function launch_firefox(browser, profile, spawn_process = spawn) {
	const child = spawn_process(browser, firefox_arguments(profile), { stdio: ['ignore', 'ignore', 'pipe'], windowsHide: true });
	let stderr = '';
	let protocol = null;
	let stopping = false;
	let spawn_error = null;
	let found;
	let refused;
	const endpoint = new Promise((resolve_, reject) => { found = resolve_; refused = reject; });
	endpoint.catch(() => {});
	child.stderr.on('data', (chunk) => {
		stderr = (stderr + String(chunk)).slice(-16384);
		try { const url = bidi_endpoint(stderr); if (url) found(url); } catch (error) { refused(error); }
	});
	child.on('error', (error) => {
		spawn_error = error;
		refused(new Error(`spawn ${browser}: ${error.code ?? error.name}: ${error.message}`));
		protocol?.fail(`Firefox process error: ${error.code ?? error.name}: ${error.message}`);
	});
	child.on('exit', (code, signal) => {
		const reason = `Firefox process exited: code=${code}, signal=${signal ?? 'none'}`;
		if (!stopping) { refused(new Error(reason)); protocol?.fail(reason); }
	});
	return { child, endpoint,
		spawn_failed: () => spawn_error !== null && child.pid === undefined,
		bind: (value) => { protocol = value; if (child.exitCode !== null || child.signalCode !== null) protocol.fail('Firefox already exited'); },
		stop: () => { stopping = true; },
		diagnostics: () => `pid=${child.pid ?? 'unavailable'}, exit=${child.exitCode ?? 'pending'}, ` +
			`signal=${child.signalCode ?? 'none'}\nlast Firefox stderr (16 KiB maximum):\n${stderr || '(empty)'}`,
	};
}

async function bounded(operation, milliseconds, message, expired = () => {}) {
	let timer;
	try {
		return await Promise.race([operation, new Promise((_, refuse) => {
			timer = setTimeout(() => { expired(); refuse(new Error(message)); }, milliseconds);
		})]);
	} finally { clearTimeout(timer); }
}

// A profile kept because its run never established or finished its session
// (main says why) is for looking at soon after; one that has outlasted
// `max_age_ms` is removed when a later run starts. Only folders this runner
// makes, labrador-firefox-*, directly in `directory`, are considered, and a
// young one may belong to a run still going. Answers what it removed.
export function remove_stale_profiles(directory, { now = Date.now(), max_age_ms = 24 * 60 * 60 * 1000,
	log = console.log } = {}) {
	const removed = [];
	for (const name of readdirSync(directory)) {
		if (!name.startsWith('labrador-firefox-')) continue;
		const path = join(directory, name);
		try {
			const status = lstatSync(path);
			if (!status.isDirectory() || now - status.mtimeMs <= max_age_ms) continue;
			rmSync(path, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
			removed.push(path);
			log(`Removed a retained Firefox profile from an earlier run: ${path}`);
		} catch (error) {
			log(`Could not remove the earlier Firefox profile ${path}: ${error.code ?? error.name}: ${error.message}`);
		}
	}
	return removed;
}

export async function main(options) {
	const files = artifact_files(options);
	// These reads occur only on an explicit CLI run, never on module import.
	// Any denied artifact read stops before launching; there is no alternate path.
	for (const file of files) {
		let bytes;
		try { bytes = readFileSync(file); }
		catch (error) { throw new Error(`readFileSync ${file}: ${error.code ?? error.name}: ${error.message}`); }
		console.log(`Firefox artifact ${file}: ${bytes.length} bytes, sha256=${createHash('sha256').update(bytes).digest('hex')}`);
	}
	let profile;
	let service;
	let launched;
	let protocol;
	let connected = false;
	let failure = null;
	try {
		remove_stale_profiles(tmpdir());
		profile = mkdtempSync(join(tmpdir(), 'labrador-firefox-'));
		service = await serve_files(files,
			{ substitute: options.bundle !== undefined ? bundle_substitute : undefined, deny_after_failure: true });
		launched = launch_firefox(options.browser, profile);
		console.log(`Firefox task profile=${profile}, pid=${launched.child.pid ?? 'unavailable'}`);
		const endpoint = await bounded(launched.endpoint, 30000, 'Firefox did not report its BiDi endpoint in 30 seconds.');
		protocol = new FirefoxProtocol(endpoint);
		launched.bind(protocol);
		await protocol.opened();
		const session = await protocol.send('session.new', { capabilities: {} });
		connected = true;
		console.log(`Firefox session ${session.sessionId}: ${JSON.stringify(session.capabilities)}`);
		const { context } = await protocol.send('browsingContext.create', { type: 'tab', background: false });
		if (typeof context !== 'string') throw new Error('Firefox created no browsing context.');
		const driven = options.bundle !== undefined ?
			drive_bundle(protocol, context, service.url, options) :
			options.test !== undefined ?
				drive_window_page(protocol, context, service.url, drive_window,
					{ ...options, expected_complaint: window_expected_complaint }) :
				drive_sample(protocol, context, service.url, SAMPLES[options.sample], options);
		await bounded(Promise.race([service.failed, driven]),
			options.timeout_ms, 'Firefox page timed out.', () => protocol.fail('Firefox page deadline expired'));
		console.log(options.bundle !== undefined ?
			'Firefox bundle: page protocol passed. No listening or physical-device acceptance.' :
			options.test !== undefined ?
				'Firefox WebWindowTests: input sequence passed. No real visibility change, touch cancellation or physical-device acceptance.' :
				`Firefox ${options.sample}: page protocol passed. No WebWindow, listening or physical-device acceptance.`);
	} catch (error) {
		failure = error;
	} finally {
		launched?.stop();
		if (protocol) {
			try { await bounded(protocol.send('browser.close'), 3000, 'Firefox close timed out.'); }
			catch (error) { console.error(`Firefox close: ${error.message}`); }
			protocol.close();
		}
		let closed = !launched;
		if (launched) {
			closed = launched.spawn_failed() || await exited(launched.child, 3000);
			if (!closed) {
				try { launched.child.kill(); closed = await exited(launched.child, 5000); }
				catch (error) {
					const refused = new Error(`terminate owned Firefox pid ${launched.child.pid}: ${error.code}: ${error.message}`);
					console.error(refused.message);
					failure ??= refused;
				}
			}
			console.log(launched.diagnostics());
		}
		if (service) await close_server(service.server);
		if (profile && closed && (!launched || connected || launched.spawn_failed())) {
			try {
				const absolute = realpathSync(profile);
				if (dirname(absolute) !== realpathSync(tmpdir()) || !basename(absolute).startsWith('labrador-firefox-')) {
					throw new Error(`Refusing cleanup outside the task-owned temporary profile: ${absolute}`);
				}
				// The profile can stay locked for a moment after Firefox exits on
				// Windows, as run_page.mjs's does for Chromium.
				rmSync(absolute, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
			} catch (error) {
				const refused = new Error(`cleanup ${profile}: ${error.code ?? error.name}: ${error.message}`);
				console.error(refused.message);
				failure ??= refused;
			}
		} else if (profile) {
			console.error(`Firefox session incomplete or process outcome uncertain; retained task profile ${profile}`);
			failure ??= new Error('Firefox process did not establish and finish its owned session.');
		}
	}
	if (failure) throw failure;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	// A command line it cannot run exits 2 with the reason; a run that
	// fails exits 1 with its stack.
	let options = null;
	try { options = parse_options(process.argv.slice(2)); }
	catch (error) { console.error(`run_firefox: ${error.message}`); process.exitCode = 2; }
	if (options) {
		try { await main(options); }
		catch (error) { console.error(`run_firefox: ${error.stack ?? error}`); process.exitCode = 1; }
	}
}
