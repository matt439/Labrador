// Explicit local pilot. Importing or planning never boots a Simulator.
import { execFile } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import { startBrowser } from '../../website/acceptance/browsers.mjs';
import { close_server, serve_files } from '../../tests/render/web/page_harness.mjs';
import { COMPLAINT_HOOK, inject_head, sample_hook } from '../../tests/render/web/run_safari.mjs';

const exec = promisify(execFile);
const pause = ms => new Promise(done => setTimeout(done, ms));
const STEM = 'LineSweeperSample';
export const OMITTED = Object.freeze([
	'Physical iPhone, real glass/multitouch, controller and keyboard acceptance',
	'Host-site phone gate, safe-area/rotation/layout judgment and OS tab/background lifecycle',
	'Audible audio, endpoint recovery, peak/long-session memory and repeated-play retention',
	'Physical display pacing, scan-out and a real GPU/process loss',
	'Board/score readback: the play probe observes trusted touch input and continuing play-state draws',
]);

export function parseOptions(argv) {
	const options = { execute: false, timeout: 240 };
	const seen = new Set();
	for (let index = 0; index < argv.length; index++) {
		const name = argv[index].replace(/^--/, '');
		if (!argv[index].startsWith('--') || seen.has(name)) throw new Error(`Unknown or duplicate option: ${argv[index]}`);
		seen.add(name);
		if (name === 'execute') { options.execute = true; continue; }
		if (!['build', 'source-commit', 'udid', 'output', 'timeout'].includes(name) || !argv[index + 1] || argv[index + 1].startsWith('--')) {
			throw new Error(`Unknown option or missing value: ${argv[index]}`);
		}
		options[name] = argv[++index];
	}
	if (!options.build || !options.output || !/^[0-9a-f]{40}$/.test(options['source-commit'] ?? '') ||
		!/^[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}$/i.test(options.udid ?? '')) {
		throw new Error('Give --build <wasm build> --source-commit <full clean commit> --udid <explicit iPhone Simulator UUID> --output <new directory>.');
	}
	options.timeout = Number(options.timeout);
	if (!Number.isInteger(options.timeout) || options.timeout < 30 || options.timeout > 600) throw new Error('--timeout must be an integer in 30–600 seconds, including setup.');
	options.build = path.resolve(options.build);
	options.output = path.resolve(options.output);
	return options;
}

export function artifacts(build) {
	return ['html', 'js', 'wasm'].map(extension => {
		const file = path.join(build, 'samples', 'linesweeper', `${STEM}.${extension}`);
		const size = statSync(file).size;
		if (!statSync(file).isFile() || size === 0 || size > 256 * 1024 * 1024) throw new Error(`Invalid pilot artifact: ${file}`);
		return { file, bytes: size, sha256: createHash('sha256').update(readFileSync(file)).digest('hex') };
	});
}

export function budget(seconds, now = Date.now, signal) {
	const deadline = now() + seconds * 1000;
	return () => {
		if (signal?.aborted) throw new Error('iOS pilot stopped by its caller.');
		const left = deadline - now();
		if (left <= 0) throw new Error('iOS pilot overall deadline expired.');
		return left;
	};
}

// Read the existing ordinary-play build stamp without starting its pacing
// collector. The observer stops on its first callback; no pacing is measured.
export const PILOT_HOOK = `window.labradorIOS = { build: null, touches: [], callbacks: [] };
${sample_hook(false, `
	value.labradorPacingMode = 1;
	value.labradorPacingBuild = (revision, build, backend, api, device) => {
		window.labradorIOS.build = { revision, build, backend, api, device };
	};
	value.labradorPacingFrame = () => 0;
	for (const name of ['onContextLost', 'onContextRestored']) {
		const original = value[name];
		value[name] = function (...args) {
			window.labradorIOS.callbacks.push(name);
			if (typeof original === 'function') return original.apply(this, args);
		};
	}
`)}
for (const type of ['touchstart', 'touchend', 'touchcancel']) {
	document.addEventListener(type, event => {
		if (window.labradorIOS.touches.length < 64) window.labradorIOS.touches.push({ type, trusted: event.isTrusted });
	}, true);
}`;

export const SNAPSHOT = `({ heard: window.labradorSafari, pilot: window.labradorIOS,
	depth: !window.labradorSafari?.quit && typeof Module?._linesweeper_test_stack_depth === 'function' ? Module._linesweeper_test_stack_depth() : null,
	viewport: [innerWidth, innerHeight], dpr: devicePixelRatio,
	buffer: Module?.canvas ? [Module.canvas.width, Module.canvas.height] : null,
	user_agent: navigator.userAgent })`;

