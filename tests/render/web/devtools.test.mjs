import assert from 'node:assert/strict';
import test from 'node:test';
import { DevTools, browser_version, is_navigation_race } from './devtools.mjs';

class FixtureSocket extends EventTarget {
	constructor(url) {
		super();
		this.url = url;
		this.sent = [];
	}

	send(text) {
		if (this.refusing) { throw new Error(this.refusing); }
		this.sent.push(JSON.parse(text));
	}
	reply(message) {
		this.dispatchEvent(new MessageEvent('message', { data: JSON.stringify(message) }));
	}
	reply_text(text) {
		this.dispatchEvent(new MessageEvent('message', { data: text }));
	}
}

test('command replies retain their method, id and flattened session', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	const opened = devtools.opened();
	devtools.socket.dispatchEvent(new Event('open'));
	await opened;
	const result = devtools.send('Runtime.evaluate', { expression: '1 + 2' }, 'page-1');
	assert.deepEqual(devtools.socket.sent, [{ id: 1, method: 'Runtime.evaluate',
		params: { expression: '1 + 2' }, sessionId: 'page-1' }]);
	devtools.socket.reply({ id: 1, result: { value: 3 } });
	assert.deepEqual(await result, { value: 3 });
	const failure = devtools.send('Page.navigate');
	const rejected = assert.rejects(failure, /Page.navigate: refused/);
	devtools.socket.reply({ id: 2, error: { message: 'refused' } });
	await rejected;
});

test('late replies after a page crash cannot replace the original failure or break teardown', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	const pending = devtools.send('Inspector.enable', {}, 'page-1');
	const rejected = assert.rejects(pending, /Inspector.enable: the page crashed/);
	devtools.refuse_all('the page crashed');
	await rejected;
	const closing = devtools.send('Browser.close');
	devtools.socket.reply({ id: 1, result: {} });
	devtools.socket.reply({ id: 1, error: { message: 'late error' } });
	devtools.socket.reply({ id: 2, result: { closed: true } });
	assert.deepEqual(await closing, { closed: true });
});

test('a closed socket refuses outstanding requests and tolerates their late replies', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	const pending = devtools.send('Target.getTargets');
	const rejected = assert.rejects(pending, /Target.getTargets: the browser went away/);
	devtools.socket.dispatchEvent(new Event('close'));
	await rejected;
	devtools.socket.reply({ id: 1, result: { targetInfos: [] } });
});

test('session events can be observed and unsubscribed', () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	const messages = [];
	const unsubscribe = devtools.on((message) => messages.push(message));
	const message = { method: 'Fetch.requestPaused', sessionId: 'page-1', params: { requestId: 'r1' } };
	devtools.socket.reply(message);
	unsubscribe();
	devtools.socket.reply(message);
	assert.deepEqual(messages, [message]);
});

test('browser identity keeps the product version and uses the browser session', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	const identity = browser_version(devtools);
	assert.deepEqual(devtools.socket.sent, [{ id: 1, method: 'Browser.getVersion', params: {} }]);
	devtools.socket.reply({ id: 1, result: { product: 'HeadlessChrome/142.0.7444.60',
		protocolVersion: '1.3', revision: '@fixture' } });
	assert.equal(await identity, 'HeadlessChrome/142.0.7444.60 (DevTools 1.3, revision @fixture)');
});

test('each wording of a navigation race is one, and other failures are not', () => {
	for (const message of [
		'Runtime.evaluate: Execution context was destroyed.',
		'Runtime.evaluate: Cannot find context with specified id',
		'Runtime.evaluate: Cannot find default execution context',
		'Runtime.evaluate: Inspected target navigated or closed',
	]) {
		assert.equal(is_navigation_race(new Error(message)), true, message);
	}
	for (const message of ['Runtime.evaluate: Uncaught ReferenceError: Module is not defined',
		'the browser went away']) {
		assert.equal(is_navigation_race(new Error(message)), false, message);
	}
	assert.equal(is_navigation_race(undefined), false);
});

test('missing browser identity fails by the command that returned it', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	const rejected = assert.rejects(browser_version(devtools), /Browser.getVersion.*product\/version/);
	devtools.socket.reply({ id: 1, result: {} });
	await rejected;
});

test('a command after the socket has closed refuses at once, naming its method', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	devtools.socket.dispatchEvent(new Event('close'));
	await assert.rejects(devtools.send('Browser.close'), /^Error: Browser.close: the browser went away$/);
	assert.deepEqual(devtools.socket.sent, []);
	assert.equal(devtools.pending.size, 0);
});

test('a send the socket refuses names its method and leaves no waiter', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	devtools.socket.refusing = 'InvalidStateError: still connecting';
	await assert.rejects(devtools.send('Target.getTargets'), /Target.getTargets: InvalidStateError: still connecting/);
	assert.equal(devtools.pending.size, 0);
});

test('a command given a timeout refuses when no answer comes, and a late answer is dropped', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	await assert.rejects(devtools.send('Page.navigate', { url: 'http://page/' }, 'page-1', { timeout_ms: 5 }),
		/Page.navigate: no answer within 5 ms/);
	assert.equal(devtools.pending.size, 0);
	devtools.socket.reply({ id: 1, result: {} });
	const answered = devtools.send('Page.enable', {}, 'page-1', { timeout_ms: 60000 });
	devtools.socket.reply({ id: 2, result: { enabled: true } });
	assert.deepEqual(await answered, { enabled: true });
});

test('a malformed message or a throwing listener refuses waiting commands without escaping dispatch', async () => {
	const devtools = new DevTools('ws://fixture', FixtureSocket);
	const first = devtools.send('Runtime.enable', {}, 'page-1');
	const malformed = assert.rejects(first, /Runtime.enable: malformed DevTools message/);
	devtools.socket.reply_text('{ not json');
	await malformed;
	devtools.on(() => { throw new Error('listener broke'); });
	const second = devtools.send('Log.enable', {}, 'page-1');
	const thrown = assert.rejects(second, /Log.enable: DevTools event listener failed: listener broke/);
	devtools.socket.reply({ method: 'Runtime.consoleAPICalled', sessionId: 'page-1', params: {} });
	await thrown;
});
