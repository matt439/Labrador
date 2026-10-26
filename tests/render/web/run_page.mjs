// Runs one page of a browser build in headless Chromium, and exits with its
// exit code.
//
//   node run_page.mjs --browser <chrome> --page <dir/Name.html>
//                     [--collect <folder in the page's file system>]
//                     [--timeout <seconds>] [--browser-warnings fail]
//                     [--hide-extension <WebGL extension name>]
//                     [--input <script.mjs>]
//   node run_page.mjs --browser <chrome> --page <dir/Name.html>
//                     --quit-keys <code,code,...> [--frames <n>]
//                     [--timeout <seconds>]
//
// ctest's command for RenderPixelTests and RenderContextLossTests in the
// wasm-* presets (tests/render/CMakeLists.txt), for WebWindowTests
// (tests/app/CMakeLists.txt), and for each sample's development page (the
// root CMakeLists.txt). A browser build's other tests run under Node, which
// has no WebGL and no DOM; this is how the ones that need either get a
// browser instead.
//
// TWO KINDS OF PAGE. A test page settles window.labradorExit with main's exit
// code (tests/render/web/shell.html) and is waited on; one that settles with
// anything but an integer has not passed. A sample's page has no
// exit code, because in a browser main never returns, so --quit-keys names
// the way out instead: the run waits for the module's runtime and then
// --frames animation frames (60 unless given), presses each key in turn - a
// KeyboardEvent.code, held long enough for a tick to see it - and passes when
// the page's Module.onQuit is called. Anything else on the way fails it: a
// console error, warning or failed assertion, an uncaught exception, a
// warning or an error the browser logs (a file the page could not fetch is
// one), a call to Module.onError, or a page that gives no onQuit to call. The
// page's own Module object is used, not a test double, so what passes is the
// page as it is built; the run only wraps the three callbacks it hands the
// module, to hear them, before the module's loader can read them.
//
// --input runs a script against the page while it runs, for a page whose
// subject is what the browser delivers to it rather than what it computes.
// The script is a module whose default export is an async function, handed
// one object: evaluate(expression, { user_gesture }) runs an expression in
// the page and answers its value, a promise's once it settles;
// send(method, params) is any DevTools command on the page's session -
// Input.dispatchKeyEvent, Emulation.setTouchEmulationEnabled - which is what
// makes the input real rather than an event the page made of itself; and
// wait(ms). The script asserts by throwing. When it returns the run has
// passed and is over, whether or not the page has exited, and a throw is a
// failure naming its message. A page that aborts or exits with a failure
// first ends the run as it would without one; one that exits cleanly first
// is waited past, because a script may have closed it on purpose and still
// have something to ask of it. By default it waits for a test page's
// labradorExit. A host-page driver can export a `ready` expression instead,
// and prepare(page), called before navigation to install hooks or emulation.
// page.on(listener) subscribes to session events and returns an unsubscribe
// function. A driver exporting strict_complaints = true fails on every
// console/browser complaint; expected_complaint({ source, level, text }) may
// recognize a diagnostic its deliberate failure injection requires. Keep such
// expectations specific to that case, and assert that the failure occurred.
//
// --browser-warnings fail makes a warning or an error the browser itself logs
// a failure of the run, whatever the page's exit code. It is for a page whose
// subject is something the browser complains about rather than the module -
// a WebGL call on an object the context does not own raises an error into a
// queue the engine drains before its next upload, and the browser's warning is
// the only place it is still visible afterwards.
//
// --hide-extension makes every WebGL context the page creates answer as one
// without that extension: getExtension returns null for it and
// getSupportedExtensions leaves it out, from a script that runs before any of
// the page's own. It is how a desktop browser is made to stand in for a device
// that lacks one - RenderPixelTestsWithoutS3tc runs the pixel contract with
// WEBGL_compressed_texture_s3tc hidden, which is what most phones' browsers
// offer. The run fails if a context made after the page finished still sees
// the extension, so a hide that did not take cannot pass for one that did.
//
// WHAT IT DOES, IN ORDER. Serves the page's folder over HTTP on a loopback
// port, because a browser will not compile a module read from file://. Starts
// the browser headless with a profile of its own, on a DevTools port the
// browser chooses, and drives it over the DevTools protocol with Node's own
// WebSocket - no package, so there is nothing to install and nothing to pin.
// Writes every console message the page logs to stdout or stderr, which is
// where doctest's report arrives. Waits on window.labradorExit, which the
// page settles with main's exit code (tests/render/web/shell.html). Then
// copies --collect out of the page's memory into a folder of the same name
// beside the page - for RenderPixelTests that is golden-actual/, the frames a
// failed comparison wrote, which on Windows land in the same place - and says
// which WebGL2 implementation the run drew with, since that is the first
// question a failure raises and nothing else in the log answers it.
//
// NO GPU FLAGS BY DEFAULT BEYOND ONE. --enable-unsafe-swiftshader lets Chrome
// fall back to its software rasteriser for WebGL where there is no GPU, which
// it otherwise refuses to; where there is one, Chrome uses it and the flag
// changes nothing. LABRADOR_TEST_BROWSER_ARGS appends more, space-separated.

