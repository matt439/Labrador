// The WebDriver client and target setup against a fake driver: request bodies,
// element references, error mapping and timeouts. No browser is started.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { test } from 'node:test';
import {
	chooseIosDevice, driverArguments, resolveDriver, safariTabsToLinks, targetCapabilities, TARGETS,
} from '../acceptance/browsers.mjs';
import {
	ELEMENT_KEY, Keys, WebDriverError, command, keyActions, newSession, pressKeys, tap, waitUntil,
} from '../acceptance/webdriver.mjs';

// Answers each request with handler(request) -> { status?, body } or a raw
// string, recording { method, url, body } for every request.
async function fakeDriver(t, handler) {
	const requests = [];
	const server = createServer((request, response) => {
		let text = '';
		request.setEncoding('utf8');
		request.on('data', (chunk) => {
			text += chunk;
		});
		request.on('end', async () => {
			const record = { method: request.method, url: request.url, body: text === '' ? undefined : JSON.parse(text) };
			requests.push(record);
			const answer = await handler(record);
			if (answer === undefined) return;
			response.writeHead(answer.status ?? 200, { 'Content-Type': 'application/json; charset=utf-8' });
			response.end(typeof answer.body === 'string' ? answer.body : JSON.stringify(answer.body));
		});
	});
	await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
	t.after(() => new Promise((resolve) => {
		server.closeAllConnections();
		server.close(resolve);
	}));
	return { url: `http://127.0.0.1:${server.address().port}/`, requests };
}

function ok(value) {
	return { body: { value } };
}

async function sessionOn(t, handler) {
	const driver = await fakeDriver(t, (request) => {
		if (request.method === 'POST' && request.url === '/session') {
			return ok({ sessionId: 'abc', capabilities: { browserName: 'fake', browserVersion: '1' } });
		}
		return handler(request);
	});
	const session = await newSession(driver.url, { browserName: 'fake' });
	return { driver, session };
}

test('newSession sends alwaysMatch capabilities and keeps what the driver matched', async (t) => {
	const { driver, session } = await sessionOn(t, () => ok(null));
	assert.deepEqual(driver.requests[0], {
		method: 'POST', url: '/session', body: { capabilities: { alwaysMatch: { browserName: 'fake' } } },
	});
	assert.equal(session.id, 'abc');
	assert.equal(session.capabilities.browserVersion, '1');
	assert.equal(session.driverUrl, driver.url.replace(/\/$/, ''));
});

test('newSession refuses an answer without a session id', async (t) => {
	const driver = await fakeDriver(t, () => ok({ capabilities: {} }));
	await assert.rejects(newSession(driver.url, {}), (error) => error instanceof WebDriverError && error.code === 'session not created');
});

test('navigate, url, title and deleteSession use the session paths', async (t) => {
	const { driver, session } = await sessionOn(t, (request) => {
		if (request.url === '/session/abc/url' && request.method === 'GET') return ok('http://example.test/docs/');
		if (request.url === '/session/abc/title') return ok('Docs');
		return ok(null);
	});
	await session.navigate('http://example.test/docs/');
	assert.equal(await session.url(), 'http://example.test/docs/');
	assert.equal(await session.title(), 'Docs');
	await session.deleteSession();
	assert.deepEqual(driver.requests.slice(1), [
		{ method: 'POST', url: '/session/abc/url', body: { url: 'http://example.test/docs/' } },
		{ method: 'GET', url: '/session/abc/url', body: undefined },
		{ method: 'GET', url: '/session/abc/title', body: undefined },
		{ method: 'DELETE', url: '/session/abc', body: undefined },
	]);
});

test('executeScript sends the script and arguments and returns the value', async (t) => {
	const element = { [ELEMENT_KEY]: 'e1' };
	const { driver, session } = await sessionOn(t, () => ok({ tag: 'A', count: 2 }));
	assert.deepEqual(await session.executeScript('return 1;', [element, 3]), { tag: 'A', count: 2 });
	assert.deepEqual(driver.requests[1], {
		method: 'POST', url: '/session/abc/execute/sync', body: { script: 'return 1;', args: [{ [ELEMENT_KEY]: 'e1' }, 3] },
	});
	await session.executeScript('return 2;');
	assert.deepEqual(driver.requests[2].body, { script: 'return 2;', args: [] });
});