// The sample owns these authored control dimensions. A fixture ties this
// small probe to touch_controls.h. Camera scale comes from the actual sample
// map hook, never an independently reconstructed layout.
export const CONTROL = Object.freeze({ size: 136, gap: 12, margin: 16 });
export function pointExpression(target) {
	if (!['pause', 'drop', 'resume', 'quit'].includes(target)) throw new Error(`Unknown pilot target: ${target}`);
	return `(() => {
		const canvas = Module.canvas, box = canvas.getBoundingClientRect();
		const map = (x, y) => { Module._linesweeper_test_map_pointer(x, y); return { ...Module.labradorTestPointerPoint }; };
		let point;
		if (${JSON.stringify(target)} === 'resume' || ${JSON.stringify(target)} === 'quit') {
			point = map(640, ${target === 'quit' ? 454 : 322});
		} else {
			const zero = map(0, 0), one = map(1, 1), scale = one.x - zero.x;
			if (!(scale > 0) || !Number.isFinite(scale)) throw new Error('Invalid sample camera scale');
			const inset = (${CONTROL.margin} + ${CONTROL.size} + ${CONTROL.gap} + ${CONTROL.size} / 2) * scale;
			point = { x: ${target === 'drop' ? 'canvas.width - inset' : 'inset'},
				y: canvas.height - (${CONTROL.margin} + ${CONTROL.size} / 2) * scale };
		}
		const result = { x: Math.floor(box.left + point.x * box.width / canvas.width),
			y: Math.floor(box.top + point.y * box.height / canvas.height) };
		if (!(result.x >= 0 && result.x < innerWidth && result.y >= 0 && result.y < innerHeight)) throw new Error('Pilot target outside viewport');
		return result;
	})()`;
}

export function touchActions(point) {
	if (![point?.x, point?.y].every(Number.isInteger)) throw new Error('Touch target must have integer client coordinates.');
	return [{ type: 'pointer', id: 'pilot-finger', parameters: { pointerType: 'touch' }, actions: [
		{ type: 'pointerMove', duration: 0, origin: 'viewport', ...point },
		{ type: 'pointerDown', button: 0 }, { type: 'pause', duration: 150 }, { type: 'pointerUp', button: 0 },
	] }];
}

async function systemIdentity(remaining, udid) {
	const read = async (file, args) => (await exec(file, args, { timeout: Math.min(10000, remaining()), maxBuffer: 1024 * 1024, signal: remaining.signal })).stdout.trim();
	const macOS = await read('sw_vers', ['-productVersion']);
	const architecture = await read('uname', ['-m']);
	const xcode = await read('xcodebuild', ['-version']);
	const list = JSON.parse(await read('xcrun', ['simctl', 'list', 'devices', 'available', '--json']));
	const selected = Object.entries(list.devices ?? {}).flatMap(([runtime, devices]) =>
		devices.filter(device => device.udid?.toLowerCase() === udid.toLowerCase() && device.isAvailable !== false && device.name?.startsWith('iPhone') && runtime.includes('.iOS-'))
			.map(device => ({ udid: device.udid, name: device.name, runtime, initial_state: device.state })));
	if (selected.length !== 1) throw new Error('The explicit UDID must identify one available iPhone Simulator.');
	return { macOS, architecture, xcode, simulator: selected[0] };
}