import { spawn } from 'node:child_process';
import { mkdtempSync, rmSync, readFileSync, statSync, mkdirSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import { basename, dirname, extname, join, normalize, resolve, sep } from 'node:path';
import { pathToFileURL } from 'node:url';
import { DevTools, browser_version } from './devtools.mjs';
import { press_sample_key } from './sample_key.mjs';

const options = parse_arguments(process.argv.slice(2));
const page_path = resolve(options.page);
const root = dirname(page_path);
const page_name = basename(page_path);
const timeout_ms = Number(options.timeout ?? 300) * 1000;

const MEDIA_TYPES = {
	'.html': 'text/html; charset=utf-8',
	'.js': 'text/javascript; charset=utf-8',
	'.mjs': 'text/javascript; charset=utf-8',
	'.wasm': 'application/wasm',
	'.data': 'application/octet-stream',
	'.json': 'application/json',
};

function parse_arguments(argv) {
	const parsed = {};
	for (let i = 0; i < argv.length; i += 2) {
		if (!argv[i].startsWith('--') || i + 1 >= argv.length) {
			fail_usage(`unexpected argument "${argv[i]}"`);
		}
		parsed[argv[i].slice(2)] = argv[i + 1];
	}
	if (!parsed.browser || !parsed.page) {
		fail_usage('--browser and --page are required');
	}
	if (parsed['browser-warnings'] !== undefined &&
		parsed['browser-warnings'] !== 'fail') {
		fail_usage('--browser-warnings takes one value, "fail"');
	}
	if (parsed['quit-keys'] !== undefined) {
		if (parsed.collect !== undefined || parsed['browser-warnings'] !== undefined) {
			fail_usage('--quit-keys takes neither --collect nor --browser-warnings; ' +
				'a sample\'s page fails on any warning already');
		}
		parsed.keys = parsed['quit-keys'].split(',').filter((code) => code.length > 0);
		if (parsed.keys.length === 0) {
			fail_usage('--quit-keys names at least one key');
		}
		for (const code of parsed.keys) {
			if (key_event(code) === undefined) {
				fail_usage(`--quit-keys: "${code}" is not a key this runner can press`);
			}
		}
		parsed.frame_count = Number(parsed.frames ?? 60);
		if (!Number.isInteger(parsed.frame_count) || parsed.frame_count < 1) {
			fail_usage('--frames takes a whole number of frames, at least one');
		}
	} else if (parsed.frames !== undefined) {
		fail_usage('--frames is for a sample\'s page, with --quit-keys');
	}
	return parsed;
}

function fail_usage(why) {
	process.stderr.write(`run_page: ${why}\n` +
		'usage: node run_page.mjs --browser <chrome> --page <file.html> ' +
		'[--collect <folder>] [--timeout <seconds>] ' +
		'[--browser-warnings fail] [--hide-extension <name>] ' +
		'[--input <script.mjs>]\n' +
		'       node run_page.mjs --browser <chrome> --page <file.html> ' +
		'--quit-keys <code,...> [--frames <n>] [--timeout <seconds>]\n');
	process.exit(2);
}

// What Input.dispatchKeyEvent needs for a KeyboardEvent.code: the key it
// types and the Windows virtual-key code, which is how Chromium tells the
// keys apart. The keys a sample's way out uses, and the letters. The table is
// inside the function because the arguments are parsed, and this asked,
// before the module's own top-level constants exist.
function key_event(code) {
	const named = {
		Escape: { key: 'Escape', vk: 27 },
		Enter: { key: 'Enter', vk: 13 },
		Space: { key: ' ', vk: 32 },
		ArrowLeft: { key: 'ArrowLeft', vk: 37 },
		ArrowUp: { key: 'ArrowUp', vk: 38 },
		ArrowRight: { key: 'ArrowRight', vk: 39 },
		ArrowDown: { key: 'ArrowDown', vk: 40 },
	};
	if (Object.hasOwn(named, code)) {
		return { code, ...named[code] };
	}
	const letter = /^Key([A-Z])$/.exec(code);
	if (letter) {
		return { code, key: letter[1].toLowerCase(), vk: letter[1].charCodeAt(0) };
	}
	return undefined;
}

function serve(directory) {
	const server = createServer((request, response) => {
		const wanted = decodeURIComponent(new URL(request.url, 'http://x').pathname);
		// A browser asks every origin for an icon the page never names. A
		// development page has none and needs none, and the 404 would be a
		// browser error that a sample's run counts as a failure of the page.
		if (wanted === '/favicon.ico') {
			response.writeHead(204).end();
			return;
		}
		const file = normalize(join(directory, wanted));
		if (!file.startsWith(directory + sep)) {
			response.writeHead(403).end();
			return;
		}
		try {
			if (!statSync(file).isFile()) {
				throw new Error('not a file');
			}
			response.writeHead(200, {
				'Content-Type': MEDIA_TYPES[extname(file)] ?? 'application/octet-stream',
				'Cache-Control': 'no-store',
			});
			response.end(readFileSync(file));
		} catch {
			response.writeHead(404).end();
		}
	});
	return new Promise((ready) => {
		server.listen(0, '127.0.0.1', () => ready(server));
	});
}

// The browser, and the DevTools endpoint it prints on stderr once it is
// listening. The port is the browser's choice, so two runs never collide.
function launch(browser, profile) {
	const extra = (process.env.LABRADOR_TEST_BROWSER_ARGS ?? '')
		.split(' ').filter((argument) => argument.length > 0);
	const child = spawn(browser, [
		'--headless=new',
		'--remote-debugging-port=0',
		`--user-data-dir=${profile}`,
		'--no-first-run',
		'--no-default-browser-check',
		'--disable-extensions',
		'--enable-unsafe-swiftshader',
		...extra,
		'about:blank',
	], { stdio: ['ignore', 'ignore', 'pipe'] });

	let stderr_tail = '';
	child.stderr.on('data', (chunk) => {
		stderr_tail = (stderr_tail + chunk.toString()).slice(-16384);
	});
	const diagnostics = () => `pid=${child.pid ?? 'unavailable'}, ` +
		`exit=${child.exitCode ?? 'not reported'}, signal=${child.signalCode ?? 'none'}\n` +
		`last browser stderr (at most 16 KiB):\n${stderr_tail || '(empty)'}\n`;
	const endpoint = new Promise((found, refused) => {
		let seen = '';
		child.stderr.on('data', (chunk) => {
			seen = (seen + chunk.toString()).slice(-16384);
			const match = seen.match(/DevTools listening on (ws:\/\/\S+)/);
			if (match) {
				found(match[1]);
			}
		});
		child.on('error', refused);
		child.on('exit', (code) => refused(new Error(
			`the browser exited (${code}) before it was listening:\n${seen}`)));
	});
	return { child, endpoint, diagnostics };
}

function console_text(args) {
	return args.map((argument) => {
		if (argument.value !== undefined) {
			return String(argument.value);
		}
		return argument.description ?? argument.type;
	}).join(' ');
}

function wait(ms) {
	return new Promise((done) => setTimeout(done, ms));
}

async function evaluate(devtools, session, expression, await_promise = false,
	user_gesture = false) {
	const answer = await devtools.send('Runtime.evaluate', {
		expression,
		awaitPromise: await_promise,
		returnByValue: true,
		userGesture: user_gesture,
	}, session);
	if (answer.exceptionDetails) {
		throw new Error(answer.exceptionDetails.exception?.description ??
			answer.exceptionDetails.text);
	}
	return answer.result.value;
}

// The files under `folder` in the page's file system, base64-encoded. Empty if
// the folder was never made, which is the passing case.
//
// THE PATH IS WALKED WITH readdir FROM THE ROOT rather than looked up, because
// a lookup that misses builds an FS.ErrnoError, which in a debug build asks
// the module for strerror - and the module has exited by now, so a miss aborts
// it. A directory listing of a folder that exists cannot miss.
function collect_expression(folder) {
	return `(() => {
		const out = {};
		const fs = (typeof Module !== 'undefined') ? Module.FS : undefined;
		if (!fs) { return out; }
		let at = '/';
		for (const part of ${JSON.stringify(folder)}.split('/').filter((p) => p.length > 0)) {
			if (!fs.readdir(at).includes(part)) { return out; }
			at = at === '/' ? '/' + part : at + '/' + part;
		}
		const names = fs.readdir(at);
		for (const name of names) {
			if (name === '.' || name === '..') { continue; }
			const bytes = fs.readFile(at + '/' + name);
			let text = '';
			for (let i = 0; i < bytes.length; i += 0x8000) {
				text += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
			}
			out[name] = btoa(text);
		}
		return out;
	})()`;
}

// A script for Page.addScriptToEvaluateOnNewDocument that makes `name` absent
// from every WebGL context the page makes, as a device without it would.
function hide_extension_script(name) {
	return `(() => {
		const hidden = ${JSON.stringify(name)};
		for (const type of [self.WebGLRenderingContext, self.WebGL2RenderingContext]) {
			if (!type) { continue; }
			const get = type.prototype.getExtension;
			type.prototype.getExtension = function (wanted) {
				return wanted === hidden ? null : get.call(this, wanted);
			};
			const list = type.prototype.getSupportedExtensions;
			type.prototype.getSupportedExtensions = function () {
				const names = list.call(this);
				return names ? names.filter((each) => each !== hidden) : names;
			};
		}
	})();`;
}

// Whether a context made now still offers `name`.
function extension_present_expression(name) {
	return `(() => {
		const gl = document.createElement('canvas').getContext('webgl2');
		return gl ? gl.getExtension(${JSON.stringify(name)}) !== null : false;
	})()`;
}

// Which WebGL2 implementation this browser hands a page, asked of a canvas of
// its own so that it answers whatever the test did to its own contexts.
const RENDERER_EXPRESSION = `(() => {
	const gl = document.createElement('canvas').getContext('webgl2');
	if (!gl) { return 'no WebGL2 context'; }
	const info = gl.getExtension('WEBGL_debug_renderer_info');
	return info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL)
		: gl.getParameter(gl.RENDERER);
})()`;

// Evaluated in a sample's page before any of its own scripts. The page
// declares `var Module = {...}` and the loader re-reads it, so an accessor on
// window sees the object the moment it is assigned, with its callbacks
// already in it, and wraps each before the loader can read it. What the
// wrappers hear is in window.labradorRun.
const SAMPLE_HOOK = `(() => {
	const heard = { initialized: false, quit: false, errors: [], has_quit: false };
	window.labradorRun = heard;
	let module;
	const hook = (value) => {
		if (value === null || typeof value !== 'object' || value.labradorRunHooked) {
			return;
		}
		Object.defineProperty(value, 'labradorRunHooked', { value: true });
		const initialized = value.onRuntimeInitialized;
		value.onRuntimeInitialized = function () {
			heard.initialized = true;
			if (typeof initialized === 'function') {
				return initialized.apply(this, arguments);
			}
		};
		const quit = value.onQuit;
		heard.has_quit = typeof quit === 'function';
		if (heard.has_quit) {
			value.onQuit = function () {
				heard.quit = true;
				return quit.apply(this, arguments);
			};
		}
		const error = value.onError;
		value.onError = function (message) {
			heard.errors.push(String(message));
			if (typeof error === 'function') {
				return error.apply(this, arguments);
			}
		};
	};
	Object.defineProperty(window, 'Module', {
		configurable: true,
		enumerable: true,
		get() { return module; },
		set(value) { module = value; hook(value); },
	});
})()`;

function frames_expression(count) {
	return `new Promise((done) => {
		let left = ${count};
		const step = () => { if (--left <= 0) { done(true); } else { requestAnimationFrame(step); } };
		requestAnimationFrame(step);
	})`;
}

// A sample's page, from the runtime to Module.onQuit. Returns an outcome in
// the shape the test page's wait gives: a code, or an abort with the reason.
async function run_sample(devtools, session, complaints) {
	const heard = () => evaluate(devtools, session, 'window.labradorRun');
	const failed = (state) => {
		const reasons = [];
		if (state && state.errors.length > 0) {
			reasons.push(`Module.onError was called: ${state.errors.join(' | ')}`);
		}
		if (complaints.count > 0) {
			reasons.push(`the page or the browser logged ${complaints.count} error(s) or warning(s)`);
		}
		return reasons.length > 0 ? reasons.join('; ') : undefined;
	};

	let state = await heard();
	while (!(state && state.initialized)) {
		const why = failed(state);
		if (why) {
			return { abort: `${why}, before the runtime was ready` };
		}
		await wait(50);
		state = await heard();
	}
	if (!state.has_quit) {
		return { abort: 'the page gives the module no Module.onQuit to call' };
	}

	await evaluate(devtools, session, frames_expression(options.frame_count), true);
	process.stdout.write(`run_page: ${options.frame_count} animation frames\n`);
	let why = failed(await heard());
	if (why) {
		return { abort: why };
	}

	const input_page = {
		send: (method, params) => devtools.send(method, params, session),
		evaluate: (expression) => evaluate(devtools, session, expression, true),
	};
	for (const code of options.keys) {
		const key = key_event(code);
		const event = {
			code: key.code,
			key: key.key,
			windowsVirtualKeyCode: key.vk,
			nativeVirtualKeyCode: key.vk,
		};
		await press_sample_key(input_page, event);
		process.stdout.write(`run_page: pressed ${code}\n`);
	}

	state = await heard();
	while (!state.quit) {
		why = failed(state);
		if (why) {
			return { abort: why };
		}
		await wait(50);
		state = await heard();
	}
	process.stdout.write('run_page: Module.onQuit was called\n');

	// A few frames more, so that a failure on the way out is heard too.
	await evaluate(devtools, session, frames_expression(10), true);
	why = failed(await heard());
	return why ? { abort: why } : { code: 0 };
}

// A note the WebGL implementation logs about its own cost, not about anything
// the page did wrong. ANGLE over SwiftShader, which is what a machine with no
// GPU draws with, logs "GPU stall due to ReadPixels" as a warning four times
// for a sample whose code reads nothing back; the readback is the browser's
// own. It is printed with the rest and counted against nobody, and a driver
// message of any other type still counts.
const PERFORMANCE_NOTE = /GL Driver Message \([^,]*, Performance,/;

async function run() {
	const input_driver = options.input === undefined ? undefined :
		await import(pathToFileURL(resolve(options.input)).href);
	const server = await serve(root);
	const url = `http://127.0.0.1:${server.address().port}/${page_name}`;
	const profile = mkdtempSync(join(tmpdir(), 'labrador-browser-'));
	const { child, endpoint, diagnostics } = launch(options.browser, profile);

	let exit_code = 1;
	let browser_complaints = 0;
	// What a sample's page is failed by: everything above, and the page's own
	// console errors and warnings and uncaught exceptions as well.
	const sample_complaints = { count: 0 };
	let devtools;
	let browser = 'unknown (DevTools did not answer)';
	try {
		devtools = new DevTools(await Promise.race([
			endpoint,
			wait(30000).then(() => {
				throw new Error('the browser did not open its DevTools port within 30 s');
			}),
		]));
		await devtools.opened();
		browser = await Promise.race([
			browser_version(devtools),
			wait(10000).then(() => { throw new Error('Browser.getVersion did not answer within 10 s'); }),
		]);

		const { targetInfos } = await devtools.send('Target.getTargets');
		const page = targetInfos.find((target) => target.type === 'page');
		if (!page) {
			throw new Error('the browser opened no page');
		}
		const { sessionId: session } = await devtools.send('Target.attachToTarget',
			{ targetId: page.targetId, flatten: true });
		const page_api = {
			evaluate: (expression, { user_gesture = false } = {}) =>
				evaluate(devtools, session, expression, true, user_gesture),
			send: (method, params = {}) => devtools.send(method, params, session),
			on: (listener) => devtools.on((message) => {
				if (message.sessionId === session) { listener(message); }
			}),
			wait,
		};
		const complaint = (source, level, text) => {
			if (input_driver?.expected_complaint?.({ source, level, text }) === true) {
				process.stdout.write(`run_page: expected ${source} diagnostic: ${text}\n`);
				return;
			}
			sample_complaints.count++;
			if (source === 'browser') { browser_complaints++; }
		};

		devtools.on((message) => {
			if (message.sessionId !== session) {
				return;
			}
			if (message.method === 'Runtime.consoleAPICalled') {
				const text = console_text(message.params.args) + '\n';
				const type = message.params.type;
				if (type === 'error' || type === 'warning' || type === 'assert') {
					process.stderr.write(text);
					complaint('console', type, text.trimEnd());
				} else {
					process.stdout.write(text);
				}
			} else if (message.method === 'Runtime.exceptionThrown') {
				const details = message.params.exceptionDetails;
				process.stderr.write(`run_page: uncaught: ${details.exception?.description ?? details.text}\n`);
				complaint('exception', 'error', details.exception?.description ?? details.text);
			} else if (message.method === 'Inspector.targetCrashed') {
				// Nothing waiting on the page will be answered now.
				devtools.refuse_all('the page crashed');
			} else if (message.method === 'Log.entryAdded') {
				const entry = message.params.entry;
				const where = entry.url ? ` (${entry.url})` : '';
				process.stderr.write(`run_page: browser ${entry.level}: ${entry.text}${where}\n`);
				if ((entry.level === 'warning' || entry.level === 'error') &&
					!PERFORMANCE_NOTE.test(entry.text)) {
					complaint('browser', entry.level, entry.text);
				}
			}
		});
		await devtools.send('Inspector.enable', {}, session);
		await devtools.send('Runtime.enable', {}, session);
		await devtools.send('Log.enable', {}, session);
		await devtools.send('Page.enable', {}, session);
		if (input_driver?.prepare) { await input_driver.prepare(page_api); }
		if (options['hide-extension']) {
			await devtools.send('Page.addScriptToEvaluateOnNewDocument', {
				source: hide_extension_script(options['hide-extension']),
			}, session);
			process.stdout.write(`run_page: hiding ${options['hide-extension']} from the page\n`);
		}
		if (options.keys) {
			await devtools.send('Page.addScriptToEvaluateOnNewDocument',
				{ source: SAMPLE_HOOK }, session);
			// A headless page is never the focused window, and a key goes to
			// the focused element of a focused window or nowhere.
			await devtools.send('Emulation.setFocusEmulationEnabled',
				{ enabled: true }, session);
		}
		await devtools.send('Page.navigate', { url }, session);

		// The navigation commits before the page's first script runs, so the
		// promise is waited for until it exists and then awaited. ONE DEADLINE
		// FOR THE WHOLE OF IT, because a page whose main thread never yields
		// answers neither question, and an evaluate that is never answered
		// would otherwise hold the run past any check made between them.
		const finished = input_driver?.ready !== undefined ? new Promise(() => {}) : options.keys
			? (async () => {
				while (await evaluate(devtools, session,
					`typeof window.labradorRun === 'object' && location.href === ${JSON.stringify(url)}`) !== true) {
					await wait(50);
				}
				return run_sample(devtools, session, sample_complaints);
			})()
			: (async () => {
				while (await evaluate(devtools, session,
					`typeof window.labradorExit === 'object' && location.href === ${JSON.stringify(url)}`) !== true) {
					await wait(50);
				}
				return evaluate(devtools, session, 'window.labradorExit', true);
			})();
		// Settled after a timeout has already won, it has nobody to tell.
		finished.catch(() => {});

		// The input script, started once the page's first script has run and
		// raced against the page's own end: a page that never exits is
		// finished by its script, and a script still waiting on a page that
		// has failed is not waited for.
		const driven = options.input === undefined ? new Promise(() => {}) :
			(async () => {
				const ready = input_driver.ready ?? "typeof window.labradorExit === 'object'";
				while (await evaluate(devtools, session,
					`(${ready}) && location.href === ${JSON.stringify(url)}`) !== true) {
					await wait(50);
				}
				await input_driver.default(page_api);
				return { code: 0 };
			})().catch((error) => ({ failed: error.stack ?? String(error) }));

		const page_end = options.input === undefined ? finished :
			finished.then((ended) => (ended.code === 0 ? new Promise(() => {}) : ended));
		const outcome = await Promise.race([
			page_end,
			driven,
			wait(timeout_ms).then(() => ({ timeout: true })),
		]);

		if (outcome.timeout) {
			process.stderr.write(`run_page: ${page_name} did not finish within ${timeout_ms / 1000} s\n`);
		} else if (outcome.failed !== undefined) {
			process.stderr.write(`run_page: ${page_name} failed its input script: ${outcome.failed}\n`);
		} else if (outcome.abort !== undefined) {
			process.stderr.write(`run_page: ${page_name} aborted: ${outcome.abort}\n`);
		} else if (!Number.isInteger(outcome.code)) {
			// process.exit(undefined) is a pass, so a page that settled with no
			// code - a runtime exit that lost it, or a shell that resolved the
			// promise some other way - would pass without having said so.
			process.stderr.write(`run_page: ${page_name} ended with no exit code ` +
				`(${JSON.stringify(outcome.code)}), which is a failure\n`);
		} else {
			exit_code = outcome.code;
		}

		if (options['browser-warnings'] === 'fail' && browser_complaints > 0) {
			process.stderr.write(`run_page: the browser logged ${browser_complaints} ` +
				'warning(s) or error(s), and --browser-warnings fail counts each as a failure\n');
			exit_code = exit_code === 0 ? 1 : exit_code;
		}
		if (input_driver?.strict_complaints === true && sample_complaints.count > 0) {
			process.stderr.write(`run_page: the input page logged ${sample_complaints.count} ` +
				'unexpected error(s) or warning(s)\n');
			exit_code = exit_code === 0 ? 1 : exit_code;
		}

		// A page that timed out is one whose main thread may never answer
		// again, so it is asked nothing more; one that finished is asked with
		// a deadline of its own all the same.
		const asked = (expression, otherwise) => Promise.race([
			evaluate(devtools, session, expression),
			wait(10000).then(() => otherwise),
		]);

		if (options.collect && !outcome.timeout) {
			const files = await asked(collect_expression(options.collect), {});
			const names = Object.keys(files);
			if (names.length > 0) {
				const into = join(root, basename(options.collect));
				mkdirSync(into, { recursive: true });
				for (const name of names) {
					writeFileSync(join(into, name), Buffer.from(files[name], 'base64'));
				}
				process.stderr.write(`run_page: ${names.length} file(s) from ${options.collect} written to ${into}\n`);
			}
		}

		if (options['hide-extension'] && !outcome.timeout) {
			const present = await asked(
				extension_present_expression(options['hide-extension']), true);
			if (present !== false) {
				process.stderr.write(`run_page: ${options['hide-extension']} was ` +
					'still offered to the page, so this run did not test its absence\n');
				exit_code = exit_code === 0 ? 1 : exit_code;
			}
		}

		if (!outcome.timeout) {
			const renderer = await asked(RENDERER_EXPRESSION,
				'unknown (the page did not answer)');
			process.stdout.write(`run_page: browser: ${browser}\n`);
			process.stdout.write(`run_page: WebGL2 renderer: ${renderer}\n`);
		} else {
			process.stdout.write(`run_page: browser: ${browser}\n`);
		}
	} catch (error) {
		process.stderr.write(`run_page: browser: ${browser}\n`);
		process.stderr.write(`run_page: ${error.message}\n`);
		process.stderr.write(`run_page: browser receipt: ${diagnostics()}`);
		exit_code = 1;
	} finally {
		if (devtools) {
			await Promise.race([devtools.send('Browser.close').catch(() => {}), wait(3000)]);
		}
		await Promise.race([new Promise((done) => child.once('exit', done)), wait(5000)]);
		if (child.exitCode === null) {
			child.kill();
		}
		server.close();
		// The profile can stay locked for a moment after the browser exits on
		// Windows; a leftover temporary folder is not worth failing a run for.
		for (let attempt = 0; attempt < 10; attempt++) {
			try {
				rmSync(profile, { recursive: true, force: true });
				break;
			} catch {
				await wait(200);
			}
		}
	}
	return exit_code;
}

process.exit(await run());
