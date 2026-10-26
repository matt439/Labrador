// Five development sample pages in Firefox BiDi, separate from Chromium's
// runner and from WebWindowTests' unported input/emulation assertions.
// No geckodriver, browser preferences, shared profile or GPU/security flags.
//
// node tests/render/web/run_firefox.mjs --browser <firefox.exe>
//   --build-dir out/build/wasm-release --sample minimal [--timeout 60] [--frames 60]
//
// Protocol/bootstrap sources (Mozilla's current WebDriver documentation):
// https://developer.mozilla.org/en-US/docs/Web/WebDriver/How_to/Create_BiDi_connection
// https://developer.mozilla.org/en-US/docs/Web/WebDriver/Reference/BiDi/Modules/session/new
// https://developer.mozilla.org/en-US/docs/Web/WebDriver/Reference/BiDi/Modules/input/performActions
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdtempSync, readFileSync, realpathSync, rmSync } from 'node:fs';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { FirefoxProtocol } from './firefox_protocol.mjs';

export const SAMPLES = Object.freeze({
	minimal: { target: 'MinimalSample', keys: ['Escape', 'Enter'] },
	collision: { target: 'CollisionSample', keys: ['Escape'] },
	audio: { target: 'AudioSample', keys: ['KeyS', 'Escape'] },
	local_multiplayer: { target: 'LocalMultiplayerSample', keys: ['Escape'] },
	components: { target: 'ComponentsGallery', keys: ['ArrowDown', 'ArrowDown', 'ArrowLeft', 'ArrowUp', 'ArrowRight', 'Escape'] },
});

const KEY_VALUES = Object.freeze({ Escape: '\uE00C', Enter: '\uE007', KeyS: 's',
	ArrowLeft: '\uE012', ArrowUp: '\uE013', ArrowRight: '\uE014', ArrowDown: '\uE015' });