test('elements are found by CSS and addressed by the W3C element key', async (t) => {
	const { driver, session } = await sessionOn(t, (request) => {
		if (request.url === '/session/abc/element') return ok({ [ELEMENT_KEY]: 'e1' });
		if (request.url === '/session/abc/elements') return ok([{ [ELEMENT_KEY]: 'e2' }, { [ELEMENT_KEY]: 'e3' }]);
		if (request.url === '/session/abc/element/e1/elements') return ok([]);
		if (request.url === '/session/abc/element/active') return ok({ [ELEMENT_KEY]: 'e9' });
		if (request.url === '/session/abc/element/e1/rect') return ok({ x: 1, y: 2, width: 3, height: 4 });
		return ok(null);
	});
	const element = await session.findElement('starlight-version-select select');
	assert.deepEqual(element, { [ELEMENT_KEY]: 'e1' });
	assert.deepEqual((await session.findElements('a')).map((found) => found[ELEMENT_KEY]), ['e2', 'e3']);
	assert.deepEqual(await session.findElements('a', element), []);
	assert.deepEqual(await session.activeElement(), { [ELEMENT_KEY]: 'e9' });
	assert.deepEqual(await session.elementRect(element), { x: 1, y: 2, width: 3, height: 4 });
	await session.click(element);
	await session.sendKeys(element, 'v0.3');
	const bodies = driver.requests.slice(1).map(({ method, url, body }) => [method, url, body]);
	assert.deepEqual(bodies, [
		['POST', '/session/abc/element', { using: 'css selector', value: 'starlight-version-select select' }],
		['POST', '/session/abc/elements', { using: 'css selector', value: 'a' }],
		['POST', '/session/abc/element/e1/elements', { using: 'css selector', value: 'a' }],
		['GET', '/session/abc/element/active', undefined],
		['GET', '/session/abc/element/e1/rect', undefined],
		['POST', '/session/abc/element/e1/click', {}],
		['POST', '/session/abc/element/e1/value', { text: 'v0.3' }],
	]);
	await assert.rejects(session.click({ id: 'e1' }), TypeError);
});

test('window rect, screenshot and timeouts', async (t) => {
	const png = Buffer.from([0x89, 0x50, 0x4e, 0x47]);
	const { driver, session } = await sessionOn(t, (request) => {
		if (request.url === '/session/abc/screenshot') return ok(png.toString('base64'));
		if (request.url === '/session/abc/window/rect') return ok({ x: 0, y: 0, width: 360, height: 800 });
		return ok(null);
	});
	assert.deepEqual(await session.setWindowRect({ width: 360, height: 800 }), { x: 0, y: 0, width: 360, height: 800 });
	assert.deepEqual(driver.requests[1].body, { width: 360, height: 800 });
	assert.deepEqual(await session.screenshot(), png);
	await session.setTimeouts({ script: 5000 });
	assert.deepEqual(driver.requests.at(-1), { method: 'POST', url: '/session/abc/timeouts', body: { script: 5000 } });
});

test('pressKeys sends one key source with W3C code points, then releases', async (t) => {
	const { driver, session } = await sessionOn(t, () => ok(null));
	await pressKeys(session, Keys.Tab, [Keys.Shift, Keys.Tab], Keys.Enter);
	assert.equal(driver.requests[1].method, 'POST');
	assert.equal(driver.requests[1].url, '/session/abc/actions');
	assert.deepEqual(driver.requests[1].body, {
		actions: [{
			type: 'key',
			id: 'keyboard',
			actions: [
				{ type: 'keyDown', value: '\uE004' }, { type: 'keyUp', value: '\uE004' },
				{ type: 'keyDown', value: '\uE008' }, { type: 'keyDown', value: '\uE004' },
				{ type: 'keyUp', value: '\uE004' }, { type: 'keyUp', value: '\uE008' },
				{ type: 'keyDown', value: '\uE007' }, { type: 'keyUp', value: '\uE007' },
			],
		}],
	});
	assert.deepEqual(driver.requests[2], { method: 'DELETE', url: '/session/abc/actions', body: undefined });
});

test('the element key is the specification\'s web element identifier', () => {
	assert.equal(ELEMENT_KEY, 'element-6066-11e4-a52e-4f735466cecf');
});

test('key constants are the specification code points', () => {
	assert.deepEqual(
		[Keys.Tab, Keys.Enter, Keys.Shift, Keys.Escape, Keys.Space, Keys.ArrowUp, Keys.ArrowDown],
		['\uE004', '\uE007', '\uE008', '\uE00C', '\uE00D', '\uE013', '\uE015'],
	);
	assert.deepEqual(keyActions(Keys.Space)[0].actions.map((action) => action.type), ['keyDown', 'keyUp']);
});

