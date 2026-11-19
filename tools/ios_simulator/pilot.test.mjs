import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync, existsSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import vm from 'node:vm';
import { prepareSimulator, startBrowser, startDriver, withinBudget } from '../../website/acceptance/browsers.mjs';
import { CONTROL, PILOT_HOOK, SNAPSHOT, artifacts, budget, parseOptions, pointExpression, runPilot, touchActions } from './pilot.mjs';

const COMMIT = 'a'.repeat(40);
const UDID = '12345678-1234-1234-1234-123456789abc';
function fixture(t) {
	const root = mkdtempSync(path.join(os.tmpdir(), 'labrador-ios-pilot-'));
	t.after(() => {
		assert.equal(path.dirname(path.resolve(root)), path.resolve(os.tmpdir()));
		assert.ok(path.basename(root).startsWith('labrador-ios-pilot-'));
		rmSync(root, { recursive: true, force: true });
	});
	const folder = path.join(root, 'samples', 'linesweeper');
	mkdirSync(folder, { recursive: true });
	for (const extension of ['html', 'js', 'wasm']) writeFileSync(path.join(folder, `LineSweeperSample.${extension}`), 'fixture-' + extension);
	return { root, options: parseOptions(['--build', root, '--source-commit', COMMIT, '--udid', UDID, '--output', path.join(root, 'receipt'), '--execute']) };
}

function runner(settings = {}) {
	const calls = [];
	let clock = 0;
	const state = {
		heard: { initialized: !settings.uninitialized, has_quit: !settings.noQuitCallback, quit: false, errors: [], draw_calls: 1, lost: 0, restored: 0 },
		pilot: { touches: [], callbacks: [], build: { revision: settings.revision ?? COMMIT, build: 'Release', backend: 'gl', api: 'fixture', device: 'fixture' } },
		depth: settings.missingExports ? null : 1, viewport: [390, 800], dpr: 3, buffer: [1170, 2400], user_agent: 'fixture, not Safari evidence',
	};
	let lost = false;
	const targets = ['pause', 'drop', 'resume', 'quit'];
	const session = {
		capabilities: { browserName: settings.browser ?? 'safari', browserVersion: 'fixture', platformName: 'iOS' },
		async send(method, command, body, options) {
			calls.push({ method, command, body, options });
			if (command === '/url') return;
			if (command === '/screenshot') {
				assert.equal(method, 'GET'); settings.onScreenshot?.();
				return Buffer.from('89504e470d0a1a0a', 'hex').toString('base64');
			}
			if (command === '/actions' && method === 'POST') {
				assert.equal(body.actions[0].parameters.pointerType, 'touch');
				assert.equal(body.actions[0].actions[0].origin, 'viewport');
				const target = targets[body.actions[0].actions[0].x - 100];
				state.pilot.touches.push({ type: 'touchstart', trusted: !settings.untrusted }, { type: 'touchend', trusted: !settings.untrusted });
				if (target === 'pause') { state.depth = 2; if (settings.nearScreenshotExpiry) clock = 239950; }
				if (target === 'resume') state.depth = 1;
				if (target === 'quit' && !settings.noQuit) { state.heard.quit = true; state.depth = 0; }
				return;
			}
			if (command === '/actions') return;
			assert.equal(command, '/execute/sync');
			const expression = body.script.slice(8, -2);
			if (expression === SNAPSHOT) {
				if (!lost && !state.heard.quit) state.heard.draw_calls += 2;
				return structuredClone(state);
			}
			if (expression === 'window.labradorComplaints ?? []') return settings.complaints ?? [];
			for (const [index, target] of targets.entries()) if (expression === pointExpression(target)) return { x: 100 + index, y: 200 };
			if (expression.includes('.loseContext()')) {
				if (settings.noExtension) throw new Error('WEBGL_lose_context unavailable');
				lost = true; ++state.heard.lost; state.pilot.callbacks.push('onContextLost'); return true;
			}
			if (expression.includes('.restoreContext()')) {
				if (!settings.noRestore) { lost = false; ++state.heard.restored; state.pilot.callbacks.push('onContextRestored'); }
				return true;
			}
			throw new Error('Unrecognized fixture expression: ' + expression);
		},
		async releaseActions() { calls.push('release'); },
	};
	return { calls, state, dependencies: {
		platform: 'darwin', fixture: true,
		now: () => clock,
		wait: async ms => { clock += settings.fastTimeout ? 60000 : ms; },
		identity: async (remaining, udid) => { calls.push('identity'); remaining(); assert.equal(udid, UDID); return { macOS: 'fixture', xcode: 'fixture', simulator: { udid } }; },
		serve: async (files, { transform }) => {
			calls.push('serve'); assert.equal(files.length, 3);
			assert.ok(transform('<html><head></head></html>').includes(PILOT_HOOK));
			return { url: 'http://127.0.0.1:1234/LineSweeperSample.html', server: {}, failed: new Promise(() => {}) };
		},
		start: async (target, options) => {
			calls.push('start'); assert.equal(target, 'ios'); assert.equal(options.udid, UDID); assert.ok(options.remaining() > 0);
			if (settings.startFailure) throw new Error('fixture driver failed');
			return { session, device: { udid: UDID, name: 'fixture iPhone', runtime: 'fixture iOS' }, async stop() { calls.push('stop'); } };
		},
		close: async () => { calls.push('close'); },
	} };
}

