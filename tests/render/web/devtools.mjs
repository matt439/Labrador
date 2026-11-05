// One WebSocket with flattened sessions: command replies and page events
// arrive in their original order. The socket type is supplied by the protocol
// fixture in devtools.test.mjs; production uses Node's WebSocket.
//
// A command refuses, naming its method, when the socket has closed or cannot
// take it, when the browser answers with an error, and when it was given a
// timeout and no answer came within it. A message that is not JSON, or a
// listener that throws, refuses every command still waiting rather than
// escaping into the socket's dispatch.
export class DevTools {
	constructor(url, Socket = WebSocket) {
		this.socket = new Socket(url);
		this.next_id = 1;
		this.pending = new Map();
		this.listeners = [];
		this.closed = null;
		this.socket.addEventListener('close', () => {
			this.closed = 'the browser went away';
			this.refuse_all(this.closed);
		});
		this.socket.addEventListener('message', (event) => this.receive(event.data));
	}

	receive(text) {
		let message;
		try {
			message = JSON.parse(text);
		} catch (error) {
			this.refuse_all(`malformed DevTools message: ${error.message}`);
			return;
		}
		if (message.id !== undefined) {
			const waiting = this.pending.get(message.id);
			// A crash or closed socket can refuse all requests before a
			// reply already in transit arrives. It no longer has a waiter;
			// dropping that reply lets teardown report the original failure.
			if (waiting === undefined) { return; }
			this.pending.delete(message.id);
			clearTimeout(waiting.timer);
			if (message.error) {
				waiting.refused(new Error(`${waiting.method}: ${message.error.message}`));
			} else {
				waiting.found(message.result);
			}
			return;
		}
		for (const listener of [...this.listeners]) {
			try {
				listener(message);
			} catch (error) {
				this.refuse_all(`DevTools event listener failed: ${error instanceof Error ? error.message : String(error)}`);
				return;
			}
		}
	}

	opened() {
		return new Promise((found, refused) => {
			this.socket.addEventListener('open', () => found(), { once: true });
			this.socket.addEventListener('error', () => refused(
				new Error('could not reach the browser\'s DevTools endpoint')), { once: true });
		});
	}

	// `timeout_ms`, when given, is how long the command may wait for its
	// answer. Without it a command waits for as long as the socket is open,
	// which is what an evaluation that awaits the page's whole run needs.
	send(method, params = {}, session_id = undefined, { timeout_ms } = {}) {
		if (this.closed !== null) { return Promise.reject(new Error(`${method}: ${this.closed}`)); }
		const id = this.next_id++;
		const message = { id, method, params };
		if (session_id) { message.sessionId = session_id; }
		return new Promise((found, refused) => {
			const waiting = { found, refused, method, timer: undefined };
			if (timeout_ms !== undefined) {
				waiting.timer = setTimeout(() => {
					this.pending.delete(id);
					refused(new Error(`${method}: no answer within ${timeout_ms} ms`));
				}, timeout_ms);
			}
			this.pending.set(id, waiting);
			try {
				this.socket.send(JSON.stringify(message));
			} catch (error) {
				clearTimeout(waiting.timer);
				this.pending.delete(id);
				refused(new Error(`${method}: ${error.message}`));
			}
		});
	}

	on(listener) {
		this.listeners.push(listener);
		return () => {
			const index = this.listeners.indexOf(listener);
			if (index >= 0) { this.listeners.splice(index, 1); }
		};
	}

	refuse_all(why) {
		for (const waiting of this.pending.values()) {
			clearTimeout(waiting.timer);
			waiting.refused(new Error(`${waiting.method}: ${why}`));
		}
		this.pending.clear();
	}
}

// Whether a failed Runtime.evaluate is the race with a navigation or reload
// that destroyed the page's execution context while it ran, which a driver
// polling across that navigation retries rather than fails on. Chrome words it
// in each of these ways depending on when the context went.
export function is_navigation_race(error) {
	return /Execution context was destroyed|Cannot find (?:default execution )?context|Inspected target navigated/
		.test(error?.message ?? '');
}

// Browser.getVersion is a browser-level command, without a page session. Its
// product field includes the product name and full browser version together.
export async function browser_version(devtools) {
	const version = await devtools.send('Browser.getVersion');
	if (typeof version.product !== 'string' || version.product.length === 0) {
		throw new Error('Browser.getVersion returned no browser product/version');
	}
	return `${version.product} (DevTools ${version.protocolVersion}, revision ${version.revision})`;
}