test('tap moves a touch pointer to the element, presses and lifts', async (t) => {
	const { driver, session } = await sessionOn(t, () => ok(null));
	await tap(session, { [ELEMENT_KEY]: 'e4' });
	const [source] = driver.requests[1].body.actions;
	assert.equal(source.type, 'pointer');
	assert.deepEqual(source.parameters, { pointerType: 'touch' });
	assert.deepEqual(source.actions[0], { type: 'pointerMove', duration: 0, origin: { [ELEMENT_KEY]: 'e4' }, x: 0, y: 0 });
	assert.deepEqual(source.actions.map((action) => action.type), ['pointerMove', 'pointerDown', 'pause', 'pointerUp']);
	assert.equal(driver.requests[2].method, 'DELETE');
});

test('a driver error becomes a WebDriverError with its code and message', async (t) => {
	const { session } = await sessionOn(t, () => ({
		status: 404, body: { value: { error: 'no such element', message: 'Unable to locate element: nav', stacktrace: '' } },
	}));
	await assert.rejects(session.findElement('nav'), (error) => {
		assert.ok(error instanceof WebDriverError);
		assert.equal(error.code, 'no such element');
		assert.equal(error.driverMessage, 'Unable to locate element: nav');
		assert.equal(error.status, 404);
		assert.match(error.message, /^POST \/session\/abc\/element: no such element: Unable to locate element: nav$/);
		return true;
	});
});

test('an error value is honoured even with a success status', async (t) => {
	const driver = await fakeDriver(t, () => ok({ error: 'unsupported operation', message: 'not on this device' }));
	await assert.rejects(command(driver.url, 'POST', '/session/x/window/rect', {}), { code: 'unsupported operation' });
});

test('a body that is not JSON and a bare HTTP failure are unknown errors', async (t) => {
	const driver = await fakeDriver(t, (request) => (request.url === '/text'
		? { status: 500, body: 'Internal failure' }
		: { status: 503, body: { value: null } }));
	await assert.rejects(command(driver.url, 'GET', '/text'), (error) =>
		error.code === 'unknown error' && error.status === 500 && /not JSON: Internal failure/.test(error.message));
	await assert.rejects(command(driver.url, 'GET', '/other'), (error) => error.code === 'unknown error' && error.status === 503);
});

test('a command that gets no answer in time fails as a client timeout', async (t) => {
	const driver = await fakeDriver(t, () => undefined);
	await assert.rejects(command(driver.url, 'GET', '/status', undefined, { timeoutMs: 100 }), { code: 'client timeout' });
});

