// The site's own LineSweeper page (mattharding.dev's /demo/linesweeper/),
// rather than the bundle's example page: its gate, failure text, controls and
// recovery, driven over real module instances as bundle_input.mjs drives the
// example. The page is the host's, in the host's repository; this runs
// against a checkout of it after `npm run sync-demo`, as
// LineSweeperHostPageTests (docs/port/web.md, section 7).
//
// Where the two pages agree - Play, QUIT and Play again, focus, the
// accessibility note, a forced context loss and its restoration, repeated
// play, the ten-second context wait, a truncated module and onError - the
// steps are bundle_page.mjs's. Where the site's page differs, this says what
// it expects instead:
//
// - A phone, and a browser without WebGL2 or WebAssembly exceptions, get a
//   recording (linesweeper.mp4, preload="none") and a sentence, not an error,
//   and nothing of the module or the recording is fetched.
// - The stage is always the column at 16:9; there is no portrait layout.
// - A loader that cannot be downloaded also gets the recording, with the raw
//   reason in the small #detail line, rather than Try again.
// - Each failure's raw reason is shown in #detail beneath the sentence.

import { READY, bundle_state, expected_complaint as expected, fetched_module,
	prepare as install, steps } from './bundle_page.mjs';

export const ready = READY;
export const strict_complaints = true;

const state = bundle_state();

export function expected_complaint(complaint) {
	return expected(state, complaint);
}

export function prepare(page) {
	return install(state, page);
}

const RECORDING = `(() => {
	const video = document.querySelector('#stage video');
	return video !== null && video.getAttribute('src') === 'linesweeper.mp4' &&
		video.preload === 'none' && video.controls &&
		document.getElementById('start').hidden && document.querySelector('canvas') === null;
})()`;

