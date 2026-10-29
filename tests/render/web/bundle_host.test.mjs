// Host policy against synthetic DOM, graphics and factory boundaries. This
// runs the source page's script; it creates no browser, GPU or generated module.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

const html = readFileSync(new URL('../../../samples/linesweeper/web/index.html', import.meta.url), 'utf8');
const script = html.match(/<script type="module">([\s\S]*?)<\/script>/)[1];
const import_expression = 'import("./linesweeper.js")';
assert.equal(script.split(import_expression).length, 2, 'one module import is the factory I/O boundary');

function host(options = {}) {
	const elements = new Map();
	const frames = [];
	const diagnostics = [];
	const runs = [];
	const events = new Map();
	const intervals = new Map();
	const timeouts = new Map();
	let imports = 0;
	let probes = 0;
	let releases = 0;
	let retired = 0;
	let active = null;
	let reloads = 0;
	class Element {
		constructor() {
			this.hidden = false;
			this.disabled = false;
			this.textContent = '';
			this.classes = new Set();
			this.children = [];
			this.events = new Map();
			this.classList = {
				toggle: (name, enabled) => enabled ? this.classes.add(name) : this.classes.delete(name),
				contains: (name) => this.classes.has(name),
			};
		}
		addEventListener(name, callback) { this.events.set(name, callback); }
		setAttribute() {}
		append(child) { this.children.push(child); child.parent = this; }
		remove() { this.parent.children = this.parent.children.filter((child) => child !== this); }
		focus() { active = this; }
		getContext(name) {
			assert.equal(name, 'webgl2');
			// A canvas on the stage is a game's, asked for again only to lose
			// the context of one that ran.
			if (this.parent) {
				return { getExtension: (name) => {
					assert.equal(name, 'WEBGL_lose_context');
					return { loseContext: () => retired++ };
				} };
			}
			probes++;
			if (options.graphicsThrows) { throw new Error('synthetic graphics refusal'); }
			if (options.graphics === false) { return null; }
			return { getExtension: (name) => {
				assert.equal(name, 'WEBGL_lose_context');
				return { loseContext: () => releases++ };
			} };
		}
	}
	for (const id of ['stage', 'start', 'play', 'needs-keyboard', 'status']) {
		elements.set(id, new Element());
	}
	elements.get('needs-keyboard').hidden = true;
	elements.get('status').textContent = html.match(/id="status"[^>]*>([^<]*)<\/p>/)[1];
	const context = vm.createContext({
		document: {
			getElementById: (id) => elements.get(id),
			createElement: (name) => { assert.equal(name, 'canvas'); return new Element(); },
		},
		console: { error: (...args) => diagnostics.push(args) },
		matchMedia: () => ({ matches: options.coarse === true }),
		navigator: { getGamepads: () => {
			if (options.padsForbidden) { throw new Error('synthetic pad policy'); }
			return options.pads ?? [];
		} },
		addEventListener: (name, callback) => events.set(name, callback),
		removeEventListener: (name, callback) => {
			if (events.get(name) === callback) { events.delete(name); }
		},
		setInterval: (callback) => { const id = intervals.size + 1; intervals.set(id, callback); return id; },
		clearInterval: (id) => intervals.delete(id),
		setTimeout: (callback, delay) => {
			assert.equal(delay, 10000);
			const id = timeouts.size + 1;
			timeouts.set(id, callback);
			return id;
		},
		clearTimeout: (id) => timeouts.delete(id),
		location: { reload: () => reloads++ },
		Uint8Array,
		WebAssembly: options.wasmMissing ? undefined : {
			validate: (bytes) => options.exceptions === false ? false : WebAssembly.validate(bytes),
		},
		requestAnimationFrame: (callback) => frames.push(callback),
		loadModule: async () => {
			imports++;
			if (options.importFailure) { throw new Error('synthetic download failure'); }
			return { default: async (config) => {
				runs.push(config);
				await options.factory?.(config);
			} };
		},
	});
	// Substitute only the dynamic import, keeping the host's lifecycle and
	// support-probe code verbatim. The browser case owns actual module loading.
	vm.runInContext(script.replace(import_expression, 'loadModule()'), context, { filename: 'bundle index.html' });
	return {
		elements, runs, diagnostics,
		click: () => elements.get('play').events.get('click')(),
		frame: () => frames.splice(0).forEach((callback) => callback()),
		event: (name) => events.get(name)?.(),
		poll: () => intervals.forEach((callback) => callback()),
		get polling() { return intervals.size; },
		expire: () => { const pending = [...timeouts.values()]; timeouts.clear(); pending.forEach((callback) => callback()); },
		get waiting() { return timeouts.size; },
		get reloads() { return reloads; },
		get status() { return elements.get('status').textContent; },
		get active() { return active; },
		get imports() { return imports; },
		get probes() { return probes; },
		get releases() { return releases; },
		get retired() { return retired; },
	};
}

const startup = 'The game could not start. Try again, or try an up-to-date browser with hardware acceleration enabled.';
const stopped = 'The game stopped unexpectedly. Try again, or reload the page if it keeps happening.';

