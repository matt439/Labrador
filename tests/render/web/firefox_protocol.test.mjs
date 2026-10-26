import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import test from 'node:test';
import vm from 'node:vm';
import { FirefoxProtocol } from './firefox_protocol.mjs';
import { SAMPLES, SAMPLE_HOOK, bidi_endpoint, drive_sample, firefox_arguments,
	key_actions, launch_firefox, parse_options } from './run_firefox.mjs';

class FixtureSocket extends EventTarget {
	constructor(url) {
		super();
		this.url = url;
		this.readyState = 0;
		this.sent = [];
	}
	open() { this.readyState = 1; this.dispatchEvent(new Event('open')); }
	close() { this.readyState = 3; this.dispatchEvent(new Event('close')); }
	send(text) { this.sent.push(JSON.parse(text)); }
	reply(message) { this.raw(JSON.stringify(message)); }
	raw(text) { this.dispatchEvent(new MessageEvent('message', { data: text })); }
}

function connected(options = {}) {
	const protocol = new FirefoxProtocol('ws://fixture/session', { Socket: FixtureSocket, ...options });
	protocol.socket.open();
	return protocol;
}

test('opening is bounded, handles already-open sockets and guards sends until open', async () => {
	const protocol = new FirefoxProtocol('ws://fixture/session', { Socket: FixtureSocket });
	await assert.rejects(protocol.send('session.new'), /session.new:.*not open/);
	const opening = protocol.opened();
	protocol.socket.open();
	await opening;
	await protocol.opened();
	protocol.close();
	await assert.rejects(protocol.opened(), /closed by the client/);
	const slow = new FirefoxProtocol('ws://fixture/session', { Socket: FixtureSocket, timeout_ms: 10 });
	await assert.rejects(slow.opened(), /opening.*timed out after 10 ms/);
	slow.close();
});

test('command ids correlate out-of-order replies and preserve BiDi error code and message', async () => {
	const protocol = connected();
	const first = protocol.send('session.new', { capabilities: {} });
	const second = protocol.send('browsingContext.create', { type: 'tab' });
	assert.deepEqual(protocol.socket.sent, [
		{ id: 1, method: 'session.new', params: { capabilities: {} } },
		{ id: 2, method: 'browsingContext.create', params: { type: 'tab' } },
	]);
	protocol.socket.reply({ type: 'success', id: 2, result: { context: 'tab' } });
	assert.deepEqual(await second, { context: 'tab' });
	protocol.socket.reply({ type: 'success', id: 1, result: { sessionId: 'session' } });
	assert.deepEqual(await first, { sessionId: 'session' });
	const failure = assert.rejects(protocol.send('unsupported.command'), /unsupported.command: unknown command: refused/);
	protocol.socket.reply({ type: 'error', id: 3, error: 'unknown command', message: 'refused' });
	await failure;
	protocol.close();
});

test('evaluation awaits promises and accepts only primitive remote values', async () => {
	const protocol = connected();
	const cases = [
		[{ type: 'string', value: '{"complete":true}' }, '{"complete":true}'],
		[{ type: 'boolean', value: false }, false], [{ type: 'number', value: 3 }, 3],
		[{ type: 'null' }, null], [{ type: 'undefined' }, undefined],
		[{ type: 'number', value: 'NaN' }, NaN], [{ type: 'number', value: 'Infinity' }, Infinity],
		[{ type: 'number', value: '-Infinity' }, -Infinity], [{ type: 'number', value: '-0' }, -0],
	];
	for (const [remote, expected] of cases) {
		const evaluated = protocol.evaluate('tab', 'Promise.resolve(value)');
		const sent = protocol.socket.sent.at(-1);
		assert.deepEqual(sent.params, { expression: 'Promise.resolve(value)', target: { context: 'tab' },
			awaitPromise: true, resultOwnership: 'none' });
		protocol.socket.reply({ type: 'success', id: sent.id,
			result: { type: 'success', realm: 'realm', result: remote } });
		assert.ok(Object.is(await evaluated, expected));
	}
	for (const remote of [{ type: 'object', value: [] }, { type: 'array', value: [] },
		{ type: 'string' }, { type: 'number', value: '3' }, null]) {
		const rejected = assert.rejects(protocol.evaluate('tab', 'value'), /script.evaluate.*(unsupported|malformed)/);
		protocol.socket.reply({ type: 'success', id: protocol.socket.sent.at(-1).id,
			result: { type: 'success', realm: 'realm', result: remote } });
		await rejected;
	}
	protocol.close();
});

