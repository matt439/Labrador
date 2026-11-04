// What the two LineSweeper host-page drivers share: bundle_input.mjs, for the
// bundle's example page, and host_input.mjs, for the site's own page. Both
// pages mount the same ES-module factory on a click, use the same element ids
// (#start, #play, #status and #accessibility) and the same player-facing
// sentences for play, quit, context loss and failure, so the hooks, the
// substituted responses and the steps a driver takes are written once here.
//
// The only substituted responses are a thin wrapper around the real factory,
// a 404 for the loader in the download-failure case, and the first 32 bytes
// of the real wasm response in the truncated case. A driver names its case by
// setting `state.phase`; `expected_complaint` recognizes the diagnostics those
// two injected failures and the injected onError log.

export const READY = "document.readyState === 'complete' && typeof window.labradorBundle === 'object'";

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

// Imported relative to the loader's own address, so it finds the real loader
// whether the page names ./linesweeper.js or a folder such as ./m/<hash>/.
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

// One run's shared state: the case being driven, every request the page
// made, and what the interception and the expected diagnostics saw.
export function bundle_state() {
	return { phase: 'touch gate', requests: [], interception_errors: [], expected_diagnostics: [] };
}

// Emscripten reports the compile failure before rejecting its factory promise.
// Compile diagnostics are expected only while deliberately serving truncated
// wasm; the injected onError also retains its exact console reason. Any warning,
// unrelated error or uncaught exception still fails the run.
export function expected_complaint(state, { source, level, text }) {
	const download = state.phase === 'download failure' && level === 'error' &&
		/linesweeper\.js|Failed to load resource|LineSweeper failure: TypeError/.test(text);
	if (download) { state.expected_diagnostics.push(text); return true; }
	const expected = source === 'console' && level === 'error' &&
		((state.phase === 'truncated wasm' &&
			/^(wasm streaming compile failed:|falling back to ArrayBuffer instantiation$|failed to asynchronously prepare wasm:|Aborted\(|LineSweeper failure:)/.test(text)) ||
		(state.phase === 'runtime onError' && /^LineSweeper failure: bundle test failure$/.test(text)));
	// A GPU process crash loses every context in the page, and Chrome warns
	// once for each one the page had already lost on purpose - the support
	// probe's scratch context - as if loseContext had been called again.
	const crash = state.phase === 'gpu process crash' && level === 'warning' && /^WebGL: CONTEXT_LOST_WEBGL: loseContext: context lost$/.test(text);
	if (crash) { state.expected_diagnostics.push(text); return true; }
	if (expected) { state.expected_diagnostics.push(text); }
	return expected;
}

export async function prepare(state, page) {
	await page.send('Emulation.setFocusEmulationEnabled', { enabled: true });
	await page.send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: PRELUDE });
	await page.send('Network.enable');
	page.on((message) => {
		if (message.method === 'Network.requestWillBeSent') {
			state.requests.push(message.params.request.url);
		}
		if (message.method !== 'Fetch.requestPaused') { return; }
		intercept(state, page, message.params).catch((error) => {
			state.interception_errors.push(error.stack ?? String(error));
		});
	});
	await page.send('Fetch.enable', { patterns: [
		{ urlPattern: '*linesweeper.js*', requestStage: 'Response' },
		{ urlPattern: '*linesweeper.wasm*', requestStage: 'Response' },
	] });
}

