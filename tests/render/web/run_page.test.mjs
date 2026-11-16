// run_page.mjs's command line, without a browser: importing it runs nothing.
import assert from 'node:assert/strict';
import { join, resolve } from 'node:path';
import test from 'node:test';
import { NETWORK_PROFILES, USAGE, UsageError, activate_page, chromium_arguments, cleanup_input_driver, driver_page_api, heap_line, page_diagnostic, parse_arguments, unexcused_browser_complaints } from './run_page.mjs';
import { prepare as prepare_iframe } from './iframe_input.mjs';

const page = join('build', 'samples', 'minimal', 'MinimalSample.html');
const sample = ['--browser', 'chrome', '--page', page, '--quit-keys', 'Escape,Enter'];

test('driver cleanup runs once and a cleanup failure cannot leave a successful exit', async () => {
	let cleanups = 0;
	const errors = [];
	assert.equal(await cleanup_input_driver(undefined, (text) => errors.push(text)), true);
	assert.equal(await cleanup_input_driver({ cleanup: async () => { cleanups++; } }, (text) => errors.push(text)), true);
	assert.equal(cleanups, 1);
	assert.equal(await cleanup_input_driver({ cleanup: async () => { throw new Error('owned server did not close'); } },
		(text) => errors.push(text)), false);
	assert.deepEqual(errors, ['run_page: input cleanup: owned server did not close\n']);
});

test('child console, exception, browser errors and crashes retain the top-page failure path', () => {
	const heard = [];
	const complaints = [];
	const crashes = [];
	const output = [];
	const sinks = { console_heard: heard, complaint: (...args) => complaints.push(args),
		crashed: (why) => crashes.push(why), write_out: (text) => output.push(text), write_error: (text) => output.push(text) };
	const messages = [
		{ method: 'Runtime.consoleAPICalled', params: { type: 'error', args: [{ type: 'string', value: 'child console error' }] } },
		{ method: 'Runtime.exceptionThrown', params: { exceptionDetails: { exception: { description: 'child exception' } } } },
		{ method: 'Log.entryAdded', params: { entry: { level: 'error', text: 'child missing file', url: 'http://127.0.0.1/child.js' } } },
		{ method: 'Inspector.targetCrashed', params: {} },
	];
	for (const sessionId of ['child', 'unrelated']) {
		for (const message of messages) page_diagnostic({ ...message, sessionId }, new Set(['parent', 'child']), sinks);
	}
	assert.deepEqual(complaints, [['console', 'error', 'child console error'],
		['exception', 'error', 'child exception'], ['browser', 'error', 'child missing file', { url: 'http://127.0.0.1/child.js' }]]);
	assert.deepEqual(crashes, ['the page crashed']);
	assert.deepEqual(heard, ['child console error\n']);
	assert.equal(output.length, 3);
});

test('a child session keeps commands and events in its own renderer and joins diagnostic monitoring', async () => {
	const commands = [];
	const listeners = new Set();
	const monitored = new Set();
	const devtools = {
		send: async (method, params, session) => {
			commands.push({ method, params, session });
			return { result: { value: 'child result' } };
		},
		on: (listener) => { listeners.add(listener); return () => listeners.delete(listener); },
	};
	const parent = driver_page_api(devtools, 'parent', 'page-target', monitored);
	const child = parent.child_session('iframe');
	assert.deepEqual([...monitored], ['parent', 'iframe']);
	assert.equal(await child.evaluate('location.origin'), 'child result');
	assert.equal(commands[0].session, 'iframe');
	assert.equal(commands[0].params.awaitPromise, true);
	assert.equal(commands[0].params.userGesture, false);
	await child.send('Log.enable');
	assert.equal(commands[1].session, 'iframe');
	await child.send_browser('Target.getTargets');
	assert.equal(commands[2].session, undefined);
	const heard = [];
	const unsubscribe = child.on((event) => heard.push(event.method));
	for (const sessionId of ['parent', 'iframe', 'unrelated']) {
		for (const listener of listeners) listener({ sessionId, method: 'Runtime.exceptionThrown' });
	}
	assert.deepEqual(heard, ['Runtime.exceptionThrown']);
	unsubscribe();
	assert.equal(listeners.size, 0);
	for (const invalid of ['', null, undefined, 1]) assert.throws(() => parent.child_session(invalid), /session id/);
});

