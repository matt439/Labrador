// What the web window must tell its owner for each thing a browser delivers
// to it: the input half of WebWindowTests, run by
// tests/render/web/run_page.mjs --input against window_page.cpp's page in
// Chromium, and by tests/render/web/run_firefox.mjs --test WebWindowTests in
// Firefox, and by tests/render/web/run_safari.mjs --window-page in Safari.
//
// drive_window holds every step and what it must produce. The default
// export hands it devtools_input, the DevTools
// protocol's Input and Emulation domains; the Firefox runner hands it its
// WebDriver BiDi input and the Safari runner hands it classic WebDriver.
// Focus moves because a click moved it, a default is prevented or kept
// because the canvas's listener said so, and a
// touch makes - or does not make - the mouse events a browser makes of a
// touch. Where a protocol cannot produce an input, its input object says so
// and the step that needs it states what was done instead.
//
// Visibility is synthetic: a headless browser has
// no tab strip and no desktop to minimise to, and neither creating and
// activating another tab, nor the lifecycle states, nor minimising its window
// changes document.visibilityState. So the page's document is told it is
// hidden and the event is dispatched at it, which tests what the window does
// with the event and not that a browser sends it.
// The ISO extra and unknown-code checks also use synthetic DOM events: printable
// key actions cannot choose that physical position independently of layout.
// That step checks the real window's DOM feed, not a physical keyboard.
//
// Each step does one thing and then reads and empties the page's log, which
// window_page.cpp writes one entry per WindowNotify call, and compares it
// whole. A frame is "tick" there and is dropped here; a frame that ran with a
// drawing buffer nobody was told about fails whichever step it fell in.
//
// Positions are CSS pixels of the page. The canvas's box starts at (20, 20)
// (shell.html), so (120, 70) is client pixel (100, 50) at a device pixel
// ratio of one.
//
// THE INPUT OBJECT. Every member returns a promise.
//
// - prepare(): makes the page a focused window, without which an element in
//   it is never sent focus or blur, at a device pixel ratio of one.
// - move(at, held), press(at), release(at): the left mouse button and the
//   pointer, at a CSS position; held says the button is down during the move.
// - wheel(at, delta_x, delta_y): a wheel turned by CSS pixels over a position.
// - key(name, { control }): a key from KEYS pressed and released, with
//   Control held if asked. presses_modifiers is true where Control can only
//   be held by pressing it as a key, so its own key_down and key_up reach the
//   page around the chord. option_tab is true when the browser uses Option-Tab
//   to focus the next element.
// - device_pixel_ratio(ratio): sets the page's device pixel ratio, and null
//   returns it to the ratio of one that prepare() started from.
// - touch(type, points): touchStart, touchMove, touchEnd or touchCancel in
//   the DevTools protocol's shape - points as [x, y, id]; touchStart and
//   touchMove name every finger on the glass, touchEnd the fingers that lift,
//   or none for all of them, and touchCancel takes every finger away.
//   touch_begin() and touch_end() bracket the touch steps.
//   cancels_touch is false where the protocol has no way to make the browser
//   cancel a touch. touch_slack is how many client pixels a touch's reported
//   position may be from the one sent, which is none where the browser
//   delivers a touch where it was sent. synthetic_touch is true for a DOM
//   fixture that cannot check browser-generated compatibility mouse events.
// - snaps_device_box: true where the browser's device-pixel-content-box at
//   the starting ratio of one is the box it snaps the canvas to, so the
//   fractional-position step must find the snapped size.

import { check_motion_preference } from './motion_preference_input.mjs';
import { check_fullscreen_requests } from './fullscreen_input.mjs';

import { check_window_metadata } from './window_metadata_input.mjs';

const CANVAS = { x: 120, y: 70 };
const OUTSIDE = { x: 650, y: 400 };

const KEYS = {
	ArrowLeft: { key: 'ArrowLeft', code: 37 },
	Space: { key: ' ', code: 32, text: ' ' },
	KeyA: { key: 'a', code: 65, text: 'a' },
	Tab: { key: 'Tab', code: 9 },
};
const CONTROL = 2;

// The failure step's exception, which the window writes to the console.
export function expected_complaint({ level, text }) {
	return level === 'error' && text === 'web window test failure';
}