test('JavaScript exceptions reject despite a successful command envelope', async () => {
	const protocol = connected();
	const failure = assert.rejects(protocol.evaluate('tab', 'throw new Error()'), /script.evaluate: fixture exception/);
	protocol.socket.reply({ type: 'success', id: 1,
		result: { type: 'exception', realm: 'realm', exceptionDetails: { text: 'fixture exception' } } });
	await failure;
	protocol.close();
});

test('each command times out independently and a late reply cannot settle a later command', async () => {
	const protocol = connected({ timeout_ms: 10 });
	await assert.rejects(protocol.send('session.status'), /session.status: timed out after 10 ms/);
	const later = protocol.send('session.end');
	protocol.socket.reply({ type: 'success', id: 1, result: { late: true } });
	protocol.socket.reply({ type: 'success', id: 2, result: {} });
	assert.deepEqual(await later, {});
	await assert.rejects(protocol.evaluate('tab', 'new Promise(() => {})', 5), /script.evaluate: timed out after 5 ms/);
	protocol.close();
});

test('socket close or error rejects opening and pending commands; late replies are ignored', async () => {
	for (const event of ['close', 'error']) {
		const connecting = new FirefoxProtocol('ws://fixture/session', { Socket: FixtureSocket });
		const opening = assert.rejects(connecting.opened(), /BiDi/);
		connecting.socket.dispatchEvent(new Event(event));
		await opening;
		const protocol = connected();
		const failure = assert.rejects(protocol.send('browsingContext.navigate'), /browsingContext.navigate:.*BiDi/);
		protocol.socket.dispatchEvent(new Event(event));
		await failure;
		protocol.socket.reply({ type: 'success', id: 1, result: {} });
		await assert.rejects(protocol.send('session.end'), /session.end:.*BiDi/);
		protocol.close();
		connecting.close();
	}
});

test('manual refusal clears all waiters and preserves the connection for teardown', async () => {
	const protocol = connected();
	const first = assert.rejects(protocol.send('session.new'), /session.new: fixture stopped/);
	const second = assert.rejects(protocol.send('session.status'), /session.status: fixture stopped/);
	protocol.refuse_all(new Error('fixture stopped'));
	await Promise.all([first, second]);
	const closing = protocol.send('session.end');
	protocol.socket.reply({ type: 'error', id: 1, error: 'unknown error', message: 'late refusal' });
	protocol.socket.reply({ type: 'success', id: 3, result: {} });
	assert.deepEqual(await closing, {});
	protocol.close();
});

test('events are delivered in order and subscriptions can be removed idempotently', () => {
	const protocol = connected();
	const received = [];
	const unsubscribe = protocol.on((message) => received.push(message));
	const event = { type: 'event', method: 'log.entryAdded', params: { level: 'error', text: 'fixture' } };
	protocol.socket.reply(event);
	unsubscribe();
	unsubscribe();
	protocol.socket.reply(event);
	assert.deepEqual(received, [event]);
	protocol.close();
});

