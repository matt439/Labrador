// The packaged host page, rather than the development shell: real module
// instances and frames, quit/replay, the phone gate and the host's recovery UI.
// The only substituted responses are a thin wrapper around the real factory
// and, in one named case, the first 32 bytes of the real wasm response.

export const ready = "document.readyState === 'complete' && typeof window.labradorBundle === 'object'";
export const strict_complaints = true;

let phase = 'touch gate';
const requests = [];
const interception_errors = [];
const expected_diagnostics = [];

// Emscripten reports the compile failure before rejecting its factory promise.
// Compile diagnostics are expected only while deliberately serving truncated
// wasm; the injected onError also retains its exact console reason. Any warning,
// unrelated error or uncaught exception still fails the run.
export function expected_complaint({ source, level, text }) {
	const expected = source === 'console' && level === 'error' &&
		((phase === 'truncated wasm' &&
			/^(wasm streaming compile failed:|falling back to ArrayBuffer instantiation$|failed to asynchronously prepare wasm:|Aborted\(|LineSweeper failure:)/.test(text)) ||
		(phase === 'runtime onError' && /^LineSweeper failure: bundle test failure$/.test(text)));
	if (expected) { expected_diagnostics.push(text); }
	return expected;
}

const PRELUDE = `(() => {
	window.labradorBundle = { runs: [] };
	if (location.search.includes('controller-gate')) {
		window.labradorBundle.pad = { connected: true, buttons: [{ pressed: false }] };
		navigator.getGamepads = () => [window.labradorBundle.pad];
	}
	// Weak, so that counting what is still alive does not keep it alive.
	window.labradorBundle.contexts = [];
	window.labradorBundle.memories = [];
	const getContext = HTMLCanvasElement.prototype.getContext;
	HTMLCanvasElement.prototype.getContext = function (name, ...args) {
		if (name === 'webgl2' && location.search.includes('no-webgl2')) { return null; }
		const context = getContext.call(this, name, ...args);
		if (name === 'webgl2' && context && !this.labradorCounted) {
			this.labradorCounted = true;
			window.labradorBundle.contexts.push(new WeakRef(context));
		}
		return context;
	};
	for (const name of ['instantiate', 'instantiateStreaming']) {
		const original = WebAssembly[name];
		WebAssembly[name] = async function (...args) {
			const result = await original.apply(this, args);
			for (const value of Object.values((result.instance ?? result).exports)) {
				if (value instanceof WebAssembly.Memory) {
					window.labradorBundle.memories.push(new WeakRef(value));
				}
			}
			return result;
		};
	}
	if (location.search.includes('no-exceptions')) {
		WebAssembly.validate = () => false;
	}
	const clear = WebGL2RenderingContext.prototype.clear;
	WebGL2RenderingContext.prototype.clear = function (...args) {
		this.canvas.labradorFrames = (this.canvas.labradorFrames ?? 0) + 1;
		return clear.apply(this, args);
	};
})()`;

const FACTORY_WRAPPER = `
import factory from './linesweeper.js?labrador-bundle-source';
export default function createLineSweeper(config) {
	// The config becomes the instance's Module, so it is held weakly: a strong
	// reference here would keep every finished instance alive.
	const heard = { config: new WeakRef(config), canvas: config.canvas,
		initialized: false, quit: false, errors: [], context: [] };
	window.labradorBundle.runs.push(heard);
	for (const name of ['onRuntimeInitialized', 'onQuit', 'onError',
		'onContextLost', 'onContextRestored']) {
		const original = config[name];
		config[name] = function (...args) {
			const result = typeof original === 'function' ? original.apply(this, args) : undefined;
			if (name === 'onRuntimeInitialized') { heard.initialized = true; }
			if (name === 'onQuit') { heard.quit = true; }
			if (name === 'onError') { heard.errors.push(String(args[0])); }
			if (name === 'onContextLost' || name === 'onContextRestored') {
				heard.context.push({ name, text: document.getElementById('status').textContent });
			}
			return result;
		};
	}
	return factory(config);
}
`;

export async function prepare(page) {
	await page.send('Emulation.setFocusEmulationEnabled', { enabled: true });
	await page.send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: PRELUDE });
	await page.send('Network.enable');
	page.on((message) => {
		if (message.method === 'Network.requestWillBeSent') {
			requests.push(message.params.request.url);
		}
		if (message.method !== 'Fetch.requestPaused') { return; }
		intercept(page, message.params).catch((error) => {
			interception_errors.push(error.stack ?? String(error));
		});
	});
	await page.send('Fetch.enable', { patterns: [
		{ urlPattern: '*linesweeper.js*', requestStage: 'Response' },
		{ urlPattern: '*linesweeper.wasm*', requestStage: 'Response' },
	] });
}