// CSSOM read-back exercises the real wasm Window. It does not inspect the
// pointer's pixels or claim that a person has used a cursor on a device.
export async function check_cursor_selection({ evaluate }) {
	const lifecycle = JSON.parse(await evaluate(`JSON.stringify({
		fixtures: Module.labradorCursorFixtures,
		errors: Module.labradorCursorFixtureErrors,
		quits: Module.labradorCursorFixtureQuits,
	})`));
	const row = (label, value, priority, shape) => ({ label, shape, status: 1, value, priority });
	const expected = {
		fixtures: [
			row('destructor active', 'pointer', 'important', 1),
			row('destructor restored', 'grab', 'important', -1),
			row('destructor keeps page edit', 'zoom-in', '', -1),
			row('close restored', 'grab', 'important', 1),
			row('close keeps page priority edit', 'pointer', '', 1),
			row('failure restored', 'grab', 'important', 1),
			row('failure keeps page edit', 'wait', '', 1),
			row('constructor failure', 'grab', 'important', -1),
			row('invalid constructor', 'grab', 'important', -1),
		],
		errors: ['web window test failure', 'web window test failure'],
		quits: 2,
	};
	if (JSON.stringify(lifecycle) !== JSON.stringify(expected)) {
		throw new Error(`cursor lifecycle: expected ${JSON.stringify(expected)}, received ${JSON.stringify(lifecycle)}`);
	}
	const checked = await evaluate(`(() => {
		const canvas = Module.canvas;
		const style = canvas.style;
		const elsewhere = document.getElementById('elsewhere');
		const baseline = {
			value: style.getPropertyValue('cursor'), priority: style.getPropertyPriority('cursor'),
			sentinel: style.getPropertyValue('--cursor-test'),
			sentinelPriority: style.getPropertyPriority('--cursor-test'),
			width: style.width, height: style.height, outline: style.outline,
			other: elsewhere.style.cssText, focused: document.activeElement,
			locked: document.pointerLockElement,
		};
		let checks = 0;
		const state = () => [style.getPropertyValue('cursor'), style.getPropertyPriority('cursor'),
			Module._web_window_test_cursor(), getComputedStyle(canvas).cursor];
		const check = (label, value, priority, shape, computed = value) => {
			const actual = state();
			const expected = [value, priority, shape, computed];
			if (JSON.stringify(actual) !== JSON.stringify(expected)) {
				throw new Error(label + ': expected ' + JSON.stringify(expected) + ', received ' + JSON.stringify(actual));
			}
			if (elsewhere.style.cssText !== baseline.other || document.activeElement !== baseline.focused ||
				document.pointerLockElement !== baseline.locked || style.width !== baseline.width ||
				style.height !== baseline.height || style.outline !== baseline.outline) {
				throw new Error(label + ': cursor selection changed another style, element, focus or pointer lock');
			}
			++checks;
		};
		const select = (shape) => {
			const result = Module._web_window_test_set_cursor(shape);
			if (result !== 0) throw new Error('valid cursor selection was refused: ' + shape);
		};
		try {
			check('the default keeps authored inline CSS and its priority', 'grab', 'important', -1);
			for (const [shape, cursor] of ['default', 'pointer', 'text', 'crosshair', 'move', 'not-allowed'].entries()) {
				select(shape);
				check('explicit shape ' + shape, cursor, 'important', shape);
			}
			select(1);
			Module._web_window_test_set_cursor_visible(0);
			check('hidden retains the requested shape', 'none', 'important', 1);
			select(2);
			check('changing the shape while hidden keeps it hidden', 'none', 'important', 2);
			Module._web_window_test_set_cursor_confined(1);
			Module._web_window_test_set_cursor_confined(0);
			check('browser confinement does not lock or change the cursor', 'none', 'important', 2);
			Module._web_window_test_set_cursor_visible(1);
			check('showing uses the shape selected while hidden', 'text', 'important', 2);
			const beforeInvalid = state();
			if (Module._web_window_test_set_cursor(99) !== 1 || JSON.stringify(state()) !== JSON.stringify(beforeInvalid)) {
				throw new Error('invalid selection did not throw before mutation');
			}
			select(-1);
			check('reset restores the authored value and priority', 'grab', 'important', -1);
			style.removeProperty('cursor');
			select(3);
			check('an absent inline property can be overridden', 'crosshair', 'important', 3);
			select(-1);
			check('reset removes the inline override and exposes page CSS', '', '', -1, 'wait');
			style.setProperty('cursor', 'grab', 'important');
			select(1);
			style.setProperty('cursor', 'zoom-in');
			check('read-back remains the requested choice after a page edit', 'zoom-in', '', 1);
			select(-1);
			check('reset preserves a later page edit', 'zoom-in', '', -1);
			select(1);
			style.setProperty('cursor', 'pointer');
			select(-1);
			check('reset preserves a priority-only page edit', 'pointer', '', -1);
			select(1);
			style.setProperty('cursor', 'progress', 'important');
			select(4);
			check('another request takes a new temporary lease', 'move', 'important', 4);
			select(-1);
			check('its reset restores the latest page value and priority', 'progress', 'important', -1);
			select(2);
			style.setProperty('--cursor-test', 'edited while owned', 'important');
			Module._web_window_test_set_cursor_visible(0);
			Module._web_window_test_set_cursor_visible(1);
			select(-1);
			check('reset affects only the cursor property', 'progress', 'important', -1);
			if (style.getPropertyValue('--cursor-test') !== 'edited while owned' ||
				style.getPropertyPriority('--cursor-test') !== 'important') {
				throw new Error('cursor restoration overwrote an unrelated page style');
			}
			style.setProperty('cursor', 'grab', 'important');
			Module._web_window_test_set_cursor_visible(0);
			select(1);
			select(-1);
			check('reset while hidden keeps it hidden with no selected shape', 'none', 'important', -1);
			Module._web_window_test_set_cursor_visible(1);
			check('showing an unselected cursor restores page CSS', 'grab', 'important', -1);
			Module._web_window_test_set_cursor_visible(0);
			style.setProperty('cursor', 'wait');
			Module._web_window_test_set_cursor_visible(1);
			check('showing preserves a page edit made while hidden', 'wait', '', -1);
			style.setProperty('cursor', 'help', 'important');
			Module._web_window_test_set_cursor_visible(1);
			check('released ownership leaves future page CSS alone', 'help', 'important', -1);
			return checks;
		} finally {
			Module._web_window_test_set_cursor_visible(1);
			Module._web_window_test_set_cursor(-1);
			if (baseline.value) style.setProperty('cursor', baseline.value, baseline.priority);
			else style.removeProperty('cursor');
			if (baseline.sentinel) style.setProperty('--cursor-test', baseline.sentinel, baseline.sentinelPriority);
			else style.removeProperty('--cursor-test');
		}
	})()`);
	console.log(`ok - cursor lifecycle fixtures and ${checked} live CSS ownership checks`);
}