test('listener exceptions refuse active and future commands without escaping socket dispatch', async () => {
	const protocol = connected();
	const failure = assert.rejects(protocol.send('browsingContext.navigate'),
		/browsingContext.navigate: BiDi event listener failed: fixture listener failed/);
	protocol.on(() => { throw new Error('fixture listener failed'); });
	let later_listener_called = false;
	protocol.on(() => { later_listener_called = true; });
	assert.doesNotThrow(() => protocol.socket.reply({ type: 'event', method: 'log.entryAdded', params: {} }));
	await failure;
	assert.equal(later_listener_called, false);
	await assert.rejects(protocol.send('session.end'), /session.end: BiDi event listener failed: fixture listener failed/);
	assert.doesNotThrow(() => protocol.socket.reply({ type: 'success', id: 1, result: {} }));
	protocol.close();
});

test('malformed messages reject every active command without throwing from the socket callback', async () => {
	for (const text of ['not JSON', 'null', '[]', '{}',
		'{"type":"success","id":1}',
		'{"type":"error","id":1,"error":{"message":"CDP shape"}}',
		'{"type":"event","method":"log.entryAdded"}',
		'{"type":"success","id":-1,"result":{}}']) {
		const protocol = connected();
		const first = assert.rejects(protocol.send('session.new'), /session.new: malformed BiDi message/);
		const second = assert.rejects(protocol.send('session.status'), /session.status: malformed BiDi message/);
		assert.doesNotThrow(() => protocol.socket.raw(text));
		await Promise.all([first, second]);
		protocol.socket.reply({ type: 'success', id: 1, result: {} });
		protocol.close();
	}
});

test('uncorrelated BiDi errors reject pending commands with the original error description', async () => {
	const protocol = connected();
	const failure = assert.rejects(protocol.send('session.new'), /session.new: BiDi invalid argument: invalid envelope/);
	protocol.socket.reply({ type: 'error', id: null, error: 'invalid argument', message: 'invalid envelope' });
	await failure;
	protocol.close();
});

test('an uncorrelated BiDi error received while idle permanently refuses later work', async () => {
	const protocol = connected();
	protocol.socket.reply({ type: 'error', id: null, error: 'invalid argument', message: 'synthetic idle protocol failure' });
	await assert.rejects(protocol.send('session.status'), /BiDi invalid argument: synthetic idle protocol failure/);
	await assert.rejects(protocol.evaluate('tab', 'true'), /BiDi invalid argument: synthetic idle protocol failure/);
	assert.equal(protocol.socket.sent.length, 0);
	protocol.close();
});

test('synchronous send failures remove their waiter and closing rejects pending work immediately', async () => {
	const protocol = connected();
	protocol.socket.send = () => { throw new Error('fixture send failed'); };
	await assert.rejects(protocol.send('session.new'), /session.new: fixture send failed/);
	assert.equal(protocol.pending.size, 0);
	protocol.socket.send = FixtureSocket.prototype.send;
	const failure = assert.rejects(protocol.send('session.status'), /session.status:.*closed by the client/);
	protocol.close();
	await failure;
	assert.equal(protocol.socket.readyState, 3);
});

test('Firefox CLI is restricted to five samples and uses a fresh-profile launch without security overrides', () => {
	const options = parse_options(['--browser', 'firefox.exe', '--build-dir', 'out/build/wasm-release', '--sample', 'audio']);
	assert.equal(options.frames, 60);
	assert.equal(options.timeout_ms, 60000);
	for (const sample of ['linesweeper', 'WebWindowTests', '__proto__']) {
		assert.throws(() => parse_options(['--browser', 'firefox.exe', '--build-dir', 'build', '--sample', sample]));
	}
	assert.throws(() => parse_options(['--browser', 'firefox.exe', '--page', 'other.html']));
	assert.deepEqual(firefox_arguments('synthetic-owned-profile'), [
		'--headless', '--no-remote', '--profile', 'synthetic-owned-profile', '--remote-debugging-port=0', 'about:blank',
	]);
	assert.equal(bidi_endpoint('noise\nWebDriver BiDi listening on ws://127.0.0.1:46249\n'),
		'ws://127.0.0.1:46249/session');
	assert.equal(bidi_endpoint('WebDriver BiDi listening on ws://127.0.0.1:46'), null);
	assert.equal(bidi_endpoint('DevTools listening on ws://127.0.0.1:1/devtools/browser'), null);
	assert.equal(bidi_endpoint('WebDriver BiDi listening on ws://192.0.2.1:1234'), null);
});

