// What the web window must tell its owner for each thing a browser delivers
// to it: the input half of WebWindowTests, run by
// tests/render/web/run_page.mjs --input against window_page.cpp's page in
// Chromium, and by tests/render/web/run_firefox.mjs --test WebWindowTests in
// Firefox.
//
// ONE SEQUENCE, TWO PROTOCOLS. drive_window holds every step and what it
// must produce; an input object puts each step through one browser's own
// event dispatch. The default export hands it devtools_input, the DevTools
// protocol's Input and Emulation domains; the Firefox runner hands it its
// WebDriver BiDi input. Either way focus moves because a click moved it, a
// default is prevented or kept because the canvas's listener said so, and a
// touch makes - or does not make - the mouse events a browser makes of a
// touch. Where a protocol cannot produce an input, its input object says so
// and the step that needs it states what was done instead.
//
// THE ONE INPUT THAT IS NOT REAL is visibilitychange: a headless browser has
// no tab strip and no desktop to minimise to, and neither creating and
// activating another tab, nor the lifecycle states, nor minimising its window
// changes document.visibilityState. So the page's document is told it is
// hidden and the event is dispatched at it, which tests what the window does
// with the event and not that a browser sends it.
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
//   page around the chord.
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
//   delivers a touch where it was sent.

const CANVAS = { x: 120, y: 70 };
const OUTSIDE = { x: 650, y: 400 };

const KEYS = {
	ArrowLeft: { key: 'ArrowLeft', code: 37 },
	Space: { key: ' ', code: 32, text: ' ' },
	KeyA: { key: 'a', code: 65, text: 'a' },
	Tab: { key: 'Tab', code: 9 },
};
const CONTROL = 2;

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
	await check('Tab is the browser\'s, and takes the keyboard away',
		['key_down tab', 'keyboard_lost'], ['keydown Tab kept']);

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

	await input.touch_begin();
	await touch('touchStart', [[120, 70, 0]]);
	step++;
	const [one] = touches('a touch goes down and focuses the canvas', await take(),
		[['touch_down', null, 100, 50], 'keyboard_gained']);
	console.log(`ok ${step} - a touch goes down and focuses the canvas`);

	await touch('touchStart', [[120, 70, 0], [220, 120, 1]]);
	step++;
	const [two] = touches('a second finger is a second point', await take(),
		[['touch_down', null, 200, 100]]);
	if (two === one) {
		throw new Error(`step ${step}: the second finger has the first's point, ${one}`);
	}
	console.log(`ok ${step} - a second finger is a second point`);

	// A browser delivers pointer moves with the next frame rather than as
	// they come, as it does a mouse's.
	await touch('touchMove', [[130, 70, 0], [220, 130, 1]]);
	await frames(2);
	step++;
	touches('both move, in whichever order the browser sends them',
		(await take()).sort(by_point((entry) => entry.split(' ')[1])),
		[['touch_move', one, 110, 50], ['touch_move', two, 200, 110]]
			.sort(by_point((want) => want[1])));
	console.log(`ok ${step} - both move`);
	await touch('touchEnd', [[130, 70, 0]]);
	step++;
	touches('the first lifts, keeping its point', await take(),
		[['touch_up', one, 110, 50], ['touch_cancel', one]]);
	console.log(`ok ${step} - the first lifts, keeping its point`);
	if (input.cancels_touch) {
		await touch('touchCancel', []);
		// Twice: pointercancel, then the capture it lost, which Touch ignores
		// for a point already gone (dom_events.h).
		await check('the browser taking the second is a cancel',
			[`touch_cancel ${two}`, `touch_cancel ${two}`]);
	} else {
		// No cancel to send: the second lifts as the first did, so that the
		// tap below starts with no finger down.
		await touch('touchEnd', [[220, 130, 1]]);
		step++;
		touches('the second lifts, keeping its point (no browser cancel to send)', await take(),
			[['touch_up', two, 200, 110], ['touch_cancel', two]]);
		console.log(`ok ${step} - the second lifts, keeping its point (no browser cancel to send)`);
	}

	await touch('touchStart', [[120, 70, 2]]);
	await touch('touchEnd', []);
	await frames(3);
	step++;
	touches('a tap is a touch and nothing the mouse hears', await take(),
		[['touch_down', null, 100, 50], ['touch_up', null, 100, 50], ['touch_cancel', null]]);
	console.log(`ok ${step} - a tap is a touch and nothing the mouse hears`);
	await input.touch_end();

	// FULL SCREEN, as the browser decides it.
	const without_sizes = (log) => log.filter((entry) => !entry.startsWith('size '));
	await evaluate('document.getElementById("canvas").requestFullscreen().then(() => true)',
		{ user_gesture: true });
	await frames(3);
	step++;
	expect(`step ${step}, a granted request is reported`, without_sizes(await take()),
		['fullscreen 1']);
	console.log(`ok ${step} - a granted request is reported`);
	await evaluate('document.exitFullscreen().then(() => true)');
	await frames(3);
	step++;
	const left = await take();
	expect(`step ${step}, leaving is reported`, without_sizes(left), ['fullscreen 0']);
	expect(`step ${step}, and the box is the page's again`,
		left.filter((entry) => entry.startsWith('size ')).slice(-1), ['size 480 250']);
	console.log(`ok ${step} - leaving is reported, and the box is the page's again`);
	await evaluate('document.getElementById("canvas").requestFullscreen().then(() => false, () => true)');
	await frames(3);
	await check('a refused request is reported on the next frame', ['fullscreen 0']);

	// CLOSED: the page is told once, and nothing reaches the owner after it -
	// no input, and no frame.
	await evaluate('Module._web_window_test_close(), true');
	step++;
	expect(`step ${step}, closing tells the page`, await raw(), ['page quit']);
	console.log(`ok ${step} - closing tells the page`);
	await press(CANVAS);
	await release(CANVAS);
	await key('ArrowLeft');
	await wheel(CANVAS, 0, 100);
	await frames(3);
	step++;
	expect(`step ${step}, a closed window says nothing more, and ticks no more`,
		await raw(), []);
	console.log(`ok ${step} - a closed window says nothing more, and ticks no more`);
}