export default async function drive(page) {
	await drive_window(page, devtools_input(page.send));
}

// The DevTools protocol's input: Input.dispatch* for the mouse, the wheel,
// keys and touches, and Emulation for focus, the device pixel ratio and a
// touch screen. Headless Chromium's own device pixel ratio is one, so
// clearing the override returns to it.
export function devtools_input(send) {
	const mouse = (type, at, buttons, extra = {}) => send('Input.dispatchMouseEvent', {
		type, x: at.x, y: at.y, button: 'left', buttons, clickCount: 1, ...extra,
	});
	const key = async (name, { control = false } = {}) => {
		const k = KEYS[name];
		const modifiers = control ? CONTROL : 0;
		const typed = k.text !== undefined && modifiers === 0;
		const common = {
			code: name, key: k.key, modifiers,
			windowsVirtualKeyCode: k.code, nativeVirtualKeyCode: k.code,
		};
		await send('Input.dispatchKeyEvent', {
			...common, type: typed ? 'keyDown' : 'rawKeyDown',
			...(typed ? { text: k.text, unmodifiedText: k.text } : {}),
		});
		await send('Input.dispatchKeyEvent', { ...common, type: 'keyUp' });
	};
	return {
		presses_modifiers: false,
		snaps_device_box: true,
		cancels_touch: true,
		touch_slack: 0,
		prepare: () => send('Emulation.setFocusEmulationEnabled', { enabled: true }),
		move: (at, held = false) => mouse('mouseMoved', at, held ? 1 : 0,
			held ? {} : { button: 'none' }),
		press: (at) => mouse('mousePressed', at, 1),
		release: (at) => mouse('mouseReleased', at, 0),
		wheel: (at, delta_x, delta_y) => send('Input.dispatchMouseEvent', {
			type: 'mouseWheel', x: at.x, y: at.y, deltaX: delta_x, deltaY: delta_y,
		}),
		key,
		device_pixel_ratio: (ratio) => (ratio === null ?
			send('Emulation.clearDeviceMetricsOverride') :
			send('Emulation.setDeviceMetricsOverride',
				{ width: 800, height: 600, deviceScaleFactor: ratio, mobile: false })),
		touch_begin: () => send('Emulation.setTouchEmulationEnabled',
			{ enabled: true, maxTouchPoints: 5 }),
		touch: (type, points) => send('Input.dispatchTouchEvent', {
			type, touchPoints: points.map(([x, y, id]) => ({ x, y, id })),
		}),
		touch_end: () => send('Emulation.setTouchEmulationEnabled', { enabled: false }),
	};
}