async function intercept(page, paused) {
	const { requestId, request } = paused;
	const url = new URL(request.url);
	let body;
	let type;
	if (url.pathname.endsWith('/linesweeper.js') && url.search === '') {
		body = Buffer.from(FACTORY_WRAPPER);
		type = 'text/javascript; charset=utf-8';
	} else if (url.pathname.endsWith('/linesweeper.wasm') && phase === 'truncated wasm') {
		const response = await page.send('Fetch.getResponseBody', { requestId });
		const original = Buffer.from(response.body, response.base64Encoded ? 'base64' : 'utf8');
		if (original.length <= 32) { throw new Error('the packaged wasm is already truncated'); }
		body = original.subarray(0, 32);
		type = 'application/wasm';
	} else {
		await page.send('Fetch.continueRequest', { requestId });
		return;
	}
	if (paused.responseStatusCode !== 200) {
		throw new Error(`packaged response ${url.pathname} returned ${paused.responseStatusCode}`);
	}
	await page.send('Fetch.fulfillRequest', {
		requestId, responseCode: 200,
		responseHeaders: [
			{ name: 'Content-Type', value: type },
			{ name: 'Cache-Control', value: 'no-store' },
		],
		body: body.toString('base64'),
	});
}

export default async function drive(page) {
	const { evaluate, send } = page;
	const assert = (condition, message) => {
		if (!condition) { throw new Error(`${phase}: ${message}`); }
	};
	const until = async (expression, message, navigating = false, attempts = 500) => {
		for (let attempt = 0; attempt < attempts; attempt++) {
			assert(interception_errors.length === 0, interception_errors.join('\n'));
			try {
				if (await evaluate(expression) === true) { return; }
			} catch (error) {
				if (!navigating || !/Execution context was destroyed|Cannot find (?:default execution context|context with specified id)/.test(error.message)) {
					throw error;
				}
			}
			await page.wait(20);
		}
		throw new Error(`${phase}: ${message}`);
	};
	const frames = (count) => evaluate(`new Promise((done) => {
		let left = ${count};
		const next = () => (--left > 0 ? requestAnimationFrame(next) : done(true));
		requestAnimationFrame(next);
	})`);
	const button_visible = `!document.getElementById('start').hidden &&
		!document.getElementById('play').hidden && !document.getElementById('play').disabled`;
	const button_exposed = `(() => {
		const button = document.getElementById('play');
		const box = button.getBoundingClientRect();
		return box.width > 0 && box.height > 0 &&
			document.elementFromPoint(box.x + box.width / 2, box.y + box.height / 2) === button;
	})()`;
	const accessibility_note = async (selector) => {
		assert(await evaluate(`(() => {
			const note = document.getElementById('accessibility');
			const target = document.querySelector(${JSON.stringify(selector)});
			if (!note || !target || note.textContent.trim().length === 0) { return false; }
			const box = note.getBoundingClientRect();
			return box.width > 0 && box.height > 0 &&
				getComputedStyle(note).visibility === 'visible' &&
				(target.getAttribute('aria-describedby') ?? '').split(/\\s+/).includes(note.id);
		})()`), `${selector} does not reference a visible accessibility note`);
	};
	const click_play = async () => {
		assert(await evaluate(button_visible), 'Play is unavailable');
		assert(await evaluate(button_exposed), 'a canvas overlays the Play button');
		const at = await evaluate(`(() => {
			const box = document.getElementById('play').getBoundingClientRect();
			return { x: box.x + box.width / 2, y: box.y + box.height / 2 };
		})()`);
		await send('Input.dispatchMouseEvent', { type: 'mousePressed', ...at,
			button: 'left', buttons: 1, clickCount: 1 });
		await send('Input.dispatchMouseEvent', { type: 'mouseReleased', ...at,
			button: 'left', buttons: 0, clickCount: 1 });
	};
	const key = async (code, vk) => {
		const event = { code, key: code === 'KeyA' ? 'a' : code,
			windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk };
		await send('Input.dispatchKeyEvent', { type: 'rawKeyDown', ...event });
		await page.wait(100);
		await send('Input.dispatchKeyEvent', { type: 'keyUp', ...event });
		await page.wait(100);
	};
	const loaded = (index) => until(`window.labradorBundle.runs[${index}]?.initialized === true &&
		(window.labradorBundle.runs[${index}].canvas.labradorFrames ?? 0) >= 5`,
		`instance ${index + 1} did not draw frames`);
	const clean_runs = async () => {
		const errors = await evaluate('window.labradorBundle.runs.flatMap((run) => run.errors)');
		assert(errors.length === 0, `unexpected onError: ${errors.join(' | ')}`);
	};
	const quit = async (index, already_paused = false) => {
		if (!already_paused) { await key('Escape', 27); }
		for (const [code, vk] of [['ArrowDown', 40], ['ArrowDown', 40],
			['ArrowDown', 40], ['Enter', 13]]) {
			await key(code, vk);
		}
		await until(`window.labradorBundle.runs[${index}].quit === true && ${button_visible}`,
			'QUIT did not reach the host callback and replay button');
		assert(await evaluate("document.getElementById('status').textContent === 'Thanks for playing.'"),
			'QUIT did not show its status');
		assert(await evaluate("document.getElementById('play').textContent === 'Play again'"),
			'QUIT did not offer Play again');
		await clean_runs();
	};
	const base_url = new URL(await evaluate('location.href'));
	const navigate = async (name) => {
		const url = new URL(base_url);
		url.searchParams.set('bundle-case', name);
		await send('Page.navigate', { url: url.href });
		await until(`location.search === ${JSON.stringify(url.search)} && (${ready})`,
			'the next host page did not settle', true);
	};

	assert(await evaluate("matchMedia('(pointer: coarse)').matches"), 'touch emulation did not produce a coarse pointer');
	assert(await evaluate("document.getElementById('play').hidden && !document.getElementById('needs-keyboard').hidden"),
		'a touch-only visitor was offered Play');
	assert(await evaluate("!(/Space|Escape|arrow/i).test(document.getElementById('status').textContent)"),
		'the touch gate gave keyboard play instructions');
	await accessibility_note('#play');
	await frames(10);
	assert(!requests.some((url) => new URL(url).pathname.endsWith('/linesweeper.wasm')),
		'the touch gate downloaded wasm');
	await key('KeyA', 65);
	assert(await evaluate(button_visible), 'a key did not lift the touch gate');
	console.log('ok - touch gate hides Play and fetches no wasm until a keyboard is seen');
	phase = 'controller gate';
	const before_controller = requests.length;
	await navigate('controller-gate');
	await frames(20);
	assert(await evaluate("document.getElementById('play').hidden"), 'a connected but unused pad lifted the gate');
	await evaluate('window.labradorBundle.pad.buttons[0].pressed = true');
	await until(button_visible, 'a controller button did not lift the touch gate');
	assert(!requests.slice(before_controller).some((url) => /\/linesweeper\.(js|wasm)$/.test(new URL(url).pathname)),
		'the controller gate downloaded the module before Play');
	console.log('ok - a controller button opens Play without fetching the module');

	await send('Emulation.setTouchEmulationEnabled', { enabled: false });
	for (const [name, advice] of [['no-webgl2', 'hardware acceleration'],
		['no-exceptions', 'WebAssembly exception support']]) {
		phase = name;
		const before = requests.length;
		await navigate(name);
		await click_play();
		await until("document.getElementById('status').classList.contains('error') && " +
			"document.getElementById('play').hidden", 'unsupported browser offered no explanation');
		assert((await evaluate("document.getElementById('status').textContent")).includes(advice),
			'the support failure did not give player advice');
		assert(!requests.slice(before).some((url) => /\/linesweeper\.(js|wasm)$/.test(new URL(url).pathname)),
			'an unsupported browser downloaded the module');
	}
	console.log('ok - unsupported graphics or exceptions show advice without downloading the module');
	phase = 'play and replay';
	await navigate('desktop');
	assert(await evaluate("document.getElementById('status').textContent.includes('Controller:') && " +
		"document.getElementById('status').textContent.includes('Down soft-drops') && " +
		"document.getElementById('status').textContent.includes('Start pauses')"),
		'the desktop page omitted controller or soft-drop instructions');
	await click_play();
	await loaded(0);
	await clean_runs();
	await accessibility_note('canvas');
	assert(await evaluate("document.activeElement === document.querySelector('canvas')"),
		'the running canvas did not receive focus');
	await key('Tab', 9);
	assert(await evaluate("document.activeElement !== document.querySelector('canvas') && document.activeElement.matches('a[href]')"),
		'Tab did not move focus from the game to a page link');
	await evaluate("document.querySelector('canvas').focus()");
	console.log('ok - the visible accessibility note describes Play and the canvas, and Tab leaves the game');
	const instructions = await evaluate("document.getElementById('status').textContent");
	await evaluate(`(() => {
		const canvas = window.labradorBundle.runs[0].canvas;
		window.labradorBundle.loss = canvas.getContext('webgl2').getExtension('WEBGL_lose_context');
		if (!window.labradorBundle.loss) { throw new Error('WEBGL_lose_context is unavailable'); }
		window.labradorBundle.loss.loseContext();
	})()`);
	await until("window.labradorBundle.runs[0].context[0]?.name === 'onContextLost'",
		'the host never heard context loss');
	assert(await evaluate("document.getElementById('status').textContent === 'The browser took the graphics away; waiting for them to come back.'"),
		'the loss status is missing');
	assert(await evaluate("document.getElementById('status').classList.contains('error')"),
		'context loss did not receive warning styling');
	await frames(5);
	const before_restore = await evaluate('window.labradorBundle.runs[0].canvas.labradorFrames');
	await evaluate('window.labradorBundle.loss.restoreContext()');
	await until("window.labradorBundle.runs[0].context[1]?.name === 'onContextRestored'",
		'the host never heard context restoration');
	await until(`window.labradorBundle.runs[0].canvas.labradorFrames > ${before_restore + 3}`,
		'frames did not resume after context restoration');
	const context = await evaluate('window.labradorBundle.runs[0].context');
	assert(context.length === 2 && context[0].name === 'onContextLost' &&
		context[1].name === 'onContextRestored' && context[1].text === instructions,
		'context callbacks or their status texts were out of order');
	// Tab already blurred the canvas. Quitting without Escape demonstrates
	// that returning focus and restoring graphics did not resume the match.
	await quit(0, true);
	await click_play();
	await loaded(1);
	await accessibility_note('canvas');
	assert(await evaluate('window.labradorBundle.runs[0].canvas !== window.labradorBundle.runs[1].canvas && ' +
		'!window.labradorBundle.runs[0].canvas.isConnected && document.querySelectorAll("canvas").length === 1'),
		'replay did not replace the first instance canvas');
	// Synthetic visibility, as in WebWindowTests: this checks the response,
	// not a physical tab switch. No update is required between hide and show.
	await evaluate(`(() => {
		for (const hidden of [true, false]) {
			Object.defineProperty(document, 'hidden', { configurable: true, get: () => hidden });
			Object.defineProperty(document, 'visibilityState', { configurable: true,
				get: () => hidden ? 'hidden' : 'visible' });
			document.dispatchEvent(new Event('visibilitychange'));
		}
		delete document.hidden;
		delete document.visibilityState;
	})()`);
	await quit(1, true);
	console.log('ok - packaged module draws, restores its context, quits and draws a fresh replay');

	// BDP-18: a finished game is released, so repeated play keeps at most one
	// live context and one module heap however many games are played.
	phase = 'repeated play';
	await navigate('replays');
	for (let round = 0; round < 5; round++) {
		await click_play();
		await loaded(round);
		await quit(round);
		await frames(2);
		// Emscripten replaces the canvas's getContext with a closure over its
		// instance, so this record's canvas would keep the instance alive. The
		// page drops it on the next Play; the record drops it here.
		await evaluate(`window.labradorBundle.runs[${round}].canvas = null`);
		await send('HeapProfiler.collectGarbage');
		const alive = await evaluate(`(() => ({
			contexts: window.labradorBundle.contexts.map((c) => c.deref())
				.filter((c) => c && !c.isContextLost()).length,
			memories: window.labradorBundle.memories.filter((m) => m.deref()).length,
			canvases: document.querySelectorAll('canvas').length,
		}))()`);
		const made = await evaluate('window.labradorBundle.memories.length');
		assert(made === round + 1, `round ${round + 1} saw ${made} module instantiations`);
		assert(alive.contexts === 1 && alive.memories === 1 && alive.canvases === 1,
			`round ${round + 1} kept ${JSON.stringify(alive)}`);
	}
	console.log('ok - five games in a row keep exactly one live context and one module heap');
	phase = 'context timeout';
	await navigate('context-timeout');
	await click_play();
	await loaded(0);
	await evaluate(`window.labradorBundle.runs[0].canvas.getContext('webgl2')
		.getExtension('WEBGL_lose_context').loseContext()`);
	await until(button_visible, 'an unreturned context never offered reload', false, 750);
	assert(await evaluate("document.getElementById('play').textContent === 'Reload page' && " +
		"document.getElementById('status').textContent.includes('graphics have not returned') && " +
		"window.labradorBundle.runs[0].canvas.isConnected"),
		'context timeout did not retain the live canvas and offer a document reload');
	await click_play();
	await until(`(${ready}) && window.labradorBundle.runs.length === 0 && ${button_visible}`,
		'the reload did not produce a fresh host page', true);
	await click_play();
	await loaded(0);
	await quit(0);
	console.log('ok - unreturned graphics offer reload, then the fresh document plays and quits');

	phase = 'truncated wasm';
	await navigate('truncated');
	await click_play();
	await until("document.getElementById('status').classList.contains('error') && " + button_visible,
		'a truncated wasm did not show an error with a recovery button');
	assert(await evaluate("document.getElementById('status').textContent === 'The game could not start. Try again, or try an up-to-date browser with hardware acceleration enabled.' && " +
		"document.getElementById('play').textContent === 'Try again'"), 'startup failure message or Try again is missing');
	assert(await evaluate(button_exposed), 'a failed startup canvas overlays Try again');
	assert(expected_diagnostics.some((text) => text.startsWith('failed to asynchronously prepare wasm:')),
		'the deliberately truncated response did not reach compilation');
	console.log('ok - truncated wasm shows its startup error and an exposed Try again button');

	phase = 'runtime onError';
	await navigate('runtime-error');
	await click_play();
	await loaded(0);
	await clean_runs();
	await evaluate("window.labradorBundle.runs[0].config.deref().onError('bundle test failure')");
	assert(await evaluate("document.getElementById('status').classList.contains('error') && " +
		"document.getElementById('status').textContent === 'The game stopped unexpectedly. Try again, or reload the page if it keeps happening.' && " +
		"document.getElementById('play').textContent === 'Try again' && " + button_visible),
		'onError did not show its failure state and recovery button');
	assert(await evaluate(button_exposed), 'a runtime failure canvas overlays Try again');
	assert(interception_errors.length === 0, interception_errors.join('\n'));
	console.log('ok - the page\'s captured onError shows its error and an exposed Try again button');
}
