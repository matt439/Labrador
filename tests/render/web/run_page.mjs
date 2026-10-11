// Runs one test page of a browser build in headless Chromium, and exits with
// its exit code.
//
//   node run_page.mjs --browser <chrome> --page <dir/Name.html>
//                     [--collect <folder in the page's file system>]
//                     [--timeout <seconds>] [--browser-warnings fail]
//
// ctest's command for RenderPixelTests and RenderContextLossTests in the
// wasm-* presets (tests/render/CMakeLists.txt). A browser build's other tests
// run under Node, which has no WebGL; this is how the ones that draw get a
// browser instead.
//
// --browser-warnings fail makes a warning or an error the browser itself logs
// a failure of the run, whatever the page's exit code. It is for a page whose
// subject is something the browser complains about rather than the module -
// a WebGL call on an object the context does not own raises an error into a
// queue the engine drains before its next upload, and the browser's warning is
// the only place it is still visible afterwards.
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
	return parsed;
}

function fail_usage(why) {
	process.stderr.write(`run_page: ${why}\n` +
		'usage: node run_page.mjs --browser <chrome> --page <file.html> ' +
		'[--collect <folder>] [--timeout <seconds>] ' +
		'[--browser-warnings fail]\n');
	process.exit(2);
}

function serve(directory) {
	const server = createServer((request, response) => {
		const wanted = decodeURIComponent(new URL(request.url, 'http://x').pathname);
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

	const endpoint = new Promise((found, refused) => {
		let seen = '';
		child.stderr.on('data', (chunk) => {
			seen += chunk.toString();
			const match = seen.match(/DevTools listening on (ws:\/\/\S+)/);
			if (match) {
				found(match[1]);
			}
		});
		child.on('error', refused);
		child.on('exit', (code) => refused(new Error(
			`the browser exited (${code}) before it was listening:\n${seen}`)));
	});
	return { child, endpoint };
}

// One WebSocket to the browser, with sessions flattened onto it, so a page's
// events and the answers to its commands arrive in the order they were sent.
class DevTools {
	constructor(url) {
		this.socket = new WebSocket(url);
		this.next_id = 1;
		this.pending = new Map();
		this.listeners = [];
		// A socket that closes - the browser crashed or was killed - will
		// answer nothing still waiting on it, so all of it is refused rather
		// than left to hold the run open.
		this.socket.addEventListener('close', () => this.refuse_all('the browser went away'));
		this.socket.addEventListener('message', (event) => {
			const message = JSON.parse(event.data);
			if (message.id !== undefined) {
				const waiting = this.pending.get(message.id);
				this.pending.delete(message.id);
				if (message.error) {
					waiting.refused(new Error(`${waiting.method}: ${message.error.message}`));
				} else {
					waiting.found(message.result);
				}
				return;
			}
			for (const listener of this.listeners) {
				listener(message);
			}
		});
	}

	opened() {
		return new Promise((found, refused) => {
			this.socket.addEventListener('open', () => found(), { once: true });
			this.socket.addEventListener('error', () => refused(
				new Error('could not reach the browser\'s DevTools endpoint')), { once: true });
		});
	}

	send(method, params = {}, session_id = undefined) {
		const id = this.next_id++;
		const message = { id, method, params };
		if (session_id) {
			message.sessionId = session_id;
		}
		return new Promise((found, refused) => {
			this.pending.set(id, { found, refused, method });
			this.socket.send(JSON.stringify(message));
		});
	}

	on(listener) {
		this.listeners.push(listener);
	}

	refuse_all(why) {
		for (const waiting of this.pending.values()) {
			waiting.refused(new Error(`${waiting.method}: ${why}`));
		}
		this.pending.clear();
	}
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

async function evaluate(devtools, session, expression, await_promise = false) {
	const answer = await devtools.send('Runtime.evaluate', {
		expression,
		awaitPromise: await_promise,
		returnByValue: true,
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

// Which WebGL2 implementation this browser hands a page, asked of a canvas of
// its own so that it answers whatever the test did to its own contexts.
const RENDERER_EXPRESSION = `(() => {
	const gl = document.createElement('canvas').getContext('webgl2');
	if (!gl) { return 'no WebGL2 context'; }
	const info = gl.getExtension('WEBGL_debug_renderer_info');
	return info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL)
		: gl.getParameter(gl.RENDERER);
})()`;

async function run() {
	const server = await serve(root);
	const url = `http://127.0.0.1:${server.address().port}/${page_name}`;
	const profile = mkdtempSync(join(tmpdir(), 'labrador-browser-'));
	const { child, endpoint } = launch(options.browser, profile);

	let exit_code = 1;
	let browser_complaints = 0;
	let devtools;
	try {
		devtools = new DevTools(await Promise.race([
			endpoint,
			wait(30000).then(() => {
				throw new Error('the browser did not open its DevTools port within 30 s');
			}),
		]));
		await devtools.opened();

		const { targetInfos } = await devtools.send('Target.getTargets');
		const page = targetInfos.find((target) => target.type === 'page');
		if (!page) {
			throw new Error('the browser opened no page');
		}
		const { sessionId: session } = await devtools.send('Target.attachToTarget',
			{ targetId: page.targetId, flatten: true });

		devtools.on((message) => {
			if (message.sessionId !== session) {
				return;
			}
			if (message.method === 'Runtime.consoleAPICalled') {
				const text = console_text(message.params.args) + '\n';
				const type = message.params.type;
				if (type === 'error' || type === 'warning' || type === 'assert') {
					process.stderr.write(text);
				} else {
					process.stdout.write(text);
				}
			} else if (message.method === 'Runtime.exceptionThrown') {
				const details = message.params.exceptionDetails;
				process.stderr.write(`run_page: uncaught: ${details.exception?.description ?? details.text}\n`);
			} else if (message.method === 'Inspector.targetCrashed') {
				// Nothing waiting on the page will be answered now.
				devtools.refuse_all('the page crashed');
			} else if (message.method === 'Log.entryAdded') {
				const level = message.params.entry.level;
				process.stderr.write(`run_page: browser ${level}: ${message.params.entry.text}\n`);
				if (level === 'warning' || level === 'error') {
					browser_complaints++;
				}
			}
		});
		await devtools.send('Inspector.enable', {}, session);
		await devtools.send('Runtime.enable', {}, session);
		await devtools.send('Log.enable', {}, session);
		await devtools.send('Page.enable', {}, session);
		await devtools.send('Page.navigate', { url }, session);

		// The navigation commits before the page's first script runs, so the
		// promise is waited for until it exists and then awaited. ONE DEADLINE
		// FOR THE WHOLE OF IT, because a page whose main thread never yields
		// answers neither question, and an evaluate that is never answered
		// would otherwise hold the run past any check made between them.
		const finished = (async () => {
			while (await evaluate(devtools, session,
				`typeof window.labradorExit === 'object' && location.href === ${JSON.stringify(url)}`) !== true) {
				await wait(50);
			}
			return evaluate(devtools, session, 'window.labradorExit', true);
		})();
		// Settled after a timeout has already won, it has nobody to tell.
		finished.catch(() => {});
		const outcome = await Promise.race([
			finished,
			wait(timeout_ms).then(() => ({ timeout: true })),
		]);

		if (outcome.timeout) {
			process.stderr.write(`run_page: ${page_name} did not finish within ${timeout_ms / 1000} s\n`);
		} else if (outcome.abort !== undefined) {
			process.stderr.write(`run_page: ${page_name} aborted: ${outcome.abort}\n`);
		} else {
			exit_code = outcome.code;
		}

		if (options['browser-warnings'] === 'fail' && browser_complaints > 0) {
			process.stderr.write(`run_page: the browser logged ${browser_complaints} ` +
				'warning(s) or error(s), and --browser-warnings fail counts each as a failure\n');
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

		if (!outcome.timeout) {
			const renderer = await asked(RENDERER_EXPRESSION,
				'unknown (the page did not answer)');
			process.stdout.write(`run_page: WebGL2 renderer: ${renderer}\n`);
		}
	} catch (error) {
		process.stderr.write(`run_page: ${error.message}\n`);
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