export async function drivePilot(session, url, { expectedCommit, remaining, record, screenshot, wait = pause } = {}) {
	const evaluate = expression => session.send('POST', '/execute/sync', { script: `return (${expression});`, args: [] }, { timeoutMs: Math.min(10000, remaining()) });
	const snapshot = () => evaluate(SNAPSHOT);
	const until = async (check, what) => {
		for (;;) {
			remaining();
			const state = await snapshot();
			if (state.heard?.errors?.length) throw new Error(`Module.onError: ${state.heard.errors.join('; ')}`);
			const complaints = await evaluate('window.labradorComplaints ?? []');
			if (complaints.length) throw new Error(`Page complaints: ${complaints.join('; ')}`);
			if (check(state)) return state;
			await wait(Math.min(100, remaining()));
		}
	};
	const step = async (name, action) => {
		const started = Date.now();
		const result = { name, status: 'running' };
		record(result);
		try { result.observed = await action(); result.status = 'passed'; }
		catch (error) { result.status = 'failed'; result.error = error.message; throw error; }
		finally { result.elapsed_ms = Date.now() - started; }
		return result.observed;
	};
	const touch = async target => {
		const point = await evaluate(pointExpression(target));
		const before = (await snapshot()).pilot.touches.length;
		await session.send('POST', '/actions', { actions: touchActions(point) }, { timeoutMs: Math.min(10000, remaining()) });
		await session.send('DELETE', '/actions', undefined, { timeoutMs: Math.min(10000, remaining()) });
		await until(state => state.pilot.touches.slice(before).some(event => event.type === 'touchstart' && event.trusted) &&
			state.pilot.touches.slice(before).some(event => event.type === 'touchend' && event.trusted), `trusted ${target} touch`);
	};
	await step('startup and compiled identity', async () => {
		await session.send('POST', '/url', { url }, { timeoutMs: Math.min(15000, remaining()) });
		const state = await until(state => {
			if (!state.heard?.initialized || !(state.heard.draw_calls > 0)) return false;
			if (state.depth === null || !state.pilot?.build) throw new Error('Development observation exports or compiled build stamp are missing.');
			return state.depth === 1;
		}, 'startup');
		if (!state.heard.has_quit) throw new Error('Development page has no onQuit callback.');
		if (state.pilot.build.revision !== expectedCommit || state.pilot.build.backend !== 'gl' || !['Debug', 'Release'].includes(state.pilot.build.build)) {
			throw new Error(`Compiled source/build mismatch: ${JSON.stringify(state.pilot.build)}`);
		}
		return state;
	});
	await step('touch pause and resume', async () => {
		await touch('pause'); await until(state => state.depth === 2, 'pause');
		await screenshot('paused.png');
		await touch('resume'); return until(state => state.depth === 1, 'resume');
	});
	await step('play touch and continuing draws', async () => {
		const before = (await snapshot()).heard.draw_calls;
		await touch('drop');
		const state = await until(state => state.depth === 1 && state.heard.draw_calls > before + 3, 'play draws');
		await screenshot('play.png');
		return state;
	});
	await step('forced context loss and restoration', async () => {
		await evaluate(`(() => { window.labradorPilotExtension = Module.canvas.getContext('webgl2').getExtension('WEBGL_lose_context');
			if (!window.labradorPilotExtension) throw new Error('WEBGL_lose_context unavailable');
			window.labradorPilotExtension.loseContext(); return true; })()`);
		const lost = await until(state => state.heard.lost > 0 && state.pilot.callbacks.includes('onContextLost'), 'context loss');
		await evaluate('(window.labradorPilotExtension.restoreContext(), true)');
		const restored = await until(state => state.heard.restored > 0 && state.pilot.callbacks.includes('onContextRestored') && state.heard.draw_calls > lost.heard.draw_calls + 3, 'restored draws');
		await screenshot('restored.png');
		return restored;
	});
	await step('touch quit and stopped drawing', async () => {
		if ((await snapshot()).depth === 1) { await touch('pause'); await until(state => state.depth === 2, 'pause before quit'); }
		await touch('quit');
		const stopped = await until(state => state.heard.quit === true, 'onQuit');
		await wait(Math.min(250, remaining()));
		const after = await snapshot();
		if (after.heard.draw_calls !== stopped.heard.draw_calls) throw new Error('Drawing continued after onQuit.');
		return after;
	});
	await until(state => state.heard.quit === true, 'final complaints');
}