export default async function drive(page) {
	const { evaluate, send } = page;
	const { assert, until, frames, button_visible, button_exposed, accessibility_note,
		click_play, key, loaded, quit, navigate, canvas_box, play_restore_replay,
		repeated_play, context_timeout, truncated_and_runtime_failure } = steps(page, state);
	const requests = state.requests;
	const status = () => evaluate("document.getElementById('status').textContent");
	const status_is_error = () => evaluate("document.getElementById('status').classList.contains('error')");
	const detail = () => evaluate("document.getElementById('detail').hidden ? null : document.getElementById('detail').textContent");
	const fetched_recording = (from) => requests.slice(from)
		.some((url) => new URL(url).pathname.endsWith('/linesweeper.mp4'));

	state.phase = 'touch gate';
	assert(await evaluate("matchMedia('(pointer: coarse)').matches && !matchMedia('(any-pointer: fine)').matches"),
		'touch emulation did not produce a coarse pointer alone');
	assert(await evaluate(RECORDING), 'a touch-only visitor was not shown the recording in place of Play');
	assert(await evaluate("document.getElementById('play').hidden || document.getElementById('start').hidden"),
		'a touch-only visitor was offered Play');
	assert(await evaluate("document.getElementById('controls').hidden"),
		'the touch gate kept the keyboard controls on show');
	assert(/key or a controller button/.test(await status()) && !await status_is_error(),
		'the touch gate did not say how to play');
	await accessibility_note('#play');
	await frames(10);
	assert(!fetched_module(state) && !fetched_recording(0),
		'the touch gate fetched the module or the recording');
	await key('KeyA', 65);
	assert(await evaluate(button_visible), 'a key did not lift the touch gate');
	assert(await evaluate("document.querySelector('#stage video') === null && !document.getElementById('controls').hidden"),
		'lifting the gate left the recording or hid the controls');
	console.log('ok - touch gate shows the recording and fetches nothing of it or the module until a key');
	state.phase = 'controller gate';
	const before_controller = requests.length;
	await navigate('controller-gate');
	await frames(20);
	assert(await evaluate(RECORDING), 'a connected but unused pad lifted the gate');
	await evaluate('window.labradorBundle.pad.buttons[0].pressed = true');
	await until(button_visible, 'a controller button did not lift the touch gate');
	assert(!fetched_module(state, before_controller),
		'the controller gate downloaded the module before Play');
	console.log('ok - a controller button opens Play without fetching the module');

	await send('Emulation.setTouchEmulationEnabled', { enabled: false });
	for (const [name, advice] of [['no-webgl2', 'hardware acceleration enabled'],
		['no-exceptions', 'an up-to-date browser can']]) {
		state.phase = name;
		const before = requests.length;
		await navigate(name);
		await frames(5);
		assert(await evaluate(RECORDING), 'an unsupported browser was not shown the recording');
		assert((await status()).includes(advice) && !await status_is_error(),
			'the support failure did not give player advice');
		assert(!fetched_module(state, before) && !fetched_recording(before),
			'an unsupported browser fetched the module or the recording');
	}
	console.log('ok - unsupported graphics or exceptions show the recording without downloading the module');

	state.phase = 'play and replay';
	await navigate('desktop');
	assert(await evaluate("!document.getElementById('controls').hidden && " +
		"document.getElementById('controls').textContent.includes('Soft drop') && " +
		"document.getElementById('controls').textContent.includes('Start pauses')"),
		'the desktop page omitted controller or soft-drop instructions');
	await accessibility_note('#play');
	await play_restore_replay();

	// The stage is the column at 16:9 at every width, so an upright phone
	// that has shown a key draws the landscape arrangement, scaled, and so
	// does a desktop. Either way the canvas is never wider than the page.
	const within_width = (box) => box.left >= 0 && box.right <= box.view_width + 0.5;
	const sixteen_nine = (box) => Math.abs(box.width * 9 / 16 - box.height) <= 1;
	state.phase = 'portrait layout';
	await send('Emulation.setDeviceMetricsOverride',
		{ width: 390, height: 844, deviceScaleFactor: 3, mobile: true });
	await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
	await navigate('portrait');
	await key('KeyA', 65);
	await click_play();
	await loaded(0);
	let box = await canvas_box();
	assert(within_width(box) && sixteen_nine(box), `portrait canvas ${JSON.stringify(box)}`);
	await quit(0);
	state.phase = 'desktop layout';
	await send('Emulation.setTouchEmulationEnabled', { enabled: false });
	await send('Emulation.setDeviceMetricsOverride',
		{ width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
	await navigate('landscape');
	await click_play();
	await loaded(0);
	box = await canvas_box();
	assert(within_width(box) && sixteen_nine(box), `desktop canvas ${JSON.stringify(box)}`);
	await quit(0);
	await send('Emulation.clearDeviceMetricsOverride');
	console.log('ok - the stage draws a 16:9 canvas within the page on an upright phone and on a desktop');

	await repeated_play();
	await context_timeout('graphics have not come back');

	state.phase = 'download failure';
	const before_download = requests.length;
	await navigate('download-failure');
	await click_play();
	await until(RECORDING, 'a failed loader download did not show the recording');
	assert(await status() === 'LineSweeper could not be downloaded; some networks block WebAssembly. Here is a recording instead.' &&
		!await status_is_error(), 'download failure message is missing');
	assert(/linesweeper\.js|dynamically imported module/.test(await detail() ?? ''),
		'a failed download did not show its raw reason beneath the sentence');
	assert(await evaluate('window.labradorBundle.runs.length === 0'), 'a failed download still called the factory');
	assert(!fetched_recording(before_download), 'the recording was fetched before it was asked for');
	console.log('ok - a failed loader download shows the recording and its reason, and fetches no recording');

	await truncated_and_runtime_failure(async (reason) => {
		assert((await detail() ?? '').includes(reason), `the raw reason "${reason}" is not beneath the sentence`);
	});
	assert(await evaluate(button_exposed), 'Try again is covered');
}