test('an unreachable driver fails as a connection failure', async (t) => {
	const driver = await fakeDriver(t, () => ok(null));
	const closed = driver.url.replace(/:\d+\//, ':1/');
	await assert.rejects(command(closed, 'GET', '/status', undefined, { timeoutMs: 2000 }), { code: 'connection failed' });
});

test('waitUntil returns the first truthy value or times out', async () => {
	let calls = 0;
	assert.equal(await waitUntil(async () => (++calls >= 3 ? 'done' : null), { intervalMs: 1 }), 'done');
	await assert.rejects(waitUntil(() => false, { timeoutMs: 20, intervalMs: 5, message: 'never' }), /waiting for never/);
});

test('each target declares the flags the suite consults', () => {
	for (const flags of Object.values(TARGETS)) {
		assert.deepEqual(Object.keys(flags).sort(), ['keyboard', 'phone', 'setWindowRect', 'touch']);
	}
	assert.equal(TARGETS.ios.setWindowRect, false);
	assert.equal(TARGETS.ios.phone, true);
});

test('Chrome capabilities are headless, sized, and sandboxed except on Linux CI', () => {
	const local = targetCapabilities('chrome', { env: {}, platform: 'win32', window: { width: 1000, height: 700 } });
	assert.deepEqual(local, {
		browserName: 'chrome', 'goog:chromeOptions': { args: ['--headless=new', '--window-size=1000,700'] },
	});
	const ci = targetCapabilities('chrome', { env: { CI: 'true', LABRADOR_ACCEPTANCE_CHROME: '/opt/chrome' }, platform: 'linux' });
	assert.ok(ci['goog:chromeOptions'].args.includes('--no-sandbox'));
	assert.equal(ci['goog:chromeOptions'].binary, '/opt/chrome');
	assert.ok(!targetCapabilities('chrome', { env: { CI: 'true' }, platform: 'win32' })['goog:chromeOptions'].args.includes('--no-sandbox'));
	// The CMake browser tests' Chromium is often another major version than the driver.
	assert.equal(targetCapabilities('chrome', { env: { LABRADOR_TEST_BROWSER: '/opt/chromium' } })['goog:chromeOptions'].binary, undefined);
});

test('Safari tabs to links only when its preference or the override says so', async () => {
	const none = async () => null;
	assert.equal(await safariTabsToLinks({ env: { HOME: '/Users/r' }, read: none }), false);
	assert.equal(await safariTabsToLinks({ env: { LABRADOR_SAFARI_TAB_TO_LINKS: '1' }, read: none }), true);
	const asked = [];
	const on = async (domain, key) => {
		asked.push(domain);
		return domain.includes('Containers') && key === 'WebKitTabToLinksPreferenceKey' ? '1' : null;
	};
	assert.equal(await safariTabsToLinks({ env: { HOME: '/Users/r' }, read: on }), true);
	assert.ok(asked[0].includes('Library/Containers/com.apple.Safari'));
	assert.equal(await safariTabsToLinks({ env: { LABRADOR_SAFARI_TAB_TO_LINKS: '0' }, read: on }), false);
	assert.equal(await safariTabsToLinks({ env: {}, read: async () => '0' }), false);
});

test('Firefox, Safari and iOS capabilities', () => {
	assert.deepEqual(targetCapabilities('firefox', { env: { LABRADOR_ACCEPTANCE_FIREFOX: 'C:/ff/firefox.exe' } }), {
		browserName: 'firefox', 'moz:firefoxOptions': { args: ['-headless'], binary: 'C:/ff/firefox.exe' },
	});
	assert.deepEqual(targetCapabilities('safari', { env: {} }), { browserName: 'safari' });
	assert.deepEqual(targetCapabilities('ios', { udid: 'U-1', env: {} }), {
		browserName: 'safari', platformName: 'iOS', 'safari:useSimulator': true, 'safari:deviceUDID': 'U-1',
	});
	assert.throws(() => targetCapabilities('ios', { env: {} }), /UDID/);
	assert.throws(() => targetCapabilities('edge', { env: {} }), /Unknown target/);
});

test('drivers come from an explicit path, the runner folder, then PATH', () => {
	assert.equal(resolveDriver('chrome', { driver: '/x/chromedriver', env: {} }), '/x/chromedriver');
	assert.equal(resolveDriver('chrome', { env: {}, platform: 'win32' }), 'chromedriver.exe');
	assert.equal(resolveDriver('firefox', { env: { GECKOWEBDRIVER: '/no/such/folder' }, platform: 'linux' }), 'geckodriver');
	assert.equal(resolveDriver('ios', { env: {}, platform: 'darwin' }), 'safaridriver');
	assert.deepEqual(driverArguments('chrome', 9515), ['--port=9515']);
	assert.deepEqual(driverArguments('safari', 4444), ['-p', '4444']);
	assert.deepEqual(driverArguments('firefox', 4444).slice(0, 2), ['--port', '4444']);
});

test('the ios target picks the newest runtime\'s first available iPhone', () => {
	const list = {
		devices: {
			'com.apple.CoreSimulator.SimRuntime.iOS-18-2': [{ udid: 'old', name: 'iPhone 16', state: 'Shutdown', isAvailable: true }],
			'com.apple.CoreSimulator.SimRuntime.iOS-26-5': [
				{ udid: 'pad', name: 'iPad Pro 13-inch (M5)', state: 'Shutdown', isAvailable: true },
				{ udid: 'gone', name: 'iPhone 17', state: 'Shutdown', isAvailable: false },
				{ udid: 'new', name: 'iPhone 17 Pro', state: 'Shutdown', isAvailable: true },
			],
			'com.apple.CoreSimulator.SimRuntime.iOS-26-4-1': [{ udid: 'mid', name: 'iPhone Air', state: 'Booted', isAvailable: true }],
			'com.apple.CoreSimulator.SimRuntime.watchOS-26-0': [{ udid: 'watch', name: 'iPhone-looking watch', isAvailable: true }],
		},
	};
	assert.deepEqual(chooseIosDevice(list), {
		udid: 'new', name: 'iPhone 17 Pro', state: 'Shutdown', runtime: 'com.apple.CoreSimulator.SimRuntime.iOS-26-5',
	});
	assert.equal(chooseIosDevice({ devices: {} }), null);
});
