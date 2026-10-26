// WebDriver BiDi transport for the Firefox sample runner. Production uses
// Node's WebSocket; protocol tests supply a socket without starting a browser.
// Message envelopes: https://www.w3.org/TR/webdriver-bidi/#protocol
// Evaluation: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Reference/BiDi/Modules/script/evaluate
function record(value) {
	return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function timeout_value(value) {
	if (!Number.isFinite(value) || value <= 0 || value > 2147483647) {
		throw new Error('BiDi timeout must be a positive finite number of milliseconds.');
	}
	return value;
}

function primitive(value) {
	if (!record(value)) throw new Error('script.evaluate returned a malformed remote value.');
	if (value.type === 'undefined') return undefined;
	if (value.type === 'null') return null;
	if ((value.type === 'string' && typeof value.value === 'string') ||
		(value.type === 'boolean' && typeof value.value === 'boolean')) return value.value;
	if (value.type === 'number') {
		if (typeof value.value === 'number' && Number.isFinite(value.value)) return value.value;
		if (value.value === 'NaN') return NaN;
		if (value.value === 'Infinity') return Infinity;
		if (value.value === '-Infinity') return -Infinity;
		if (value.value === '-0') return -0;
		throw new Error('script.evaluate returned a malformed number.');
	}
	throw new Error(`script.evaluate returned unsupported remote value ${String(value.type)}; use JSON.stringify for objects.`);
}

export class FirefoxProtocol {
	constructor(url, { Socket = WebSocket, timeout_ms = 10000 } = {}) {
		this.timeout_ms = timeout_value(timeout_ms);
		this.socket = new Socket(url);
		this.next_id = 1;
		this.pending = new Map();
		this.listeners = new Set();
		this.open_waiters = new Set();
		this.failure = null;
		this.socket.addEventListener('open', () => {
			for (const waiting of this.open_waiters) {
				clearTimeout(waiting.timer);
				waiting.found();
			}
			this.open_waiters.clear();
		});
		this.socket.addEventListener('close', () => this.fail('the browser closed its BiDi connection'));
		this.socket.addEventListener('error', () => this.fail('the browser BiDi socket failed'));
		this.socket.addEventListener('message', (event) => this.receive(event.data));
	}

	opened() {
		if (this.failure !== null) return Promise.reject(this.failure);
		if (this.socket.readyState === 1) return Promise.resolve();
		if (this.socket.readyState !== 0) return Promise.reject(new Error('the BiDi socket is not connecting or open'));
		return new Promise((found, refused) => {
			const waiting = { found, refused, timer: null };
			waiting.timer = setTimeout(() => {
				this.open_waiters.delete(waiting);
				refused(new Error(`opening the BiDi socket timed out after ${this.timeout_ms} ms`));
			}, this.timeout_ms);
			this.open_waiters.add(waiting);
		});
	}

	send(method, params = {}) {
		return this.command(method, params, this.timeout_ms);
	}

	command(method, params, timeout_ms) {
		if (this.failure !== null) return Promise.reject(new Error(`${method}: ${this.failure.message}`));
		if (this.socket.readyState !== 1) return Promise.reject(new Error(`${method}: the BiDi socket is not open`));
		let text;
		let duration;
		const id = this.next_id++;
		try {
			duration = timeout_value(timeout_ms);
			text = JSON.stringify({ id, method, params });
		} catch (error) {
			return Promise.reject(new Error(`${method}: ${error.message}`));
		}
		return new Promise((found, refused) => {
			const timer = setTimeout(() => {
				this.pending.delete(id);
				refused(new Error(`${method}: timed out after ${duration} ms`));
			}, duration);
			this.pending.set(id, { found, refused, method, timer });
			try {
				this.socket.send(text);
			} catch (error) {
				clearTimeout(timer);
				this.pending.delete(id);
				refused(new Error(`${method}: ${error.message}`));
			}
		});
	}

	receive(text) {
		let message;
		try {
			if (typeof text !== 'string') throw new Error('expected a text message');
			message = JSON.parse(text);
			if (!record(message)) throw new Error('expected a message object');
			if (message.type === 'event') {
				if (typeof message.method !== 'string' || !record(message.params)) throw new Error('invalid event');
			} else if (message.type === 'success' || message.type === 'error') {
				if (message.type === 'error' && message.id === null) {
					if (typeof message.error !== 'string' || typeof message.message !== 'string') throw new Error('invalid error');
					this.fail(`BiDi ${message.error}: ${message.message}`);
					return;
				}
				if (!Number.isSafeInteger(message.id) || message.id < 0) throw new Error('invalid response id');
				// Timed-out or refused commands can still receive replies during teardown.
				if (!this.pending.has(message.id)) return;
				if (message.type === 'success' && !record(message.result)) throw new Error('invalid success result');
				if (message.type === 'error' &&
					(typeof message.error !== 'string' || typeof message.message !== 'string')) throw new Error('invalid error');
			} else {
				throw new Error('unknown message type');
			}
		} catch (error) {
			this.fail(`malformed BiDi message: ${error.message}`);
			return;
		}
		if (message.type === 'event') {
			for (const listener of [...this.listeners]) {
				try {
					listener(message);
				} catch (error) {
					this.fail(`BiDi event listener failed: ${error instanceof Error ? error.message : String(error)}`);
					return;
				}
			}
			return;
		}
		const waiting = this.pending.get(message.id);
		this.pending.delete(message.id);
		clearTimeout(waiting.timer);
		if (message.type === 'error') {
			waiting.refused(new Error(`${waiting.method}: ${message.error}: ${message.message}`));
		} else {
			waiting.found(message.result);
		}
	}

	on(listener) {
		this.listeners.add(listener);
		return () => this.listeners.delete(listener);
	}

	refuse_all(reason) {
		const message = reason instanceof Error ? reason.message : String(reason);
		for (const waiting of this.pending.values()) {
			clearTimeout(waiting.timer);
			waiting.refused(new Error(`${waiting.method}: ${message}`));
		}
		this.pending.clear();
	}

	fail(reason) {
		if (this.failure === null) this.failure = new Error(reason);
		this.refuse_all(this.failure);
		for (const waiting of this.open_waiters) {
			clearTimeout(waiting.timer);
			waiting.refused(this.failure);
		}
		this.open_waiters.clear();
	}

	close() {
		this.fail('the BiDi connection was closed by the client');
		if (this.socket.readyState === 0 || this.socket.readyState === 1) this.socket.close();
	}

	async evaluate(context, expression, timeout_ms = this.timeout_ms) {
		const result = await this.command('script.evaluate', {
			expression, target: { context }, awaitPromise: true, resultOwnership: 'none',
		}, timeout_ms);
		if (result.type === 'exception') {
			throw new Error(`script.evaluate: ${result.exceptionDetails?.text ?? 'JavaScript exception'}`);
		}
		if (result.type !== 'success') throw new Error('script.evaluate returned a malformed evaluation result.');
		return primitive(result.result);
	}
}
