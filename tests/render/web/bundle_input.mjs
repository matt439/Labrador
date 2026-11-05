// The packaged host page, rather than the development shell: real module
// instances and frames, quit/replay, the phone gate and the host's recovery UI.
// The hooks, the substituted responses and the steps it shares with the site's
// own page's driver (host_input.mjs) are in bundle_page.mjs.

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

export default async function drive(page) {
	const { evaluate, send } = page;
	const { assert, until, frames, button_visible, button_exposed, accessibility_note,
		click_play, key, loaded, quit, navigate, canvas_box, desktop_instructions, play_restore_replay,
		real_loss_and_tab_switch, repeated_play, context_timeout,
		truncated_and_runtime_failure } = steps(page, state);
	const requests = state.requests;

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
	state.phase = 'controller gate';
	const before_controller = requests.length;
	await navigate('controller-gate');
	await frames(20);
	assert(await evaluate("document.getElementById('play').hidden"), 'a connected but unused pad lifted the gate');
	await evaluate('window.labradorBundle.pad.buttons[0].pressed = true');
	await until(button_visible, 'a controller button did not lift the touch gate');
	assert(!fetched_module(state, before_controller),
		'the controller gate downloaded the module before Play');
	console.log('ok - a controller button opens Play without fetching the module');

	await send('Emulation.setTouchEmulationEnabled', { enabled: false });
	for (const [name, advice] of [['no-webgl2', 'hardware acceleration'],
		['no-exceptions', 'WebAssembly exception support']]) {
		state.phase = name;
		const before = requests.length;
		await navigate(name);
		await click_play();
		await until("document.getElementById('status').classList.contains('error') && " +
			"document.getElementById('play').hidden", 'unsupported browser offered no explanation');
		assert((await evaluate("document.getElementById('status').textContent")).includes(advice),
			'the support failure did not give player advice');
		assert(!fetched_module(state, before), 'an unsupported browser downloaded the module');
	}
	console.log('ok - unsupported graphics or exceptions show advice without downloading the module');
	state.phase = 'play and replay';
	await navigate('desktop');
	await desktop_instructions();
	await play_restore_replay();

	// BDP-09: the stage follows the screen. Upright on a phone-sized viewport
	// the drawn canvas is taller than wide; on a desktop it is 16:9. Either
	// way all of it is on screen.
	const on_screen = (box) => box.left >= 0 && box.top >= 0 &&
		box.right <= box.view_width + 0.5 && box.bottom <= box.view_height + 0.5;
	state.phase = 'portrait layout';
	await send('Emulation.setDeviceMetricsOverride',
		{ width: 390, height: 844, deviceScaleFactor: 3, mobile: true });
	await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
	await navigate('portrait');
	await key('KeyA', 65);
	await click_play();
	await loaded(0);
	let box = await canvas_box();
	assert(on_screen(box) && box.height > box.width, `portrait canvas ${JSON.stringify(box)}`);
	await quit(0);
	state.phase = 'desktop layout';
	await send('Emulation.setTouchEmulationEnabled', { enabled: false });
	await send('Emulation.setDeviceMetricsOverride',
		{ width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
	await navigate('landscape');
	await click_play();
	await loaded(0);
	box = await canvas_box();
	assert(on_screen(box) && Math.abs(box.width * 9 / 16 - box.height) <= 1,
		`desktop canvas ${JSON.stringify(box)}`);
	await quit(0);
	await send('Emulation.clearDeviceMetricsOverride');
	console.log('ok - an upright phone viewport draws a tall canvas and a desktop one 16:9, each on screen');

	await real_loss_and_tab_switch();
	await repeated_play();
	await context_timeout('graphics have not returned');

	state.phase = 'download failure';
	await navigate('download-failure');
	await click_play();
	await until("document.getElementById('status').classList.contains('error') && " + button_visible,
		'a failed loader download did not show an error with a recovery button');
	assert(await evaluate("document.getElementById('status').textContent === 'The game could not download. Check your connection and try again.' && " +
		"document.getElementById('play').textContent === 'Try again'"), 'download failure message or Try again is missing');
	assert(await evaluate(button_exposed), 'a failed download leaves Try again covered');
	assert(await evaluate('window.labradorBundle.runs.length === 0'), 'a failed download still called the factory');
	console.log('ok - a failed loader download gives connection advice and an exposed Try again button');

	await truncated_and_runtime_failure();
}