export async function drive_window(page, input) {
	const { evaluate } = page;
	let step = 0;

	const until = async (expression) => {
		while (await evaluate(expression) !== true) {
			await page.wait(20);
		}
	};
	const frames = (count) => evaluate(`new Promise((done) => {
		let left = ${count};
		const next = () => (--left > 0 ? requestAnimationFrame(next) : done(true));
		requestAnimationFrame(next);
	})`);

	const raw = () => evaluate('JSON.stringify(Module.labradorWindowLog.splice(0))')
		.then((text) => JSON.parse(text));
	const take = async () => {
		const log = await raw();
		const unreported = log.filter((entry) => entry.startsWith('tick-unreported'));
		if (unreported.length > 0) {
			throw new Error(`step ${step + 1}: a frame ran with a buffer the owner was not told of: ${unreported[0]}`);
		}
		return log.filter((entry) => entry !== 'tick');
	};
	const defaults = () => evaluate('JSON.stringify(Module.labradorDefaults.splice(0))')
		.then((text) => JSON.parse(text));

	const expect = (what, actual, expected) => {
		if (JSON.stringify(actual) !== JSON.stringify(expected)) {
			throw new Error(`${what}\n  expected ${JSON.stringify(expected)}\n  received ${JSON.stringify(actual)}`);
		}
	};
	const check = async (what, expected, expected_defaults = undefined) => {
		step++;
		expect(`step ${step}, ${what}`, await take(), expected);
		if (expected_defaults !== undefined) {
			expect(`step ${step}, ${what}: the page's defaults`, await defaults(),
				expected_defaults);
		}
		console.log(`ok ${step} - ${what}`);
	};
	// A step whose checks are `run`'s own: it is handed `what`, to name in a
	// failure as `step ${step}, ${what}`, and the ok line names it the same.
	const pass = async (what, run) => {
		step++;
		const result = await run(what);
		console.log(`ok ${step} - ${what}`);
		return result;
	};

	const { move, press, release, wheel, key, touch } = input;

	const set_hidden = (hidden) => evaluate(`(() => {
		Object.defineProperty(document, 'hidden', { configurable: true, get: () => ${hidden} });
		Object.defineProperty(document, 'visibilityState', { configurable: true,
			get: () => '${hidden ? 'hidden' : 'visible'}' });
		document.dispatchEvent(new Event('visibilitychange'));
		return true;
	})()`);

	// A headless page is not a focused window unless it is told to be, and an
	// element in a page without focus is never sent focus or blur.
	await input.prepare();
	await until(`typeof Module === 'object' && Module.labradorWindowReady === true`);
	await pass('native title and icon requests leave the host metadata alone',
		() => check_window_metadata(page));

	await pass('cursor choices preserve page CSS, focus and lifecycle ownership',
		() => check_cursor_selection(page));
	await pass('motion preference refreshes and unavailable queries fall back',
		() => check_motion_preference(page));
	await frames(2);

	// THE CONSTRUCTOR'S REPORTS. The canvas's box, measured, rather than the
	// 320x200 asked for; and a canvas the page has not focused does not have
	// the keyboard, which a Win32 owner would otherwise assume.
	await check('the window reports the measured box and that it lacks the keyboard',
		['size 400 250', 'keyboard_lost']);

	// THE KEYBOARD FOLLOWS FOCUS.
	await key('ArrowLeft');
	await check('a key while the canvas lacks the keyboard reaches nothing',
		[], ['keydown ArrowLeft kept']);

	await move(CANVAS);
	await check('the pointer over the canvas is reported in client pixels',
		['mouse_move 100 50']);

	await wheel(CANVAS, 0, 100);
	await check('the wheel belongs to the page until the canvas is clicked',
		[], ['wheel kept']);

	await press(CANVAS);
	await check('a press on the canvas is reported, and gives it the keyboard',
		['mouse_move 100 50', 'button_down left', 'keyboard_gained']);

	// THE CAPTURE ANALOGUE. A press that started on the canvas is followed
	// off it, and its release is heard wherever it happens.
	await move(OUTSIDE, true);
	await check('a held button is followed off the canvas', ['mouse_move 630 380']);
	await release(OUTSIDE);
	await check('and released off it', ['button_up left']);
	await move({ x: OUTSIDE.x + 10, y: OUTSIDE.y + 10 });
	await check('a pointer off the canvas with nothing held is nobody\'s', []);

	// THE KEYS, AND WHICH DEFAULTS THE CANVAS TAKES.
	await key('ArrowLeft');
	await check('an arrow is the game\'s, and does not scroll the page',
		['key_down left', 'key_up left'], ['keydown ArrowLeft prevented']);
	await key('Space');
	await check('Space is a key and text, and does not scroll the page',
		['key_down space', 'text 32', 'key_up space'], ['keydown Space prevented']);
	await key('KeyA');
	await check('a letter is a key and text',
		['key_down a', 'text 97', 'key_up a'], ['keydown KeyA prevented']);
	await evaluate(`(() => {
		const canvas = document.getElementById('canvas');
		for (const type of ['keydown', 'keyup']) {
			canvas.dispatchEvent(new KeyboardEvent(type, {
				code: 'IntlBackslash', key: '<', bubbles: true, cancelable: true,
			}));
		}
		return true;
	})()`);
	await check('synthetic ISO-position events reach the real DOM window feed',
		['key_down international_backslash', 'text 60', 'key_up international_backslash'],
		['keydown IntlBackslash prevented']);
	await evaluate(`(() => {
		const canvas = document.getElementById('canvas');
		for (const type of ['keydown', 'keyup']) {
			canvas.dispatchEvent(new KeyboardEvent(type, {
				code: 'IntlRo', key: 'Unidentified', bubbles: true, cancelable: true,
			}));
		}
		return true;
	})()`);
	await check('an unselected international position stays unmapped in the DOM feed',
		['key_down ', 'key_up '], ['keydown IntlRo kept']);
	await key('KeyA', { control: true });
	if (input.presses_modifiers) {
		// Control held with Control's own key: a key the browser keeps, as a
		// chord is, since the modifier it sets is its own.
		await check('a chord is a key, not text, and is the browser\'s',
			['key_down control', 'key_down a', 'key_up a', 'key_up control'],
			['keydown ControlLeft kept', 'keydown KeyA kept']);
	} else {
		await check('a chord is a key, not text, and is the browser\'s',
			['key_down a', 'key_up a'], ['keydown KeyA kept']);
	}

	await move(CANVAS);
	await take();
	await wheel(CANVAS, 0, 100);
	await check('the wheel is the game\'s once the canvas has the keyboard, notches signed as on Windows',
		['wheel -1'], ['wheel prevented']);
	await wheel(CANVAS, 50, 0);
	await check('the horizontal wheel keeps the DOM\'s sign',
		['hwheel 0.5'], ['wheel prevented']);

	// A LOST CAPTURE. The keyboard going elsewhere mid-drag ends the drag as
	// WM_CAPTURECHANGED does, and the release that follows is nobody's.
	// Poll only on request: even if browser frames elapse during dispatch,
	// these events all reach the mouse before its next fixed update.
	const poll_mouse = () => evaluate('Module._web_window_test_poll_mouse()');
	await poll_mouse();
	await poll_mouse();
	await press(CANVAS);
	await release(CANVAS);
	await check('a short click reaches the window before the next mouse poll',
		['mouse_move 100 50', 'button_down left', 'button_up left']);
	await pass('the short click has both edges with no held button', async (what) =>
		expect(what, await poll_mouse(), 6));
	await pass('the next poll has no click edges', async (what) =>
		expect(what, await poll_mouse(), 0));
	await press(CANVAS);
	await release(CANVAS);
	await press(CANVAS);
	await check('pending click then held press before focus leaves',
		['mouse_move 100 50', 'button_down left', 'button_up left',
			'mouse_move 100 50', 'button_down left']);
	await evaluate('document.getElementById("elsewhere").focus(), true');
	await check('focus cancellation discards the pending gesture',
		['capture_lost', 'keyboard_lost']);
	await release(OUTSIDE);
	await check('the cancelled release is not delivered', []);
	await pass('no pending click is reported after cancellation', async (what) =>
		expect(what, await poll_mouse(), 0));
	await evaluate('document.getElementById("canvas").focus(), true');
	await check('focus can return without a gesture', ['keyboard_gained']);
	await pass('focus return synthesises no button edge', async (what) =>
		expect(what, await poll_mouse(), 0));
	await move(CANVAS);
	await take();
	await press(CANVAS);
	await check('a second press', ['mouse_move 100 50', 'button_down left']);
	await evaluate('document.getElementById("elsewhere").focus(), true');
	await check('the keyboard leaving mid-drag loses the capture first',
		['capture_lost', 'keyboard_lost']);
	await move(OUTSIDE, true);
	await release(OUTSIDE);
	await check('the release after a lost capture reaches nothing', []);

	await press(OUTSIDE);
	await release(OUTSIDE);
	await check('a press and release beside the canvas reach nothing', []);

	// Tab is how a keyboard leaves the canvas: the browser keeps it, so focus
	// moves on, and the key that left is released with the keyboard. The
	// pointer comes back first, as a mouse's does: a WebDriver press is a move
	// to the position and then the button, and the move is reported too.
	await move(CANVAS);
	await take();
	await press(CANVAS);
	await release(CANVAS);
	await check('a click gives the canvas the keyboard back',
		['mouse_move 100 50', 'button_down left', 'keyboard_gained', 'button_up left']);
	await key('Tab');
	const tab_keys = input.option_tab ? ['key_down alt', 'key_down tab'] : ['key_down tab'];
	const tab_defaults = input.option_tab ? ['keydown AltLeft kept', 'keydown Tab kept'] : ['keydown Tab kept'];
	await check('Tab is the browser\'s, and takes the keyboard away',
		[...tab_keys, 'keyboard_lost'], tab_defaults);

	// VISIBILITY, the page's foreground. A repeat says nothing.
	await set_hidden(true);
	await check('a hidden page suspends', ['suspending']);
	await set_hidden(true);
	await check('a second hidden says nothing', []);
	await set_hidden(false);
	await check('a shown page resumes', ['resuming']);
	await evaluate('delete document.hidden, delete document.visibilityState, true');

	// THE SIZE, MEASURED BEFORE EVERY TICK: a CSS resize and a change of
	// device pixel ratio, neither of which any event announces.
	await evaluate('document.getElementById("canvas").style.width = "300px", true');
	await frames(3);
	await check('a CSS resize is reported before the frame that draws into it, and never below the minimum',
		['size 320 250']);
	await evaluate('document.getElementById("canvas").style.width = "480px", true');
	await frames(3);
	await check('a CSS resize above the minimum is the box', ['size 480 250']);

	await input.device_pixel_ratio(2);
	await frames(3);
	await check('a device pixel ratio of two doubles the buffer', ['size 960 500']);
	// From beside the canvas, so that there is a move to report: WebDriver
	// sends none to where the pointer already is.
	await move(OUTSIDE);
	await move(CANVAS);
	await check('and a CSS position is scaled into it', ['mouse_move 200 100']);
	await input.device_pixel_ratio(null);
	await frames(3);
	await check('and back', ['size 480 250']);

	// THE BOX THE BROWSER SNAPS TO: a canvas at y = 20.25 CSS pixels and
	// 302.4 tall covers 20.25 to 322.65, which Chromium snaps to a 303-pixel
	// box although 302.4 rounds to 302. The buffer is the box the page's own
	// device-pixel-content-box observer reports while that box is within a
	// pixel of the CSS size times devicePixelRatio, or the browser resamples
	// the buffer and it blurs (REV#97); otherwise it is that product rounded.
	// Firefox under WebDriver BiDi reports a box at its own scale rather than
	// the ratio of one the viewport is given, so it takes the second branch,
	// and input.snaps_device_box says which browser must take the first. At
	// the ratio of one the page starts at, because an emulated ratio in
	// Chromium changes devicePixelRatio and not the box.
	await evaluate(`(() => {
		window.labradorDeviceBox = null;
		const canvas = document.getElementById('canvas');
		canvas.style.top = '20.25px';
		canvas.style.height = '302.4px';
		const observer = new ResizeObserver((entries) => {
			const sizes = entries[entries.length - 1].devicePixelContentBoxSize;
			window.labradorDeviceBox = sizes && sizes[0] ?
				[sizes[0].inlineSize, sizes[0].blockSize] : 'unreported';
		});
		try {
			observer.observe(canvas, { box: 'device-pixel-content-box' });
		} catch (error) {
			if (!(error instanceof TypeError)) throw error;
			window.labradorDeviceBox = 'unreported';
		}
		return true;
	})()`);
	await frames(4);
	await pass('at a fractional device-pixel position the buffer follows the reported box or rounded scale', async (what) => {
		const box = await evaluate('JSON.stringify(window.labradorDeviceBox)').then(JSON.parse);
		const ratio = await evaluate('devicePixelRatio');
		const sizes = (await take()).filter((entry) => entry.startsWith('size '));
		const exact = [480 * ratio, 302.4 * ratio];
		const snapped = Array.isArray(box) &&
			box.every((side, i) => Math.abs(side - exact[i]) < 1);
		if (input.snaps_device_box) {
			expect(`step ${step}, ${what}: the box`, box, [480, 303]);
		}
		const expected = snapped ? box : exact.map(Math.round);
		expect(`step ${step}, ${what} (box ${JSON.stringify(box)}, ratio ${ratio})`,
			sizes.slice(-1), [`size ${expected[0]} ${expected[1]}`]);
	});
	await evaluate(`(() => {
		const canvas = document.getElementById('canvas');
		canvas.style.top = '';
		canvas.style.height = '';
		return true;
	})()`);
	await frames(4);
	await pass('and back to a whole position', async (what) => {
		expect(`step ${step}, ${what}`,
			(await take()).filter((entry) => entry.startsWith('size ')).slice(-1),
			['size 480 250']);
	});

	// TOUCH: pointer events with ids, focusing the canvas as a click does,
	// and never the mouse as well.
	// A touch's entry is compared as a call, a point - any point for null,
	// and the one captured from an earlier step otherwise - and a position
	// within input.touch_slack client pixels of the one given.
	const touch_point = (entry, call, point, x, y) => {
		const found = /^(touch_[a-z]+) (\d+)(?: (-?\d+) (-?\d+))?$/.exec(entry ?? '');
		if (!found || found[1] !== call || (point !== null && found[2] !== point)) return undefined;
		if (x !== undefined && (found[3] === undefined ||
			Math.abs(Number(found[3]) - x) > input.touch_slack ||
			Math.abs(Number(found[4]) - y) > input.touch_slack)) return undefined;
		return found[2];
	};
	const touches = (what, log, expected) => {
		const points = [];
		const matched = log.length === expected.length && expected.every((want, index) => {
			if (typeof want === 'string') return log[index] === want;
			const point = touch_point(log[index], ...want);
			points.push(point);
			return point !== undefined;
		});
		if (!matched) {
			const described = expected.map((want) => (typeof want === 'string' ? want :
				[want[0], want[1] ?? '<any>', ...want.slice(2)].join(' ')));
			throw new Error(`step ${step}, ${what}\n  expected ${JSON.stringify(described)}` +
				` (positions within ${input.touch_slack})\n  received ${JSON.stringify(log)}`);
		}
		return points;
	};
	const by_point = (point) => (a, b) => Number(point(a)) - Number(point(b));

	if (input.synthetic_touch) console.log('Touch checks use synthetic DOM pointers. Browser touch and compatibility mouse generation are untested.');
	await input.touch_begin();
	await touch('touchStart', [[120, 70, 0]]);
	const [one] = await pass('a touch goes down and focuses the canvas', async (what) =>
		touches(what, await take(), [['touch_down', null, 100, 50], 'keyboard_gained']));

	await touch('touchStart', [[120, 70, 0], [220, 120, 1]]);
	const [two] = await pass('a second finger is a second point', async (what) => {
		const points = touches(what, await take(), [['touch_down', null, 200, 100]]);
		if (points[0] === one) {
			throw new Error(`step ${step}: the second finger has the first's point, ${one}`);
		}
		return points;
	});

	// A browser delivers pointer moves with the next frame rather than as
	// they come, as it does a mouse's.
	await touch('touchMove', [[130, 70, 0], [220, 130, 1]]);
	await frames(2);
	await pass('both move, in whichever order the browser sends them', async (what) =>
		touches(what, (await take()).sort(by_point((entry) => entry.split(' ')[1])),
			[['touch_move', one, 110, 50], ['touch_move', two, 200, 110]]
				.sort(by_point((want) => want[1]))));
	await touch('touchEnd', [[130, 70, 0]]);
	await pass('the first lifts, keeping its point', async (what) =>
		touches(what, await take(), [['touch_up', one, 110, 50], ['touch_cancel', one]]));
	if (input.cancels_touch) {
		await touch('touchCancel', []);
		// Twice: pointercancel, then the capture it lost, which Touch ignores
		// for a point already gone (dom_events.h).
		await check(input.synthetic_touch ? 'a synthetic cancel retires the second point' : 'the browser taking the second is a cancel',
			[`touch_cancel ${two}`, `touch_cancel ${two}`]);
	} else {
		// No cancel to send: the second lifts as the first did, so that the
		// tap below starts with no finger down.
		await touch('touchEnd', [[220, 130, 1]]);
		await pass('the second lifts, keeping its point (no browser cancel to send)', async (what) =>
			touches(what, await take(), [['touch_up', two, 200, 110], ['touch_cancel', two]]));
	}

	await touch('touchStart', [[120, 70, 2]]);
	await touch('touchEnd', []);
	await frames(3);
	await pass(input.synthetic_touch ? 'a synthetic tap reaches the touch listener' : 'a tap is a touch and nothing the mouse hears', async (what) =>
		touches(what, await take(),
			[['touch_down', null, 100, 50], ['touch_up', null, 100, 50], ['touch_cancel', null]]));
	await input.touch_end();

	// FULL SCREEN, as the browser decides it.
	const without_sizes = (log) => log.filter((entry) => !entry.startsWith('size '));
	await evaluate('document.getElementById("canvas").requestFullscreen().then(() => true)',
		{ user_gesture: true });
	await frames(3);
	await pass('a granted request is reported', async (what) =>
		expect(`step ${step}, ${what}`, without_sizes(await take()), ['fullscreen 1']));
	await evaluate('document.exitFullscreen().then(() => true)');
	await frames(3);
	await pass('leaving is reported, and the box is the page\'s again', async (what) => {
		const left = await take();
		// Safari can deliver mouse movement and transient focus changes as
		// its native window returns to the page. Require the complete focus
		// pair if it occurs, and check where focus actually ends up.
		const transition = without_sizes(left).filter((entry) => !/^mouse_move -?\d+ -?\d+$/.test(entry));
		const keyboard_event = (entry) => entry === 'keyboard_lost' || entry === 'keyboard_gained';
		const focus = transition.filter(keyboard_event);
		expect(`step ${step}, ${what}: leaving`, transition.filter((entry) => !keyboard_event(entry)),
			['fullscreen 0']);
		if (focus.length > 0) {
			expect(`step ${step}, ${what}: focus events`, focus, ['keyboard_lost', 'keyboard_gained']);
		}
		expect(`step ${step}, ${what}: canvas focus`,
			await evaluate('document.hasFocus() && document.activeElement === Module.canvas'), true);
		expect(`step ${step}, ${what}: the box`,
			left.filter((entry) => entry.startsWith('size ')).slice(-1), ['size 480 250']);
	});
	await evaluate('document.getElementById("canvas").requestFullscreen().then(() => false, () => true)');
	await frames(3);
	await check('a refused request is reported on the next frame', ['fullscreen 0']);
	await check_fullscreen_requests(page, input);

	// CLOSED: the page is told once, synchronously; the owner hears on_closed
	// once, on a later task; and nothing else reaches the owner after it -
	// no input, and no frame.
	await pass('closing tells the page before the owner', async (what) =>
		expect(`step ${step}, ${what}`,
			await evaluate('Module._web_window_test_close(), JSON.stringify(Module.labradorWindowLog.splice(0))')
				.then(JSON.parse), ['page quit']));
	await frames(2);
	await pass('the owner hears on_closed once, after the page', async (what) =>
		expect(`step ${step}, ${what}`, await raw(), ['closed']));
	await press(CANVAS);
	await release(CANVAS);
	await key('ArrowLeft');
	await wheel(CANVAS, 0, 100);
	await frames(3);
	await pass('a closed window says nothing more, and ticks no more', async (what) =>
		expect(`step ${step}, ${what}`, await raw(), []));

	// FAILED: a second window on the canvas whose owner throws on a key. The
	// page hears the error, then the owner hears on_closed once; nothing more.
	await evaluate(`(Module.onError = (message) =>
		Module.labradorWindowLog.push('page error ' + message),
		document.getElementById('canvas').focus(),
		Module._web_window_test_reopen_throwing(), true)`);
	await raw();
	await key('ArrowLeft');
	await frames(3);
	await pass('a failure tells the page, then the owner hears on_closed', async (what) =>
		expect(`step ${step}, ${what}`, (await raw()).filter((entry) => entry !== 'tick'),
			['page error web window test failure', 'closed']));
}
