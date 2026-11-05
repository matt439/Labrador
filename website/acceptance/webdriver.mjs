// A W3C WebDriver (classic, HTTP+JSON) client for the acceptance suite. It
// speaks only the standard protocol, so chromedriver, geckodriver and
// safaridriver, desktop or iOS Simulator, are driven by the same calls.
// Every command has its own timeout: a driver that stops answering fails the
// check that used it instead of hanging the job. Failures are WebDriverError
// values carrying the protocol's error code (such as "no such element") and
// the driver's message.
import { describeError } from '../scripts/snapshot-source.mjs';

// The W3C key under which a driver returns an element reference. An element
// is passed around as that one-key object, so it can also be handed straight
// to executeScript as an argument.
export const ELEMENT_KEY = 'element-6066-11e4-a52e-4f735466cecf';

export const DEFAULT_COMMAND_TIMEOUT_MS = 30_000;
// Creating a session starts the browser; the iOS Simulator's Safari has taken
// over half a minute.
export const DEFAULT_SESSION_TIMEOUT_MS = 120_000;

// Code points from the WebDriver specification's key table.
export const Keys = Object.freeze({
	Tab: '\uE004',
	Enter: '\uE007',
	Shift: '\uE008',
	// Option on a Mac.
	Alt: '\uE00A',
	Escape: '\uE00C',
	Space: '\uE00D',
	ArrowLeft: '\uE012',
	ArrowUp: '\uE013',
	ArrowRight: '\uE014',
	ArrowDown: '\uE015',
});

export class WebDriverError extends Error {
	constructor(code, message, { status, method, path: commandPath, cause } = {}) {
		super(`${method ?? '?'} ${commandPath ?? '?'}: ${code}: ${message}`, cause === undefined ? undefined : { cause });
		this.name = 'WebDriverError';
		// The specification's error code, or "client timeout" when no answer
		// arrived in time, or "connection failed" when the driver was unreachable.
		this.code = code;
		this.driverMessage = message;
		this.status = status;
	}
}

function trimSlash(url) {
	return String(url).replace(/\/+$/, '');
}

// One HTTP round trip. A JSON body's value is returned; a body whose value
// carries an error, or any non-2xx status, becomes a WebDriverError.
export async function command(baseUrl, method, commandPath, body, { timeoutMs = DEFAULT_COMMAND_TIMEOUT_MS } = {}) {
	const context = { method, path: commandPath };
	let response;
	try {
		response = await fetch(`${trimSlash(baseUrl)}${commandPath}`, {
			method,
			headers: body === undefined ? undefined : { 'Content-Type': 'application/json; charset=utf-8' },
			body: body === undefined ? undefined : JSON.stringify(body),
			signal: AbortSignal.timeout(timeoutMs),
		});
	} catch (error) {
		if (error?.name === 'TimeoutError' || error?.name === 'AbortError') {
			throw new WebDriverError('client timeout', `no answer within ${timeoutMs} ms`, { ...context, cause: error });
		}
		throw new WebDriverError('connection failed', describeError(error.cause ?? error), { ...context, cause: error });
	}
	let text;
	try {
		text = await response.text();
	} catch (error) {
		const code = error?.name === 'TimeoutError' ? 'client timeout' : 'connection failed';
		throw new WebDriverError(code, describeError(error), { ...context, status: response.status, cause: error });
	}
	let parsed;
	try {
		parsed = text === '' ? {} : JSON.parse(text);
	} catch {
		throw new WebDriverError('unknown error', `HTTP ${response.status} with a body that is not JSON: ${text.slice(0, 200)}`,
			{ ...context, status: response.status });
	}
	const value = parsed?.value;
	if (value !== null && typeof value === 'object' && typeof value.error === 'string') {
		throw new WebDriverError(value.error, String(value.message ?? ''), { ...context, status: response.status });
	}
	if (!response.ok) {
		throw new WebDriverError('unknown error', `HTTP ${response.status}: ${text.slice(0, 200)}`, { ...context, status: response.status });
	}
	return value;
}

export function elementId(element) {
	const id = element?.[ELEMENT_KEY];
	if (typeof id !== 'string') throw new TypeError('Not a WebDriver element reference.');
	return id;
}

function reference(value) {
	elementId(value);
	return { [ELEMENT_KEY]: value[ELEMENT_KEY] };
}

export class Session {
	constructor(driverUrl, id, capabilities, { timeoutMs = DEFAULT_COMMAND_TIMEOUT_MS } = {}) {
		this.driverUrl = trimSlash(driverUrl);
		this.id = id;
		// What the driver reports it matched, such as browserVersion.
		this.capabilities = capabilities;
		this.timeoutMs = timeoutMs;
	}

	send(method, commandPath, body, options = {}) {
		return command(this.driverUrl, method, `/session/${encodeURIComponent(this.id)}${commandPath}`, body,
			{ timeoutMs: options.timeoutMs ?? this.timeoutMs });
	}

	element(element) {
		return `/element/${encodeURIComponent(elementId(element))}`;
	}

	// Returns once the driver's page load strategy is satisfied.
	async navigate(url) {
		await this.send('POST', '/url', { url });
	}

	url() {
		return this.send('GET', '/url');
	}

	title() {
		return this.send('GET', '/title');
	}

	async refresh() {
		await this.send('POST', '/refresh', {});
	}

	// The script is a function body: arguments arrive as `arguments`, and its
	// return value, including element references, comes back as JSON.
	executeScript(script, args = []) {
		return this.send('POST', '/execute/sync', { script, args });
	}