test('planning makes no driver/Simulator/output changes, and Windows execution is blocked', async t => {
	const { options } = fixture(t);
	const fake = runner();
	const plan = await runPilot({ ...options, execute: false }, fake.dependencies);
	assert.equal(plan.status, 'planned'); assert.equal(plan.artifacts.length, 3);
	assert.deepEqual(fake.calls, []); assert.equal(existsSync(options.output), false);
	const blocked = await runPilot(options, { ...fake.dependencies, platform: 'win32' });
	assert.equal(blocked.status, 'blocked'); assert.match(blocked.reason, /macOS.*Xcode.*Simulator.*safaridriver/);
	assert.deepEqual(fake.calls, []); assert.equal(existsSync(options.output), false);
});

test('the fixture drives trusted touch, matching compiled identity, lifecycle and quit with cleanup', async t => {
	const { options } = fixture(t); const fake = runner();
	const report = await runPilot(options, fake.dependencies);
	assert.equal(report.status, 'passed'); assert.equal(report.evidence, 'protocol-fixture');
	assert.equal(report.steps.length, 5); assert.ok(report.steps.every(step => step.status === 'passed'));
	assert.equal(report.browser.version, 'fixture'); assert.ok(report.omitted.some(value => value.includes('Board/score')));
	assert.deepEqual(fake.calls.slice(-3), ['release', 'stop', 'close']);
	assert.equal(JSON.parse(readFileSync(path.join(options.output, 'report.json'), 'utf8')).status, 'passed');
	assert.ok(existsSync(path.join(options.output, 'restored.png')));
	await assert.rejects(() => runPilot(options, fake.dependencies), /already exists/);
});

for (const [name, settings, reason] of [
	['missing development exports', { missingExports: true }, /observation exports/],
	['missing page callback', { noQuitCallback: true }, /no onQuit callback/],
	['wrong source', { revision: 'b'.repeat(40) }, /Compiled source/],
	['dirty source', { revision: COMMIT + '-dirty' }, /Compiled source/],
	['wrong browser', { browser: 'firefox' }, /did not match/],
	['page complaint', { complaints: ['fixture warning'] }, /Page complaints/],
	['untrusted touch', { untrusted: true, fastTimeout: true }, /deadline/],
	['startup stalls', { uninitialized: true, fastTimeout: true }, /deadline/],
	['quit stalls', { noQuit: true, fastTimeout: true }, /deadline/],
	['restoration stalls', { noRestore: true, fastTimeout: true }, /deadline/],
	['missing loss extension', { noExtension: true }, /WEBGL_lose_context/],
]) test(`${name} fails with diagnostics and stops owned resources`, async t => {
	const { options } = fixture(t); const fake = runner(settings);
	const report = await runPilot(options, fake.dependencies);
	assert.equal(report.status, 'failed'); assert.match(report.failure, reason);
	assert.ok(report.failure_snapshot); assert.deepEqual(fake.calls.slice(-3), ['release', 'stop', 'close']);
	assert.equal(JSON.parse(readFileSync(path.join(options.output, 'report.json'), 'utf8')).status, 'failed');
});

test('failure before a session still closes its loopback server and retains the reason', async t => {
	const { options } = fixture(t); const fake = runner({ startFailure: true });
	const report = await runPilot(options, fake.dependencies);
	assert.equal(report.status, 'failed'); assert.match(report.failure, /driver failed/);
	assert.equal(fake.calls.at(-1), 'close'); assert.ok(!fake.calls.includes('stop'));
});

test('changed artifact bytes cannot be reported as the original build', async t => {
	const { options } = fixture(t); const wasm = path.join(options.build, 'samples', 'linesweeper', 'LineSweeperSample.wasm');
	const fake = runner({ onScreenshot: () => writeFileSync(wasm, 'changed fixture') });
	const report = await runPilot(options, fake.dependencies);
	assert.equal(report.status, 'failed'); assert.match(report.failure, /build bytes changed/);
});

test('options and the shared retry budget refuse ambiguous identities and unbounded execution', () => {
	for (const argv of [[], ['--execute'], ['--browser', 'firefox'], ['--build', 'a', '--build', 'b'], ['--execute', '--execute']]) assert.throws(() => parseOptions(argv));
	const valid = ['--build', 'b', '--source-commit', COMMIT, '--udid', UDID, '--output', 'out/receipt'];
	assert.throws(() => parseOptions([...valid, '--timeout', 'Infinity']), /30–600/);
	assert.throws(() => parseOptions(valid.map(value => value === COMMIT ? 'short' : value)), /full clean commit/);
	let clock = 0; const remaining = budget(1, () => clock);
	assert.equal(withinBudget(10000, remaining), 1000); clock = 999;
	assert.equal(withinBudget(10000, remaining), 1); clock = 1000;
	assert.throws(() => withinBudget(10000, remaining), /deadline/);
	assert.throws(() => touchActions({ x: 1.5, y: 1 }), /integer/);
	assert.throws(() => pointExpression('unrecognized'), /Unknown/);
});