function retry_visible(page, message) {
	assert.equal(page.status, message);
	assert.equal(page.elements.get('status').classList.contains('error'), true);
	assert.equal(page.elements.get('start').hidden, false);
	assert.equal(page.elements.get('play').disabled, false);
	assert.equal(page.elements.get('play').textContent, 'Try again');
	assert.equal(page.active, page.elements.get('play'));
}

test('page load does not import a module or create a graphics probe; each click releases its scratch context', async () => {
	const page = host();
	assert.equal(page.imports, 0);
	assert.equal(page.probes, 0);
	await page.click();
	assert.equal(page.imports, 1);
	assert.equal(page.probes, 1);
	assert.equal(page.releases, 1);
	assert.equal(page.runs.length, 1, 'the legacy exception probe is accepted by the real Node validator');
	page.runs[0].onQuit();
	await page.click();
	assert.equal(page.imports, 1, 'replay reuses the imported factory');
	assert.equal(page.probes, 2);
	assert.equal(page.releases, 2);
	assert.equal(page.retired, 1, 'replay loses the context of the quit game');
	assert.equal(page.runs.length, 2);
});

for (const options of [{ exceptions: false }, { wasmMissing: true }, { graphics: false }, { graphicsThrows: true }]) {
	test(`unsupported browser ${JSON.stringify(options)} downloads nothing and offers no retry`, async () => {
		const page = host(options);
		await page.click();
		assert.equal(page.imports, 0);
		assert.equal(page.runs.length, 0);
		assert.equal(page.elements.get('status').classList.contains('error'), true);
		assert.equal(page.elements.get('play').hidden, true);
		assert.equal(page.elements.get('start').hidden, true);
		assert.match(page.status, options.graphics === false || options.graphicsThrows ? /hardware acceleration/ : /WebAssembly exception support/);
	});
}

test('a failed download gives connection advice, preserves diagnostics and can retry', async () => {
	const options = { importFailure: true };
	const page = host(options);
	await page.click();
	retry_visible(page, 'The game could not download. Check your connection and try again.');
	assert.match(String(page.diagnostics[0][1]), /synthetic download failure/);
	options.importFailure = false;
	await page.click();
	assert.equal(page.runs.length, 1);
	assert.equal(page.elements.get('status').classList.contains('error'), false);
});

test('factory rejection gives startup advice without exposing the raw reason in the status', async () => {
	const page = host({ factory: () => { throw new Error('Aborted(synthetic compile failure)'); } });
	await page.click();
	retry_visible(page, startup);
	assert.match(String(page.diagnostics[0][1]), /Aborted\(synthetic compile failure\)/);
});

test('onError during initialization survives a later promise rejection with its original diagnostic', async () => {
	const page = host({ factory: (config) => {
		config.onError('synthetic initialization failure');
		throw new Error('later promise rejection');
	} });
	await page.click();
	page.frame();
	retry_visible(page, startup);
	assert.equal(page.diagnostics.length, 1);
	assert.equal(page.diagnostics[0][1], 'synthetic initialization failure');
});

test('an error before the first scheduled frame still says the game could not start', async () => {
	const page = host();
	await page.click();
	page.runs[0].onError('synthetic first frame failure');
	page.frame();
	retry_visible(page, startup);
	await page.click();
	assert.equal(page.retired, 1, 'retry loses the context of a game that failed at startup');
});

test('an error after the loop gets its first frame offers runtime recovery and logs the reason', async () => {
	const page = host();
	await page.click();
	page.frame();
	page.runs[0].onError('synthetic runtime failure');
	retry_visible(page, stopped);
	assert.equal(page.diagnostics[0][1], 'synthetic runtime failure');
	await page.click();
	assert.equal(page.retired, 1, 'retry loses the context of a game that ran');
});

test('callbacks and a pending frame from an ended instance cannot overwrite replay status', async () => {
	const page = host();
	await page.click();
	const first = page.runs[0];
	first.onQuit();
	await page.click();
	const instructions = page.status;
	first.onError('obsolete error');
	first.onContextLost();
	first.onContextRestored();
	first.onQuit();
	page.frame();
	assert.equal(page.status, instructions);
	assert.equal(page.elements.get('start').hidden, true);
	assert.equal(page.elements.get('stage').children.length, 1);
	assert.equal(page.diagnostics.length, 0);
});

test('desktop instructions cover fixed keyboard and controller actions without changing bindings', () => {
	const page = host();
	for (const instruction of ['Down soft-drops', 'Z anticlockwise', 'C/Shift holds',
		'Escape/P pauses', 'Controller:', 'B anticlockwise', 'X/Left shoulder holds', 'Start pauses']) {
		assert.ok(page.status.includes(instruction), instruction);
	}
});

test('the coarse-pointer gate shows device instructions, and a keyboard restores the full list', () => {
	const page = host({ coarse: true });
	assert.doesNotMatch(page.status, /Space|Escape|arrow/i);
	assert.match(page.status, /Touch play.*keyboard key or controller button/);
	assert.equal(page.elements.get('play').hidden, true);
	assert.equal(page.polling, 1);
	page.event('keydown');
	assert.equal(page.elements.get('play').hidden, false);
	assert.equal(page.elements.get('needs-keyboard').hidden, true);
	assert.equal(page.polling, 0);
	assert.match(page.status, /Keyboard:.*Left\/Right/s);
	assert.equal(page.probes, 0);
	assert.equal(page.imports, 0);
});