test('preload preserves callbacks and observes real-method submission calls only for the module canvas', () => {
	const calls = [];
	class SyntheticContext {
		clear(...args) { calls.push({ method: 'clear', receiver: this, args }); return 7; }
		drawElements(...args) { calls.push({ method: 'draw', receiver: this, args }); return 23; }
		getParameter(name) { return `synthetic ${name}`; }
	}
	const context = vm.createContext({ WebGL2RenderingContext: SyntheticContext });
	context.window = context;
	vm.runInContext(`(${SAMPLE_HOOK})()`, context);
	const callbackCalls = [];
	const canvas = {};
	const module = { canvas };
	for (const name of ['onRuntimeInitialized', 'onQuit', 'onError']) {
		module[name] = function (...args) { callbackCalls.push({ name, receiver: this, args }); return 42; };
	}
	context.Module = module;
	context.Module = module;
	assert.equal(module.onRuntimeInitialized('ready'), 42);
	const gl = new SyntheticContext();
	gl.canvas = canvas;
	gl.VERSION = 'version'; gl.RENDERER = 'renderer';
	assert.equal(gl.clear(1), 7);
	assert.equal(gl.drawElements(4, 12, 5123, 0), 23);
	const other = new SyntheticContext(); other.canvas = {};
	other.clear(1); other.drawElements(4, 100, 5123, 0);
	assert.equal(module.onQuit('quit'), 42);
	assert.equal(module.onError('fixture error'), 42);
	const heard = JSON.parse(JSON.stringify(context.labradorFirefox));
	assert.equal(heard.initialized, true);
	assert.equal(heard.has_quit, true);
	assert.equal(heard.quit, true);
	assert.deepEqual(heard.errors, ['fixture error']);
	assert.equal(heard.clears, 1);
	assert.equal(heard.draw_calls, 1);
	assert.equal(heard.elements, 12);
	assert.equal(heard.raw_api, 'synthetic version');
	assert.equal(heard.raw_renderer, 'synthetic renderer');
	assert.equal(callbackCalls.length, 3);
	assert.equal(callbackCalls[0].receiver, module);
	assert.deepEqual(callbackCalls[2].args, ['fixture error']);
	assert.equal(calls[1].receiver, gl);
	assert.deepEqual(calls[1].args, [4, 12, 5123, 0]);
	const missing = {};
	context.Module = missing;
	assert.equal(missing.onQuit, undefined);
	assert.equal(context.labradorFirefox.has_quit, false);
});

class SampleControl {
	constructor(options = {}) {
		this.options = options;
		this.calls = [];
		this.reads = 0;
		this.removed = false;
		this.input = false;
		this.heard = { initialized: true, has_quit: true, quit: false, errors: [], clears: 1,
			draw_calls: options.no_draw ? 0 : 1, elements: options.no_draw ? 0 : 6,
			raw_api: 'synthetic', raw_renderer: 'synthetic' };
	}
	on(listener) { this.listener = listener; return () => { this.removed = true; this.listener = null; }; }
	async send(method, params) {
		this.calls.push({ method, params });
		if (method === 'input.performActions') { this.input = true; this.heard.quit = true; }
		return {};
	}
	async evaluate(context, expression) {
		assert.equal(context, 'synthetic-tab');
		if (expression.startsWith('JSON.stringify')) {
			++this.reads;
			if (this.options.late_warning && this.reads === 4) {
				this.listener({ method: 'log.entryAdded', params: { level: 'warn', text: 'synthetic late warning' } });
			}
			if (this.options.error) this.heard.errors = ['synthetic engine error'];
			return JSON.stringify(this.heard);
		}
		assert.match(expression, /requestAnimationFrame/);
		if (!this.input && !this.options.no_draw) { ++this.heard.clears; ++this.heard.draw_calls; }
		if (this.options.early_quit) this.heard.quit = true;
		return true;
	}
}