test('the iframe is paused until its diagnostics, prelude and factory observation are installed', async () => {
	let attached;
	const commands = [];
	const child = { send: async (method) => commands.push(method), on() {} };
	await prepare_iframe({ on: (listener) => { attached = listener; },
		child_session: (id) => { assert.equal(id, 'child'); return child; },
		send: async (method, params) => {
			if (method === 'Emulation.setDeviceMetricsOverride') {
				assert.deepEqual(params, { width: 1280, height: 1000, deviceScaleFactor: 1, mobile: false });
				return;
			}
			assert.equal(method, 'Target.setAutoAttach');
			assert.deepEqual(params, { autoAttach: true, waitForDebuggerOnStart: true, flatten: true });
		} });
	attached({ method: 'Target.attachedToTarget', params: { sessionId: 'child', targetInfo: { type: 'iframe' } } });
	await new Promise((resolve) => setImmediate(resolve));
	assert.deepEqual(commands, ['Runtime.enable', 'Log.enable', 'Inspector.enable', 'Page.enable',
		'Page.addScriptToEvaluateOnNewDocument', 'Fetch.enable', 'Runtime.runIfWaitingForDebugger']);
});

test('only Windows headless profiles disable native-window occlusion tracking', () => {
	const flag = '--disable-features=CalculateNativeWinOcclusion';
	const windows = chromium_arguments('disposable-profile', [], 'win32');
	assert.equal(windows.filter((argument) => argument === flag).length, 1);
	assert.ok(windows.includes('--headless=new'));
	assert.ok(windows.includes('--user-data-dir=disposable-profile'));
	for (const platform of ['linux', 'darwin']) {
		assert.deepEqual(chromium_arguments('disposable-profile', [], platform),
			windows.filter((argument) => argument !== flag));
	}
});

test('explicit browser arguments keep their order after the platform defaults', () => {
	const extra = ['--disable-gpu-watchdog', '--disable-features=OwnerSelectedFeature'];
	for (const platform of ['win32', 'linux', 'darwin']) {
		const defaults = chromium_arguments('profile', [], platform);
		const args = chromium_arguments('profile', extra, platform);
		assert.deepEqual(args, [...defaults.slice(0, -1), ...extra, 'about:blank']);
	}
	assert.deepEqual(extra, ['--disable-gpu-watchdog', '--disable-features=OwnerSelectedFeature']);
});

test('page activation waits for Page.enable and its own reply before navigation', async () => {
	const commands = [];
	let enabled;
	let activated;
	const enable_reply = new Promise((resolve) => { enabled = resolve; });
	const activate_reply = new Promise((resolve) => { activated = resolve; });
	const send = async (method, params, session) => {
		commands.push({ method, params, session });
		if (method === 'Page.enable') await enable_reply;
		if (method === 'Page.bringToFront') await activate_reply;
	};
	const prepared = (async () => {
		await activate_page(send, 'page-session');
		await send('Page.navigate', { url: 'http://127.0.0.1/test.html' }, 'page-session');
	})();
	assert.deepEqual(commands, [{ method: 'Page.enable', params: {}, session: 'page-session' }]);
	enabled({});
	await new Promise((resolve) => setImmediate(resolve));
	assert.deepEqual(commands.map((command) => command.method), ['Page.enable', 'Page.bringToFront']);
	assert.deepEqual(commands[1], { method: 'Page.bringToFront', params: {}, session: 'page-session' });
	activated({});
	await prepared;
	assert.deepEqual(commands[2], { method: 'Page.navigate',
		params: { url: 'http://127.0.0.1/test.html' }, session: 'page-session' });
});

test('a browser activation refusal propagates instead of navigating an inactive view', async () => {
	const commands = [];
	const refused = new Error('Page.bringToFront: target disappeared');
	const send = async (method) => {
		commands.push(method);
		if (method === 'Page.bringToFront') throw refused;
	};
	await assert.rejects(activate_page(send, 'page-session'), (error) => error === refused);
	assert.deepEqual(commands, ['Page.enable', 'Page.bringToFront']);
});

test('a sample\'s command line gives its keys, media, paths and deadline', () => {
	const parsed = parse_arguments([...sample, '--emulate-media', 'prefers-reduced-motion=reduce',
		'--expect-console', 'motion reduced', '--timeout', '60']);
	assert.deepEqual(parsed.keys, ['Escape', 'Enter']);
	assert.equal(parsed.frame_count, 60);
	assert.deepEqual(parsed.media, { name: 'prefers-reduced-motion', value: 'reduce' });
	assert.equal(parsed['expect-console'], 'motion reduced');
	assert.equal(parsed.page_path, resolve(page));
	assert.equal(parsed.root_path, resolve('build', 'samples', 'minimal'));
	assert.equal(parsed.page_name, 'MinimalSample.html');
	assert.equal(parsed.timeout_ms, 60000);
	const rooted = parse_arguments(['--browser', 'chrome', '--page', page, '--root', 'build']);
	assert.equal(rooted.page_name, 'samples/minimal/MinimalSample.html');
	assert.equal(rooted.timeout_ms, 300000);
});