async function intercept(state, page, paused) {
	const { requestId, request } = paused;
	const url = new URL(request.url);
	let body;
	let type;
	if (url.pathname.endsWith('/linesweeper.js') && url.search === '' &&
		state.phase === 'download failure') {
		await page.send('Fetch.fulfillRequest', { requestId, responseCode: 404,
			responseHeaders: [{ name: 'Content-Type', value: 'text/plain' }],
			body: Buffer.from('not found').toString('base64') });
		return;
	}
	if (url.pathname.endsWith('/linesweeper.js') && url.search === '') {
		body = Buffer.from(FACTORY_WRAPPER);
		type = 'text/javascript; charset=utf-8';
	} else if (url.pathname.endsWith('/linesweeper.wasm') && state.phase === 'truncated wasm') {
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

// Whether a request since `from` fetched the loader or the module.
export function fetched_module(state, from = 0) {
	return state.requests.slice(from).some((url) => /\/linesweeper\.(js|wasm)$/.test(new URL(url).pathname));
}

// The steps both drivers take, bound to one page and one run's state.
export function steps(page, state) {
	const { evaluate, send } = page;
	const assert = (condition, message) => {
		if (!condition) { throw new Error(`${state.phase}: ${message}`); }
	};
	const until = async (expression, message, navigating = false, attempts = 500) => {
		for (let attempt = 0; attempt < attempts; attempt++) {
			assert(state.interception_errors.length === 0, state.interception_errors.join('\n'));
			try {
				if (await evaluate(expression) === true) { return; }
			} catch (error) {
				if (!navigating || !/Execution context was destroyed|Cannot find (?:default execution context|context with specified id)/.test(error.message)) {
					throw error;
				}
			}
			await page.wait(20);
		}
		throw new Error(`${state.phase}: ${message}`);
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
		// A visitor scrolls to the button first; the site's page puts its
		// stage below the fold of a short window. A button already in view,
		// as on the bundle's page, is left where it is, and the scroll is
		// instant so the click lands where the button now is.
		await evaluate(`(() => {
			const button = document.getElementById('play');
			const box = button.getBoundingClientRect();
			if (box.top < 0 || box.bottom > innerHeight || box.left < 0 || box.right > innerWidth) {
				button.scrollIntoView({ block: 'center', behavior: 'instant' });
			}
		})()`);
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
	let base_url;
	const navigate = async (name) => {
		base_url ??= new URL(await evaluate('location.href'));
		const url = new URL(base_url);
		url.searchParams.set('bundle-case', name);
		await send('Page.navigate', { url: url.href });
		await until(`location.search === ${JSON.stringify(url.search)} && (${READY})`,
			'the next host page did not settle', true);
	};
	const canvas_box = () => evaluate(`(() => {
		const box = document.querySelector('canvas').getBoundingClientRect();
		return { left: box.left, top: box.top, right: box.right, bottom: box.bottom,
			width: box.width, height: box.height,
			view_width: innerWidth, view_height: innerHeight };
	})()`);

	// Play, focus, Tab out, a forced context loss and its restoration, QUIT,
	// and a replay on a fresh canvas that hears a synthetic tab switch.
	const play_restore_replay = async () => {
		await click_play();
		await loaded(0);
		await clean_runs();
		await accessibility_note('canvas');
		assert(await evaluate("document.activeElement === document.querySelector('canvas')"),
			'the running canvas did not receive focus');
		await key('Tab', 9);
		assert(await evaluate("document.activeElement !== document.querySelector('canvas') && document.activeElement.matches('a[href], #fullscreen')"),
			'Tab did not move focus from the game to a page control');
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
	};

	// WEB#14: what the forced loss and the synthetic visibility above stand in
	// for, made real. Browser.crashGpuProcess kills Chrome's GPU process,
	// which loses every WebGL context in the page and gives them back once a
	// new one starts, as a driver reset does; opening another tab in front
	// hides this one, and activating it again shows it, as a person switching
	// tabs does. Both are Chrome's own events, not the page's.
	const real_loss_and_tab_switch = async () => {
		state.phase = 'gpu process crash';
		await navigate('gpu-crash');
		await click_play();
		await loaded(0);
		const instructions = await evaluate("document.getElementById('status').textContent");
		const before_crash = await evaluate('window.labradorBundle.runs[0].canvas.labradorFrames');
		await page.send_browser('Browser.crashGpuProcess');
		await until("window.labradorBundle.runs[0].context[1]?.name === 'onContextRestored'",
			'a GPU process crash was not heard as a loss and a restoration', false, 1500);
		const context = await evaluate('window.labradorBundle.runs[0].context');
		assert(context.length === 2 && context[0].name === 'onContextLost' &&
			context[0].text === 'The browser took the graphics away; waiting for them to come back.' &&
			context[1].text === instructions,
			`the crash's callbacks or status texts were ${JSON.stringify(context)}`);
		await until(`window.labradorBundle.runs[0].canvas.labradorFrames > ${before_crash + 5} && ` +
			`document.getElementById('status').textContent === ${JSON.stringify(instructions)} && ` +
			"!document.getElementById('status').classList.contains('error')",
			'frames or the status did not come back after the GPU process restarted');
		console.log('ok - a real GPU process crash loses and restores the context, and the game draws again');

		state.phase = 'tab switch';
		await evaluate(`(() => {
			window.labradorBundle.visibility = [];
			document.addEventListener('visibilitychange', () =>
				window.labradorBundle.visibility.push(document.visibilityState));
		})()`);
		// Focus emulation keeps a page focused and visible whatever is in
		// front of it, so it is off while another tab is.
		await send('Emulation.setFocusEmulationEnabled', { enabled: false });
		const { targetId: other } = await page.send_browser('Target.createTarget', { url: 'about:blank' });
		await until("document.visibilityState === 'hidden'", 'opening a tab in front did not hide the game');
		const hidden_frames = await evaluate('window.labradorBundle.runs[0].canvas.labradorFrames');
		await page.wait(500);
		assert(await evaluate(`window.labradorBundle.runs[0].canvas.labradorFrames <= ${hidden_frames + 1}`),
			'the hidden game went on drawing');
		await page.send_browser('Target.activateTarget', { targetId: page.target_id });
		await until("document.visibilityState === 'visible'", 'activating the game\'s tab did not show it');
		await page.send_browser('Target.closeTarget', { targetId: other });
		await send('Emulation.setFocusEmulationEnabled', { enabled: true });
		assert(await evaluate("JSON.stringify(window.labradorBundle.visibility) === '[\"hidden\",\"visible\"]'"),
			'the tab switch was not one hide and one show');
		await until(`window.labradorBundle.runs[0].canvas.labradorFrames > ${hidden_frames + 5}`,
			'the game did not draw again after its tab came back');
		// Hidden, the game lost the keyboard and paused, so QUIT is reached
		// without Escape: had it played on, the arrows would move a piece.
		await evaluate("document.querySelector('canvas').focus()");
		await quit(0, true);
		console.log('ok - a real tab switch hides and pauses the game, stops its frames, and shows it again');
	};

	// BDP-18: a finished game is released, so repeated play keeps at most one
	// live context and one module heap however many games are played.
	const repeated_play = async () => {
		state.phase = 'repeated play';
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
	};

	// The ten-second context wait, then Try again through _linesweeper_quit.
	// `unreturned` is a phrase of the page's sentence for graphics that stayed
	// away, which is the one sentence the two pages word differently.
	const context_timeout = async (unreturned) => {
		state.phase = 'context timeout';
		await navigate('context-timeout');
		await click_play();
		await loaded(0);
		await evaluate(`window.labradorBundle.runs[0].canvas.getContext('webgl2')
			.getExtension('WEBGL_lose_context').loseContext()`);
		await until(button_visible, 'an unreturned context never offered a new game', false, 750);
		assert(await evaluate("document.getElementById('play').textContent === 'Try again' && " +
			`document.getElementById('status').textContent.includes(${JSON.stringify(unreturned)}) && ` +
			'window.labradorBundle.runs[0].canvas.isConnected'),
			'context timeout did not retain the live canvas and offer Try again');
		await click_play();
		await loaded(1);
		assert(await evaluate('window.labradorBundle.runs[0].quit === true && ' +
			'!window.labradorBundle.runs[0].canvas.isConnected && document.querySelectorAll("canvas").length === 1'),
			'Try again did not quit the old game and replace its canvas');
		assert(await evaluate("!document.getElementById('status').classList.contains('error')"),
			'the fresh game kept the graphics warning');
		await quit(1);
		console.log('ok - unreturned graphics offer Try again, which quits the old game and plays a fresh one');
	};

	// Startup and runtime failures past the download: a truncated module and
	// an onError from a running game, each with a sentence and Try again.
	// `details`, where given, checks the page's line for the raw reason.
	const truncated_and_runtime_failure = async (details) => {
		state.phase = 'truncated wasm';
		await navigate('truncated');
		await click_play();
		await until("document.getElementById('status').classList.contains('error') && " + button_visible,
			'a truncated wasm did not show an error with a recovery button');
		assert(await evaluate("document.getElementById('status').textContent === 'The game could not start. Try again, or try an up-to-date browser with hardware acceleration enabled.' && " +
			"document.getElementById('play').textContent === 'Try again'"), 'startup failure message or Try again is missing');
		assert(await evaluate(button_exposed), 'a failed startup canvas overlays Try again');
		assert(state.expected_diagnostics.some((text) => text.startsWith('failed to asynchronously prepare wasm:')),
			'the deliberately truncated response did not reach compilation');
		if (details) { await details('CompileError'); }
		console.log('ok - truncated wasm shows its startup error and an exposed Try again button');

		state.phase = 'runtime onError';
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
		if (details) { await details('bundle test failure'); }
		assert(state.interception_errors.length === 0, state.interception_errors.join('\n'));
		console.log('ok - the page\'s captured onError shows its error and an exposed Try again button');
	};

	return {
		assert, until, frames, button_visible, button_exposed, accessibility_note,
		click_play, key, loaded, clean_runs, quit, navigate, canvas_box,
		play_restore_replay, real_loss_and_tab_switch, repeated_play, context_timeout,
		truncated_and_runtime_failure,
	};
}