test('sample control requires continuing draws then uses BiDi key actions before accepting onQuit', async () => {
	const protocol = new SampleControl();
	await drive_sample(protocol, 'synthetic-tab', 'http://127.0.0.1:1/ComponentsGallery.html', SAMPLES.components,
		{ pause: async () => {}, log: () => {} });
	assert.equal(protocol.removed, true);
	const input = protocol.calls.find((call) => call.method === 'input.performActions');
	assert.equal(input.params.context, 'synthetic-tab');
	assert.equal(input.params.actions[0].type, 'key');
	assert.deepEqual(input.params.actions[0].actions.filter((action) => action.type === 'keyDown').map((action) => action.value),
		['\uE015', '\uE015', '\uE012', '\uE013', '\uE014', '\uE00C']);
	assert.equal(protocol.calls.at(-1).method, 'input.releaseActions');
	assert.deepEqual(key_actions(SAMPLES.audio.keys)[0].actions.filter((action) => action.type === 'keyDown')
		.map((action) => action.value), ['s', '\uE00C']);
});

test('sample control refuses absent drawing, early quit, engine errors and a warning arriving during final evaluation', async () => {
	for (const [options, expected] of [[{ no_draw: true }, /deadline expired/],
		[{ early_quit: true }, /quit before trusted key input/], [{ error: true }, /Module.onError/],
		[{ late_warning: true }, /synthetic late warning/]]) {
		const protocol = new SampleControl(options);
		await assert.rejects(drive_sample(protocol, 'synthetic-tab', 'http://127.0.0.1:1/test.html', SAMPLES.minimal,
			{ timeout_ms: options.no_draw ? 10 : 1000, pause: async () => {}, log: () => {} }), expected);
		assert.equal(protocol.removed, true);
		if (!options.late_warning) assert.equal(protocol.input, false);
	}
});

test('owned process exit refuses active protocol work immediately and preserves its exit diagnostic', async () => {
	const child = new EventEmitter();
	child.stderr = new EventEmitter();
	child.pid = 123; child.exitCode = null; child.signalCode = null;
	const launched = launch_firefox('synthetic-firefox', 'synthetic-profile', (browser, args, options) => {
		assert.equal(browser, 'synthetic-firefox');
		assert.equal(args.includes('--no-remote'), true);
		assert.equal(options.windowsHide, true);
		return child;
	});
	let endpoint_settled = false;
	const endpoint = launched.endpoint.then((url) => { endpoint_settled = true; return url; });
	child.stderr.emit('data', Buffer.from('WebDriver BiDi listening on ws://127.0.0.1:46'));
	await Promise.resolve();
	assert.equal(endpoint_settled, false);
	child.stderr.emit('data', Buffer.from('249\r'));
	await Promise.resolve();
	assert.equal(endpoint_settled, false);
	child.stderr.emit('data', Buffer.from('\n'));
	assert.equal(await endpoint, 'ws://127.0.0.1:46249/session');
	const protocol = connected();
	launched.bind(protocol);
	const failure = assert.rejects(protocol.send('browsingContext.navigate'), /Firefox process exited: code=9/);
	child.exitCode = 9;
	child.emit('exit', 9, null);
	assert.equal(protocol.pending.size, 0);
	await failure;
	assert.match(launched.diagnostics(), /pid=123, exit=9/);
	assert.doesNotThrow(() => protocol.socket.reply({ type: 'success', id: 1, result: {} }));
	protocol.close();
});