export function parse_options(arguments_) {
	const options = {};
	const allowed = new Set(['browser', 'build-dir', 'sample', 'timeout', 'frames']);
	for (let index = 0; index < arguments_.length; index += 2) {
		const name = arguments_[index].replace(/^--/, '');
		if (!arguments_[index].startsWith('--') || !allowed.has(name) ||
			index + 1 >= arguments_.length || Object.hasOwn(options, name)) {
			throw new Error(`Invalid or duplicate argument ${arguments_[index]}.`);
		}
		options[name] = arguments_[index + 1];
	}
	if (!options.browser || !options['build-dir'] || !Object.hasOwn(SAMPLES, options.sample ?? '')) {
		throw new Error('--browser, --build-dir and --sample (minimal/collision/audio/local_multiplayer/components) are required.');
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
	return ['--headless', '--no-remote', '--profile', profile, '--remote-debugging-port=0', 'about:blank'];
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
	let module;
	Object.defineProperty(window, 'Module', {
		configurable: true, enumerable: true,
		get() { return module; },
		set(value) {
			module = value;
			if (!value || typeof value !== 'object' || value.labradorFirefoxHooked) return;
			Object.defineProperty(value, 'labradorFirefoxHooked', { value: true });
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

export function key_actions(keys) {
	return [{ type: 'key', id: 'keyboard', actions: keys.flatMap((key) => {
		if (!Object.hasOwn(KEY_VALUES, key)) throw new Error(`Unsupported sample key ${key}.`);
		return [{ type: 'keyDown', value: KEY_VALUES[key] }, { type: 'pause', duration: 150 },
			{ type: 'keyUp', value: KEY_VALUES[key] }, { type: 'pause', duration: 150 }];
	}) }];
}

const wait = (milliseconds) => new Promise((done) => setTimeout(done, milliseconds));

function frames_expression(count) {
	return `new Promise((done) => {
		let left = ${count};
		const next = () => --left <= 0 ? done(true) : requestAnimationFrame(next);
		requestAnimationFrame(next);
	})`;
}

// Exported for offline control fixtures; this never launches a browser itself.
export async function drive_sample(protocol, context, url, sample,
	{ timeout_ms = 60000, frames = 60, pause = wait, log = console.log } = {}) {
	const deadline = Date.now() + timeout_ms;
	const remaining = () => {
		const duration = deadline - Date.now();
		if (duration <= 0) throw new Error('Firefox sample deadline expired.');
		return Math.min(duration, 10000);
	};
	const complaints = [];
	const unsubscribe = protocol.on((message) => {
		if (message.method === 'log.entryAdded') {
			const entry = message.params;
			log(`Firefox ${entry.level}: ${entry.text ?? '(no text)'}`);
			if (entry.level === 'warn' || entry.level === 'error') complaints.push(`${entry.level}: ${entry.text}`);
		} else if (message.method === 'browsingContext.contextDestroyed' && message.params.context === context) {
			protocol.fail('the tested Firefox context was destroyed');
		}
	});
	const evaluate = (expression) => protocol.evaluate(context, expression, remaining());
	const state = async () => {
		if (complaints.length > 0) throw new Error(`Firefox complaints: ${complaints.join('; ')}`);
		const heard = JSON.parse(await evaluate('JSON.stringify(window.labradorFirefox ?? null)'));
		if (complaints.length > 0) throw new Error(`Firefox complaints: ${complaints.join('; ')}`);
		if (heard?.errors.length > 0) throw new Error(`Module.onError: ${heard.errors.join('; ')}`);
		return heard;
	};
	try {
		await protocol.send('session.subscribe', { events: ['log.entryAdded', 'browsingContext.contextDestroyed'], contexts: [context] });
		await protocol.send('script.addPreloadScript', { functionDeclaration: SAMPLE_HOOK, contexts: [context] });
		await protocol.send('browsingContext.activate', { context });
		await protocol.send('browsingContext.navigate', { context, url, wait: 'complete' });
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

function artifact_files(build_directory, sample_name) {
	const sample = SAMPLES[sample_name];
	const directory = resolve(build_directory, 'samples', sample_name);
	return ['html', 'js', 'wasm'].map((extension) => join(directory, `${sample.target}.${extension}`));
}

async function serve(files) {
	let fatal;
	const failed = new Promise((_, refuse) => { fatal = refuse; });
	failed.catch(() => {});
	const allowed = new Map(files.map((file) => ['/' + basename(file), file]));
	let denied = false;
	const server = createServer((request, response) => {
		if (denied) { response.writeHead(503).end(); return; }
		const wanted = new URL(request.url, 'http://127.0.0.1').pathname;
		if (wanted === '/favicon.ico') { response.writeHead(204).end(); return; }
		const file = allowed.get(wanted);
		if (!file) {
			denied = true;
			response.writeHead(404).end();
			fatal(new Error(`Unexpected sample resource request: ${wanted}`));
			return;
		}
		try {
			const bytes = readFileSync(file);
			response.writeHead(200, { 'Cache-Control': 'no-store', 'Content-Type': file.endsWith('.wasm') ?
				'application/wasm' : file.endsWith('.js') ? 'text/javascript' : 'text/html; charset=utf-8' });
			response.end(bytes);
		} catch (error) {
			denied = true;
			response.writeHead(500).end();
			fatal(new Error(`readFileSync ${file}: ${error.code ?? error.name}: ${error.message}`));
		}
	});
	await new Promise((ready, refused) => {
		server.once('error', refused);
		server.listen(0, '127.0.0.1', ready);
	});
	return { server, failed, url: `http://127.0.0.1:${server.address().port}/${basename(files[0])}` };
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

async function exited(child, milliseconds) {
	if (child.exitCode !== null || child.signalCode !== null) return true;
	let timer;
	return new Promise((done) => {
		const finish = (value) => { clearTimeout(timer); child.off('exit', on_exit); done(value); };
		const on_exit = () => finish(true);
		child.once('exit', on_exit);
		timer = setTimeout(() => finish(false), milliseconds);
	});
}

export async function main(options) {
	const files = artifact_files(options['build-dir'], options.sample);
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
		profile = mkdtempSync(join(tmpdir(), 'labrador-firefox-'));
		service = await serve(files);
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
		await bounded(Promise.race([service.failed,
			drive_sample(protocol, context, service.url, SAMPLES[options.sample], options)]),
		options.timeout_ms, 'Firefox sample timed out.', () => protocol.fail('Firefox sample deadline expired'));
		console.log(`Firefox ${options.sample}: page protocol passed. No WebWindow, listening or physical-device acceptance.`);
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
		if (service) await new Promise((done) => {
			service.server.close(done);
			service.server.closeAllConnections();
		});
		if (profile && closed && (!launched || connected || launched.spawn_failed())) {
			try {
				const absolute = realpathSync(profile);
				if (dirname(absolute) !== realpathSync(tmpdir()) || !basename(absolute).startsWith('labrador-firefox-')) {
					throw new Error(`Refusing cleanup outside the task-owned temporary profile: ${absolute}`);
				}
				rmSync(absolute, { recursive: true, force: true });
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
	try { await main(parse_options(process.argv.slice(2))); }
	catch (error) { console.error(`run_firefox: ${error.stack ?? error}`); process.exitCode = 1; }
}