test('the probe tracks authored control sizes and uses observation hooks rather than injecting game actions', () => {
	const source = readFileSync(new URL('../../samples/linesweeper/presentation/touch_controls.h', import.meta.url), 'utf8');
	for (const [name, value] of Object.entries(CONTROL)) assert.ok(source.includes(`control_${name} = ${value}.0f;`));
	assert.ok(PILOT_HOOK.includes('value.labradorPacingMode = 1;'));
	assert.ok(!PILOT_HOOK.includes('dispatchEvent'));
	assert.ok(!PILOT_HOOK.includes('labradorTestInput ='));
	assert.ok(pointExpression('pause').includes('_linesweeper_test_map_pointer'));
	assert.ok(SNAPSHOT.includes('!window.labradorSafari?.quit'));
});

test('the pilot hook adds observations without replacing Safari sample callbacks', () => {
	const listeners = new Map();
	class WebGL2RenderingContext { clear() {} drawElements() {} }
	const document = { addEventListener: (name, listener) => listeners.set(name, listener) };
	const sandbox = vm.createContext({ document, WebGL2RenderingContext });
	sandbox.window = sandbox;
	vm.runInContext(PILOT_HOOK, sandbox);
	const original = [];
	sandbox.Module = {
		onContextLost() { original.push('lost'); },
		onContextRestored() { original.push('restored'); },
		onQuit() { original.push('quit'); },
	};
	sandbox.Module.onRuntimeInitialized();
	assert.equal(sandbox.labradorSafari.initialized, true);
	assert.equal(sandbox.Module.labradorPacingMode, 1);
	sandbox.Module.labradorPacingBuild(COMMIT, 'Release', 'gl', 'fixture API', 'fixture device');
	assert.equal(sandbox.labradorIOS.build.revision, COMMIT);
	assert.equal(sandbox.Module.labradorPacingFrame(), 0);
	listeners.get('touchstart')({ isTrusted: true });
	listeners.get('touchend')({ isTrusted: false });
	assert.deepEqual(Array.from(sandbox.labradorIOS.touches, touch => [touch.type, touch.trusted]),
		[['touchstart', true], ['touchend', false]]);
	sandbox.Module.onContextLost();
	sandbox.Module.onContextRestored();
	assert.deepEqual(Array.from(sandbox.labradorIOS.callbacks), ['onContextLost', 'onContextRestored']);
	sandbox.Module.onQuit();
	assert.deepEqual(original, ['lost', 'restored', 'quit']);
	assert.equal(sandbox.labradorSafari.quit, true);
	assert.equal(sandbox.labradorSafari.quit_key_count, 0);
});

test('expired setup budgets refuse before invoking xcrun or spawning a driver', async () => {
	const remaining = () => { throw new Error('fixture deadline expired'); };
	await assert.rejects(() => prepareSimulator({ udid: UDID, remaining }), /deadline expired/);
	await assert.rejects(() => startBrowser('ios', { udid: UDID, remaining }), /deadline expired/);
	await assert.rejects(() => startDriver('must-not-be-started', 'ios', { remaining }), /deadline expired/);
});

test('caller cancellation is recorded before any Simulator session starts', async t => {
	const { options } = fixture(t); const fake = runner();
	const controller = new AbortController(); controller.abort();
	const report = await runPilot(options, { ...fake.dependencies, signal: controller.signal });
	assert.equal(report.status, 'failed'); assert.match(report.failure, /stopped by its caller/);
	assert.deepEqual(fake.calls, ['identity']);
});

test('cleanup failure preserves a failed receipt instead of losing the report', async t => {
	const { options } = fixture(t); const fake = runner();
	const report = await runPilot(options, { ...fake.dependencies, close: async () => { throw new Error('fixture close failure'); } });
	assert.equal(report.status, 'failed'); assert.ok(report.cleanup.some(line => line.includes('close failure')));
	assert.equal(JSON.parse(readFileSync(path.join(options.output, 'report.json'), 'utf8')).status, 'failed');
});

test('a screenshot receives only the overall time still available', async t => {
	const { options } = fixture(t); const fake = runner({ nearScreenshotExpiry: true });
	const report = await runPilot(options, fake.dependencies);
	const screenshot = fake.calls.find(call => call.command === '/screenshot');
	assert.equal(screenshot.options.timeoutMs, 50);
	assert.equal(report.status, 'failed'); assert.match(report.failure, /deadline/);
});