test('a controller button lifts the gate without a download, then disconnect cannot re-close it', () => {
	const pad = { connected: true, buttons: [{ pressed: false }] };
	const page = host({ coarse: true, pads: [null, pad] });
	page.event('gamepadconnected');
	page.poll();
	assert.equal(page.elements.get('play').hidden, true, 'connection without a button keeps the gate');
	pad.buttons[0].pressed = true;
	page.poll();
	assert.equal(page.elements.get('play').hidden, false);
	assert.equal(page.polling, 0);
	pad.connected = false;
	page.event('gamepadconnected');
	assert.equal(page.elements.get('play').hidden, false);
	assert.equal(page.imports, 0);
	assert.equal(page.probes, 0);
});

test('a page that forbids controller access still opens the gate with a keyboard', () => {
	const page = host({ coarse: true, padsForbidden: true });
	page.poll();
	assert.equal(page.elements.get('play').hidden, true);
	page.event('keydown');
	assert.equal(page.elements.get('play').hidden, false);
	assert.equal(page.diagnostics.length, 0);
});

test('context loss is visibly warned and an early restore cancels its bounded wait', async () => {
	const page = host();
	await page.click();
	const instructions = page.status;
	page.runs[0].onContextLost();
	page.runs[0].onContextLost();
	assert.equal(page.elements.get('status').classList.contains('error'), true);
	assert.equal(page.elements.get('start').hidden, true);
	assert.equal(page.waiting, 1);
	page.runs[0].onContextRestored();
	assert.equal(page.waiting, 0);
	assert.equal(page.elements.get('status').classList.contains('error'), false);
	page.expire();
	assert.equal(page.status, instructions);
	assert.equal(page.elements.get('start').hidden, true);
});

test('factory resolution cannot erase a context warning reported during initialization', async () => {
	const page = host({ factory: (config) => config.onContextLost() });
	await page.click();
	assert.match(page.status, /waiting for them to come back/);
	assert.equal(page.elements.get('status').classList.contains('error'), true);
	assert.equal(page.waiting, 1);
});

test('factory rejection after a context timeout restores ordinary startup retry', async () => {
	let refuse;
	const options = { factory: (config) => {
		config.onContextLost();
		return new Promise((resolve, reject) => { refuse = reject; });
	} };
	const page = host(options);
	const mounting = page.click();
	await new Promise((resolve) => setImmediate(resolve));
	page.expire();
	assert.equal(page.elements.get('play').textContent, 'Reload page');
	refuse(new Error('synthetic late initialization failure'));
	await mounting;
	retry_visible(page, startup);
	options.factory = () => {};
	await page.click();
	assert.equal(page.reloads, 0);
	assert.equal(page.runs.length, 2);
});

test('late factory resolution keeps focus on an already offered reload button', async () => {
	let finish;
	const page = host({ factory: (config) => {
		config.onContextLost();
		return new Promise((resolve) => { finish = resolve; });
	} });
	const mounting = page.click();
	await new Promise((resolve) => setImmediate(resolve));
	page.expire();
	assert.equal(page.active, page.elements.get('play'));
	finish();
	await mounting;
	assert.equal(page.active, page.elements.get('play'));
	assert.equal(page.elements.get('start').hidden, false);
});

test('an unreturned context offers a document reload instead of abandoning a live instance', async () => {
	const page = host();
	await page.click();
	page.runs[0].onContextLost();
	page.expire();
	assert.match(page.status, /graphics have not returned.*Reload/);
	assert.equal(page.elements.get('play').textContent, 'Reload page');
	assert.equal(page.elements.get('play').disabled, false);
	assert.equal(page.elements.get('start').hidden, false);
	await page.click();
	assert.equal(page.reloads, 1);
	assert.equal(page.runs.length, 1);
	assert.equal(page.elements.get('stage').children.length, 1);
});

test('a late restore hides the reload offer and keeps the original instance', async () => {
	const page = host();
	await page.click();
	const instructions = page.status;
	page.runs[0].onContextLost();
	page.expire();
	page.runs[0].onContextRestored();
	assert.equal(page.status, instructions);
	assert.equal(page.elements.get('status').classList.contains('error'), false);
	assert.equal(page.elements.get('start').hidden, true);
	assert.equal(page.runs.length, 1);
	assert.equal(page.active, page.runs[0].canvas);
});

for (const ending of ['onQuit', 'onError']) {
	test(`${ending} cancels context recovery so an old timeout cannot overwrite the end state`, async () => {
		const page = host();
		await page.click();
		page.runs[0].onContextLost();
		page.runs[0][ending]('synthetic end');
		const status = page.status;
		assert.equal(page.waiting, 0);
		page.expire();
		assert.equal(page.status, status);
		assert.notEqual(page.elements.get('play').textContent, 'Reload page');
	});
}
