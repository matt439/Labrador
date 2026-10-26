// One WebSocket with flattened sessions: command replies and page events
// arrive in their original order. The socket type is supplied by the protocol
// fixture in devtools.test.mjs; production uses Node's WebSocket.
export class DevTools {
	constructor(url, Socket = WebSocket) {
		this.socket = new Socket(url);
		this.next_id = 1;
		this.pending = new Map();
		this.listeners = [];
		this.socket.addEventListener('close', () => this.refuse_all('the browser went away'));
		this.socket.addEventListener('message', (event) => {
			const message = JSON.parse(event.data);
			if (message.id !== undefined) {
				const waiting = this.pending.get(message.id);
				// A crash or closed socket can refuse all requests before a
				// reply already in transit arrives. It no longer has a waiter;
				// dropping that reply lets teardown report the original failure.
				if (waiting === undefined) { return; }
				this.pending.delete(message.id);
				if (message.error) {
					waiting.refused(new Error(`${waiting.method}: ${message.error.message}`));
				} else {
					waiting.found(message.result);
				}
				return;
			}
			for (const listener of this.listeners) { listener(message); }
		});
	}

	opened() {
		return new Promise((found, refused) => {
			this.socket.addEventListener('open', () => found(), { once: true });
			this.socket.addEventListener('error', () => refused(
				new Error('could not reach the browser\'s DevTools endpoint')), { once: true });
		});
	}

	send(method, params = {}, session_id = undefined) {
		const id = this.next_id++;
		const message = { id, method, params };
		if (session_id) { message.sessionId = session_id; }
		return new Promise((found, refused) => {
			this.pending.set(id, { found, refused, method });
			this.socket.send(JSON.stringify(message));
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
			waiting.refused(new Error(`${waiting.method}: ${why}`));
		}
		this.pending.clear();
	}
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