	// The script's last argument is a callback that ends the command.
	executeAsyncScript(script, args = []) {
		return this.send('POST', '/execute/async', { script, args });
	}

	async setTimeouts({ script, pageLoad, implicit } = {}) {
		const body = {};
		if (script !== undefined) body.script = script;
		if (pageLoad !== undefined) body.pageLoad = pageLoad;
		if (implicit !== undefined) body.implicit = implicit;
		await this.send('POST', '/timeouts', body);
	}

	// Throws WebDriverError "no such element" when nothing matches. With `from`,
	// the search is scoped to that element's descendants.
	async findElement(selector, from) {
		const prefix = from === undefined ? '' : this.element(from);
		return reference(await this.send('POST', `${prefix}/element`, { using: 'css selector', value: selector }));
	}

	async findElements(selector, from) {
		const prefix = from === undefined ? '' : this.element(from);
		const found = await this.send('POST', `${prefix}/elements`, { using: 'css selector', value: selector });
		return found.map(reference);
	}

	// Throws WebDriverError "no such element" when the document has no focus.
	async activeElement() {
		return reference(await this.send('GET', '/element/active'));
	}

	async click(element) {
		await this.send('POST', `${this.element(element)}/click`, {});
	}

	async sendKeys(element, text) {
		await this.send('POST', `${this.element(element)}/value`, { text: String(text) });
	}

	elementProperty(element, name) {
		return this.send('GET', `${this.element(element)}/property/${encodeURIComponent(name)}`);
	}

	// { x, y, width, height } in CSS pixels relative to the document.
	elementRect(element) {
		return this.send('GET', `${this.element(element)}/rect`);
	}

	// Some drivers, such as the iOS Simulator's, refuse this with
	// "unsupported operation"; the target's setWindowRect flag says so first.
	setWindowRect(rect) {
		return this.send('POST', '/window/rect', rect);
	}

	// Each entry is one input source: { type: 'key' | 'pointer' | 'none', id,
	// parameters?, actions }.
	async performActions(actions) {
		await this.send('POST', '/actions', { actions });
	}

	async releaseActions() {
		await this.send('DELETE', '/actions');
	}

	// The viewport as a PNG.
	async screenshot() {
		return Buffer.from(await this.send('GET', '/screenshot'), 'base64');
	}

	async deleteSession() {
		await command(this.driverUrl, 'DELETE', `/session/${encodeURIComponent(this.id)}`, undefined, { timeoutMs: this.timeoutMs });
	}
}

// `capabilities` is the alwaysMatch object, for example { browserName: 'chrome' }.
export async function newSession(driverUrl, capabilities, { timeoutMs = DEFAULT_SESSION_TIMEOUT_MS, commandTimeoutMs } = {}) {
	const value = await command(driverUrl, 'POST', '/session', { capabilities: { alwaysMatch: capabilities } }, { timeoutMs });
	if (typeof value?.sessionId !== 'string') {
		throw new WebDriverError('session not created', 'the driver answered without a sessionId', { method: 'POST', path: '/session' });
	}
	return new Session(driverUrl, value.sessionId, value.capabilities ?? {}, { timeoutMs: commandTimeoutMs });
}

export function status(driverUrl, { timeoutMs = 2_000 } = {}) {
	return command(driverUrl, 'GET', '/status', undefined, { timeoutMs });
}

// The key source's actions for pressing each argument in turn. A string is one
// key; an array is a chord, pressed in order and released in reverse, such as
// [Keys.Shift, Keys.Tab].
export function keyActions(...keys) {
	const actions = [];
	for (const key of keys) {
		const chord = Array.isArray(key) ? key : [key];
		for (const value of chord) actions.push({ type: 'keyDown', value });
		for (const value of [...chord].reverse()) actions.push({ type: 'keyUp', value });
	}
	return [{ type: 'key', id: 'keyboard', actions }];
}

// Presses keys at whatever has focus, then releases all input state.
export async function pressKeys(session, ...keys) {
	await session.performActions(keyActions(...keys));
	await session.releaseActions();
}

// pauseMs is how long the finger rests before lifting; 0 lifts at once.
export function tapActions(element, { x = 0, y = 0, pauseMs = 50 } = {}) {
	const actions = [
		{ type: 'pointerMove', duration: 0, origin: reference(element), x, y },
		{ type: 'pointerDown', button: 0 },
	];
	if (pauseMs > 0) actions.push({ type: 'pause', duration: pauseMs });
	actions.push({ type: 'pointerUp', button: 0 });
	return [{ type: 'pointer', id: 'finger', parameters: { pointerType: 'touch' }, actions }];
}

// A touch tap at the element's centre (or offset from it), as a finger
// produces, rather than the mouse-like click of the element click command.
export async function tap(session, element, options) {
	await session.performActions(tapActions(element, options));
	await session.releaseActions();
}

// Polls `check` until it returns a truthy value, which is returned. A thrown
// error ends the wait at once.
export async function waitUntil(check, { timeoutMs = 10_000, intervalMs = 100, message = 'condition' } = {}) {
	const deadline = Date.now() + timeoutMs;
	for (;;) {
		const result = await check();
		if (result) return result;
		if (Date.now() >= deadline) throw new Error(`Timed out after ${timeoutMs} ms waiting for ${message}.`);
		await new Promise((resolve) => setTimeout(resolve, intervalMs));
	}
}
