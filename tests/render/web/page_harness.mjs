// What the WebDriver page runners share: run_firefox.mjs over BiDi and
// run_safari.mjs over classic WebDriver. Each sample's page and quit keys,
// WebDriver's code points for the keys they press, the frame wait, the run's
// deadline, and the server that hands the browser only the run's own files.
import { readFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { basename } from 'node:path';
import { CONTENT_TYPES } from '../../../website/acceptance/server.mjs';

// Each development sample's page target and the keys that quit it. The root
// CMakeLists.txt's labrador_sample_page_test entries press the same keys in
// Chromium; page_harness.test.mjs holds the two to each other.
export const SAMPLES = Object.freeze({
	// The pause menu: RESUME, RESTART, COLOURS, QUIT.
	linesweeper: { target: 'LineSweeperSample', keys: ['Escape', 'ArrowDown', 'ArrowDown', 'ArrowDown', 'Enter'] },
	minimal: { target: 'MinimalSample', keys: ['Escape', 'Enter'] },
	collision: { target: 'CollisionSample', keys: ['Escape'] },
	audio: { target: 'AudioSample', keys: ['KeyS', 'Escape'] },
	local_multiplayer: { target: 'LocalMultiplayerSample', keys: ['Escape'] },
	// The keys ComponentsPageTests presses: a browser can take the first
	// Escape to leave full screen, so a second one quits the gallery.
	components: { target: 'ComponentsGallery',
		keys: ['ArrowDown', 'ArrowDown', 'ArrowLeft', 'ArrowUp', 'ArrowRight', 'KeyR', 'Escape', 'Escape'] },
});

// WebDriver's value for each KeyboardEvent.code a runner presses: a code
// point from the specification's key table, or the character a printable key
// types. https://www.w3.org/TR/webdriver2/#keyboard-actions
export const WEBDRIVER_KEYS = Object.freeze({
	Tab: '\uE004',
	Enter: '\uE007',
	Escape: '\uE00C',
	Space: ' ',
	ArrowLeft: '\uE012',
	ArrowUp: '\uE013',
	ArrowRight: '\uE014',
	ArrowDown: '\uE015',
	KeyA: 'a',
	KeyR: 'r',
	KeyS: 's',
});
export const WEBDRIVER_CONTROL = '\uE009';
// Alt, which is Option on a Mac.
export const WEBDRIVER_OPTION = '\uE00A';

// The part of WEBDRIVER_KEYS one kind of input may press; any other code is
// refused rather than sent.
export function key_table(codes) {
	return Object.freeze(Object.fromEntries(codes.map((code) => {
		if (!Object.hasOwn(WEBDRIVER_KEYS, code)) throw new Error(`WebDriver has no key ${code} here.`);
		return [code, WEBDRIVER_KEYS[code]];
	})));
}

// The keys SAMPLES presses.
export const SAMPLE_KEYS = key_table(['Escape', 'Enter', 'KeyS', 'KeyR',
	'ArrowLeft', 'ArrowUp', 'ArrowRight', 'ArrowDown']);

// One key source pressing each key in turn, each held for hold_ms and
// released for gap_ms (hold_ms unless given), so a page's once-a-frame poll
// sees both levels.
export function key_actions(keys, { table = SAMPLE_KEYS, hold_ms = 150, gap_ms = hold_ms } = {}) {
	return [{ type: 'key', id: 'keyboard', actions: keys.flatMap((key) => {
		if (!Object.hasOwn(table, key)) throw new Error(`Unsupported sample key ${key}.`);
		return [{ type: 'keyDown', value: table[key] }, { type: 'pause', duration: hold_ms },
			{ type: 'keyUp', value: table[key] }, { type: 'pause', duration: gap_ms }];
	}) }];
}

// An expression that settles once `count` animation frames have run.
export function frames_expression(count) {
	return `new Promise((done) => {
		let left = ${count};
		const next = () => --left <= 0 ? done(true) : requestAnimationFrame(next);
		requestAnimationFrame(next);
	})`;
}

// The statements, for a hook that runs before a development page's own
// scripts, that hear the page's Module. The page declares `var Module = {...}`
// and the loader re-reads it, so an accessor on window sees the object the
// moment it is assigned, with its callbacks already in it, and wraps
// onRuntimeInitialized, onQuit and onError before the loader can read them.
// The enclosing hook declares `heard` ({ initialized, quit, has_quit, errors
// }); these declare `module`, the object, for the rest of the hook to read.
// Each flag is set before the page's own callback runs, so a callback that
// throws is still heard. A Module without onQuit is recorded as such rather
// than given one. `marker` names the property that keeps an object from being
// wrapped twice, and `extra` runs once for each object, as `value`.
export function module_hook(marker, extra = '') {
	return `let module;
	Object.defineProperty(window, 'Module', {
		configurable: true, enumerable: true,
		get() { return module; },
		set(value) {
			module = value;
			if (value === null || typeof value !== 'object' || value.${marker}) return;
			Object.defineProperty(value, '${marker}', { value: true });
			${extra}
			const initialized = value.onRuntimeInitialized;
			value.onRuntimeInitialized = function (...args) {
				heard.initialized = true;
				if (typeof initialized === 'function') return initialized.apply(this, args);
			};
			const quit = value.onQuit;
			heard.has_quit = typeof quit === 'function';
			if (heard.has_quit) {
				value.onQuit = function (...args) {
					heard.quit = true;
					return quit.apply(this, args);
				};
			}
			const error = value.onError;
			value.onError = function (...args) {
				heard.errors.push(String(args[0]));
				if (typeof error === 'function') return error.apply(this, args);
			};
		},
	});`;
}

// A run's deadline: each call answers the time left, at most `cap_ms`, for one
// command's own timeout, and throws once none is left.
export function deadline_after(timeout_ms, what, cap_ms) {
	const deadline = Date.now() + timeout_ms;
	return () => {
		const duration = deadline - Date.now();
		if (duration <= 0) throw new Error(`${what} deadline expired.`);
		return Math.min(duration, cap_ms);
	};
}

// Serves `files` by their base names on a loopback port, and nothing else: a
// request for any other path answers 404 and settles `failed`, which a run
// races against its page. `substitute(url)` may answer a request first with
// { type, body }; `transform(text)` rewrites each HTML file before it is sent.
// With deny_after_failure, every request after the first refusal answers 503.
// Content types are the website acceptance server's, which run_page.mjs's
// server uses too, so the three runners serve a build's files alike.
// `page` is the path the returned url names, the first file's by default.
export async function serve_files(files, { substitute = () => null, transform = null,
	deny_after_failure = false, page = basename(files[0] ?? '') } = {}) {
	let fatal;
	const failed = new Promise((_, refuse) => { fatal = refuse; });
	failed.catch(() => {});
	const allowed = new Map(files.map((file) => ['/' + basename(file), file]));
	let denied = false;
	const deny = (response, status, error) => {
		denied = deny_after_failure;
		response.writeHead(status).end();
		fatal(error);
	};
	const server = createServer((request, response) => {
		if (denied) { response.writeHead(503).end(); return; }
		const requested = new URL(request.url, 'http://127.0.0.1');
		const wanted = requested.pathname;
		if (wanted === '/favicon.ico') { response.writeHead(204).end(); return; }
		const substituted = substitute(requested);
		if (substituted) {
			response.writeHead(200, { 'Cache-Control': 'no-store', 'Content-Type': substituted.type });
			response.end(substituted.body);
			return;
		}
		const file = allowed.get(wanted);
		if (!file) {
			deny(response, 404, new Error(`Unexpected page resource request: ${wanted}`));
			return;
		}
		try {
			let bytes = readFileSync(file);
			const extension = /\.[^.]+$/.exec(file)?.[0] ?? '';
			if (extension === '.html' && transform) bytes = Buffer.from(transform(bytes.toString('utf8')));
			response.writeHead(200, { 'Cache-Control': 'no-store',
				'Content-Type': CONTENT_TYPES[extension] ?? 'application/octet-stream' });
			response.end(bytes);
		} catch (error) {
			deny(response, 500, new Error(`readFileSync ${file}: ${error.code ?? error.name}: ${error.message}`));
		}
	});
	await new Promise((ready, refused) => {
		server.once('error', refused);
		server.listen(0, '127.0.0.1', ready);
	});
	return { server, failed, url: `http://127.0.0.1:${server.address().port}/${page}` };
}

// Closes a server from serve_files, and every connection a browser left open.
export function close_server(server) {
	return new Promise((done) => {
		server.close(done);
		server.closeAllConnections();
	});
}

// Whether `child` has exited, waiting at most `milliseconds` for it. A child
// that has already gone answers at once: its 'exit' event will not come again.
export async function exited(child, milliseconds) {
	if (child.exitCode !== null || child.signalCode !== null) return true;
	let timer;
	return new Promise((done) => {
		const finish = (value) => { clearTimeout(timer); child.off('exit', on_exit); done(value); };
		const on_exit = () => finish(true);
		child.once('exit', on_exit);
		timer = setTimeout(() => finish(false), milliseconds);
	});
}