export async function runPilot(options, dependencies = {}) {
	const platform = dependencies.platform ?? process.platform;
	if (options.execute && platform !== 'darwin') return { status: 'blocked', reason: 'Execution needs an explicitly authorized macOS host with Xcode, an available iPhone Simulator and enabled safaridriver. This host is ' + platform, omitted: OMITTED };
	if (existsSync(options.output)) throw new Error('The output directory already exists; preserve earlier receipts and choose a new path.');
	const files = artifacts(options.build);
	const report = { schema_version: 1, status: 'planned', evidence: dependencies.fixture ? 'protocol-fixture' : 'iOS-Simulator-only',
		expected_source_commit: options['source-commit'], artifacts: files, selected_simulator: options.udid,
		budget_seconds: options.timeout, omitted: OMITTED, steps: [], logs: [], cleanup: [] };
	if (!options.execute) return report;
	mkdirSync(options.output);
	const started = Date.now();
	const runBudget = budget(options.timeout, dependencies.now, dependencies.signal);
	const remaining = () => { if (serverError) throw serverError; return runBudget(); };
	remaining.signal = dependencies.signal;
	let browser, service, serverError;
	report.started_at = new Date().toISOString();
	const log = line => { if (report.logs.length < 256) report.logs.push(`${Date.now() - started}ms ${String(line).slice(0, 2000)}`); };
	try {
		report.environment = await (dependencies.identity ?? systemIdentity)(remaining, options.udid);
		service = await (dependencies.serve ?? serve_files)(files.map(file => file.file), { transform: html => inject_head(html, [COMPLAINT_HOOK, PILOT_HOOK]) });
		service.failed.catch(error => { serverError = error; });
		browser = await (dependencies.start ?? startBrowser)('ios', { udid: report.environment.simulator.udid, remaining, log, sessionTimeoutMs: 15000, commandTimeoutMs: 5000 });
		report.setup_elapsed_ms = Date.now() - started;
		if (browser.device?.udid?.toLowerCase() !== options.udid.toLowerCase() || browser.session.capabilities.browserName !== 'safari' || browser.session.capabilities.platformName?.toLowerCase() !== 'ios' || !browser.session.capabilities.browserVersion) throw new Error('Driver did not match the requested Safari iOS Simulator.');
		report.browser = { name: browser.session.capabilities.browserName, version: browser.session.capabilities.browserVersion,
			platform: browser.session.capabilities.platformName, simulator: browser.device };
		const screenshot = async name => {
			const encoded = await browser.session.send('GET', '/screenshot', undefined, { timeoutMs: Math.min(5000, remaining()) });
			if (typeof encoded !== 'string') throw new Error('Driver returned a non-string screenshot.');
			const bytes = Buffer.from(encoded, 'base64');
			if (!Buffer.isBuffer(bytes) || bytes.length > 50 * 1024 * 1024 || !bytes.subarray(0, 8).equals(Buffer.from('89504e470d0a1a0a', 'hex'))) throw new Error('Driver returned an invalid screenshot.');
			writeFileSync(path.join(options.output, name), bytes, { flag: 'wx' });
		};
		await drivePilot(browser.session, service.url, { expectedCommit: options['source-commit'], remaining, record: step => report.steps.push(step), screenshot, wait: dependencies.wait });
		remaining();
		if (JSON.stringify(artifacts(options.build)) !== JSON.stringify(files)) throw new Error('Pilot build bytes changed during execution.');
		report.status = 'passed';
	} catch (error) {
		report.setup_elapsed_ms ??= Date.now() - started;
		report.status = 'failed'; report.failure = String(error.stack ?? error);
		if (browser) {
			try { report.failure_snapshot = await browser.session.send('POST', '/execute/sync', { script: `return (${SNAPSHOT});`, args: [] }, { timeoutMs: 5000 }); }
			catch (diagnostic) { report.diagnostic_error = diagnostic.message; }
		}
	} finally {
		if (browser) {
			try { await browser.session.releaseActions(); report.cleanup.push('input released'); } catch (error) { report.cleanup.push(`input release failed: ${error.message}`); report.status = 'failed'; }
			try { await browser.stop(); report.cleanup.push('owned WebDriver/session cleanup finished; selected Simulator may remain booted'); } catch (error) { report.cleanup.push(`driver cleanup failed: ${error.message}`); report.status = 'failed'; }
		}
		if (service) {
			try { await (dependencies.close ?? close_server)(service.server); report.cleanup.push('loopback server stopped'); }
			catch (error) { report.cleanup.push(`loopback cleanup failed: ${error.message}`); report.status = 'failed'; }
		}
		if (report.logs.some(line => /did not exit after kill|deleting the session failed/.test(line))) report.status = 'failed';
		if (serverError) { report.status = 'failed'; report.failure ??= String(serverError.stack ?? serverError); }
		report.elapsed_ms = Date.now() - started;
		writeFileSync(path.join(options.output, 'report.json'), JSON.stringify(report, null, 2) + '\n', { flag: 'wx' });
	}
	return report;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	const controller = new AbortController();
	let stoppedCode;
	for (const [name, code] of [['SIGINT', 130], ['SIGTERM', 143]]) process.once(name, () => { stoppedCode = code; controller.abort(); });
	try {
		const report = await runPilot(parseOptions(process.argv.slice(2)), { signal: controller.signal });
		console.log(JSON.stringify(report, null, 2));
		process.exitCode = stoppedCode ?? (report.status === 'blocked' ? 2 : report.status === 'failed' ? 1 : 0);
	} catch (error) { console.error(`ios_simulator_pilot: ${error.stack ?? error}`); process.exitCode = 2; }
}