test('a malformed --emulate-media is refused with the other usage errors, before anything starts', () => {
	for (const media of ['reduce', '=reduce', '']) {
		assert.throws(() => parse_arguments([...sample, '--emulate-media', media]),
			(error) => error instanceof UsageError && /--emulate-media takes <feature>=<value>/.test(error.message));
	}
});

test('an unknown or misspelt option is refused rather than ignored', () => {
	assert.throws(() => parse_arguments([...sample, '--expect-consol', 'motion']),
		(error) => error instanceof UsageError && /unknown option --expect-consol/.test(error.message));
	for (const refused of [['--browser', 'chrome'], [...sample, '--frames', '0'], [...sample, '--collect', 'out'],
		[...sample, 'loose'], ['--browser', 'chrome', '--page', page, '--root', join('elsewhere')]]) {
		assert.throws(() => parse_arguments(refused), UsageError, refused.join(' '));
	}
});

test('--cpu-throttle and --network take a factor and a named profile, and refuse anything else', () => {
	const parsed = parse_arguments([...sample, '--cpu-throttle', '4', '--network', 'slow-4g']);
	assert.equal(parsed.cpu_rate, 4);
	assert.deepEqual(parsed.network_conditions, { offline: false, ...NETWORK_PROFILES['slow-4g'] });
	assert.equal(NETWORK_PROFILES['slow-4g'].latency, 562.5);
	assert.equal(NETWORK_PROFILES['slow-4g'].downloadThroughput, 180000);
	assert.equal(parse_arguments(sample).cpu_rate, undefined);
	assert.equal(parse_arguments(sample).network_conditions, undefined);
	for (const rate of ['0', '0.5', '21', 'fast', '']) {
		assert.throws(() => parse_arguments([...sample, '--cpu-throttle', rate]),
			(error) => error instanceof UsageError && /--cpu-throttle takes/.test(error.message), rate);
	}
	assert.throws(() => parse_arguments([...sample, '--network', '4g']),
		(error) => error instanceof UsageError && /--network takes one of slow-4g/.test(error.message));
});

test('the usage names the options either form takes', () => {
	for (const option of ['--emulate-media', '--expect-console', '--root', '--quit-keys', '--input',
		'--hide-extension', '--sample-input', '--cpu-throttle', '--network', '--query']) {
		assert.match(USAGE, new RegExp(option));
	}
	assert.match(USAGE, /either form: .*--emulate-media.*--expect-console.*--root/);
});

test('a context-lost warning is excused only by the page\'s own report of an unasked loss, one for one', () => {
	const lost = 'WebGL: CONTEXT_LOST_WEBGL: loseContext: context lost';
	const other = 'WebGL: INVALID_OPERATION: bindTexture: object does not belong to this context';
	const report = 'unasked context loss: the browser lost the context before the test asked\n';
	assert.deepEqual(unexcused_browser_complaints([lost, other], []), [lost, other]);
	assert.deepEqual(unexcused_browser_complaints([lost, other], ['ok: a step\n', report]), [other]);
	assert.deepEqual(unexcused_browser_complaints([lost, lost], [report]), [lost]);
	assert.deepEqual(unexcused_browser_complaints([other], [report, report]), [other]);
	assert.deepEqual(unexcused_browser_complaints([lost], [`said: ${report}`]), [lost]);
});

test('a heap line names each module heap in bytes and MiB, a collected one and none at all', () => {
	assert.equal(heap_line('after 60 frames', [16777216]),
		'run_page: wasm heap after 60 frames: 16777216 bytes (16.00 MiB)\n');
	assert.equal(heap_line('at quit', [17891328, null]),
		'run_page: wasm heap at quit: 17891328 bytes (17.06 MiB), collected\n');
	assert.equal(heap_line('at quit', []), 'run_page: wasm heap at quit: no module heap was seen\n');
});

test('--query is appended to the page address, and only a plain query is taken', () => {
	const page = ['--browser', 'chrome', '--page', 'build/Page.html'];
	assert.equal(parse_arguments(page).page_query, '');
	assert.equal(parse_arguments([...page, '--query', 'through_post']).page_query, '?through_post');
	assert.equal(parse_arguments([...page, '--query', 'a=1&b-2']).page_query, '?a=1&b-2');
	for (const query of ['a b', '"x"', 'a/b', '']) {
		assert.throws(() => parse_arguments([...page, '--query', query]),
			(error) => error instanceof UsageError && /--query takes/.test(error.message), query);
	}
});
