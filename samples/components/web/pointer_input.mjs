// The gallery driven by a real mouse and a real (emulated) touchscreen, as
// ComponentsPointerPageTests: tests/render/web/run_page.mjs --input runs it
// against the built development page in Chromium. Each step dispatches
// input over the DevTools protocol at the CSS position of a row, worked out
// from the canvas's own box, and reads what changed from the console line
// main_web.cpp writes for every status the gallery shows.
//
// What it shows is that the page's pointer rules survive a browser's actual
// geometry and events: the drag, the tap on a value's left half, a release
// off the row that pressed, and a tap on Quit. ComponentsTests holds each
// rule with a hidden window under native null render and audio
// (gallery_pointer_tests.cpp). Touch here is Chromium's emulation, not a
// touchscreen: what a phone's browser delivers is the open check in
// docs/port/web-to-be-tested.md.

// Installed before the page's own scripts, as run_page.mjs installs its hook
// for --quit-keys: Module.onQuit and onRuntimeInitialized are wrapped as the
// page assigns them, so this script can wait for each.
const HOOK = `(() => {
	const heard = { initialized: false, quit: false };
	window.componentsPointer = heard;
	let module;
	Object.defineProperty(window, 'Module', {
		configurable: true,
		enumerable: true,
		get() { return module; },
		set(value) {
			module = value;
			const initialized = value.onRuntimeInitialized;
			value.onRuntimeInitialized = function () {
				heard.initialized = true;
				return initialized?.apply(this, arguments);
			};
			const quit = value.onQuit;
			value.onQuit = function () {
				heard.quit = true;
				return quit?.apply(this, arguments);
			};
		},
	});
})()`;

export const ready = 'window.componentsPointer?.initialized === true';
export const strict_complaints = true;

export async function prepare(page) {
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: HOOK });
	await page.send('Emulation.setFocusEmulationEnabled', { enabled: true });
}

// The gallery's layout (gallery_state.cpp), in its 1280x720 world units.
// Each upper-case name mirrors the lower-case constant of that name there,
// except TRACK_MIDDLE, which is worked out from two of them.
const LABEL_X = 160;
const VALUE_X = 480;
const FIRST_ROW_Y = 240;
const ROW_SPACING = 50;
const TRACK_WIDTH = 300;

// A row's line of text is twenty units tall, so its middle is TEXT_MIDDLE
// below the row's top. A slider's track starts track_inset (4) below that
// top and is track_height (10) tall, so its middle is a unit higher.
const TEXT_MIDDLE = 10;
const TRACK_MIDDLE = 4 + 10 / 2;

const row_y = (row) => FIRST_ROW_Y + row * ROW_SPACING;
const row_middle = (row) => row_y(row) + TEXT_MIDDLE;
const track_middle = (row) => row_y(row) + TRACK_MIDDLE;

// A page expression that settles after `count` animation frames.
const frames_script = (count) => `new Promise((done) => {
	let left = ${count};
	const step = () => { if (--left <= 0) { done(true); } else { requestAnimationFrame(step); } };
	requestAnimationFrame(step);
})`;

export default async function drive(page) {
	const { evaluate, send } = page;
	const heard = [];
	page.on((message) => {
		if (message.method === 'Runtime.consoleAPICalled') {
			heard.push(message.params.args.map((arg) => String(arg.value ?? '')).join(' '));
		}
	});
	const assert = (condition, message) => {
		if (!condition) { throw new Error(message); }
	};
	const frames = (count = 6) => evaluate(frames_script(count));
	const status_since = (from) => heard.slice(from).filter((text) => text.startsWith('Components: '));

	await frames(10);

	// The canvas is 16:9 and the layout is fitted into it, so one scale
	// serves both axes and there are no bars.
	const box = await evaluate(`(() => {
		const r = document.getElementById('canvas').getBoundingClientRect();
		return { x: r.left, y: r.top, width: r.width, height: r.height };
	})()`);
	assert(Math.abs(box.width / box.height - 16 / 9) < 0.01, 'the canvas is not 16:9');
	const at = (x, y) => ({ x: box.x + x * box.width / 1280, y: box.y + y * box.height / 720 });

	const mouse = (type, point, buttons) => send('Input.dispatchMouseEvent', {
		type, x: point.x, y: point.y, button: 'left', buttons, clickCount: 1,
	});
	const tap = async (point) => {
		await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ ...point, id: 1 }] });
		await frames();
		await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
		await frames();
	};

	// A drag on the music slider: pressed halfway along the track, then
	// pulled past its right end, which clamps.
	let from = heard.length;
	const music = track_middle(2);
	await mouse('mousePressed', at(VALUE_X + TRACK_WIDTH / 2, music), 1);
	await frames();
	await mouse('mouseMoved', at(1270, music), 1);
	await frames();
	await mouse('mouseReleased', at(1270, music), 0);
	await frames();
	let said = status_since(from);
	assert(said.some((text) => text.includes('Music slider example: 50%')),
		`pressing the track's middle did not set 50: ${JSON.stringify(said)}`);
	assert(said.at(-1)?.includes('Music slider example: 100%'),
		`dragging past the end did not clamp at 100: ${JSON.stringify(said)}`);
	console.log('ok - a mouse drag sets the slider and clamps past its end');

	// A press on the window row that lets go over another row does nothing.
	from = heard.length;
	await mouse('mousePressed', at(VALUE_X + 80, row_middle(1)), 1);
	await frames();
	await mouse('mouseMoved', at(VALUE_X + 80, row_middle(3)), 1);
	await frames();
	await mouse('mouseReleased', at(VALUE_X + 80, row_middle(3)), 0);
	await frames();
	said = status_since(from);
	assert(said.length === 0, `a release off the pressed row changed something: ${JSON.stringify(said)}`);
	console.log('ok - a release off the row that was pressed changes nothing');

	// Touch, from here on.
	await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });

	// A tap on the left half of the difficulty's value steps back from
	// Normal, and once only: the mouse events a browser may make of the tap
	// do not step it again.
	from = heard.length;
	await tap(at(VALUE_X + 4, row_middle(0)));
	await frames(10);
	said = status_since(from);
	assert(said.length === 1 && said[0].includes('Difficulty is now Easy'),
		`a tap on the value's left half did not step back once: ${JSON.stringify(said)}`);
	console.log('ok - a tap on a value steps it once');

	// A tap on the label column of Quit chooses it, and the page is told.
	await tap(at(LABEL_X + 10, row_middle(4)));
	for (let tries = 0; tries < 100 && await evaluate('window.componentsPointer.quit') !== true; ++tries) {
		await frames(2);
	}
	assert(await evaluate('window.componentsPointer.quit') === true, 'a tap on Quit did not quit');
	console.log('ok - a tap on Quit quits');
}
