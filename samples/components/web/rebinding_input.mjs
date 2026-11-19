// The game-owned binding row through real browser keyboard/pointer events.
// No gamepad mapping or persistence is implied by this desktop capture flow.
import { writeFileSync } from 'node:fs';
const HOOK = `(() => {
	const state = { initialized: false, quit: false, layoutRequests: 0 };
	window.componentsBinding = state;
	const keyboard = new URLSearchParams(location.search).has('layout-events')
		? new EventTarget() : navigator.keyboard;
	if (keyboard) {
		// A disposable-page guard, not a browser/profile permission change.
		// Any accidental map/lock request fails this fixture before it can prompt.
		keyboard.getLayoutMap = () => { ++state.layoutRequests; throw new Error('unexpected layout-map request'); };
		keyboard.lock = () => { ++state.layoutRequests; throw new Error('unexpected keyboard-lock request'); };
		if (new URLSearchParams(location.search).has('layout-events')) {
			Object.defineProperty(navigator, 'keyboard', { value: keyboard, configurable: true });
			window.componentsLabelKeyboard = keyboard;
		}
	}
	let module;
	Object.defineProperty(window, 'Module', {
		configurable: true,
		get() { return module; },
		set(value) {
			module = value;
			const initialized = value.onRuntimeInitialized;
			value.onRuntimeInitialized = function () {
				state.initialized = true;
				return initialized?.apply(this, arguments);
			};
			const quit = value.onQuit;
			value.onQuit = function () {
				state.quit = true;
				return quit?.apply(this, arguments);
			};
		},
	});
})()`;

export const ready = 'window.componentsBinding?.initialized === true';
export const strict_complaints = true;

export async function prepare(page) {
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: HOOK });
	await page.send('Emulation.setFocusEmulationEnabled', { enabled: true });
}

const frame_script = (count) => `new Promise((resolve) => {
	let left = ${count};
	const tick = () => { if (--left <= 0) resolve(); else requestAnimationFrame(tick); };
	requestAnimationFrame(tick);
})`;

export default async function drive(page) {
	const heard = [];
	page.on((message) => {
		if (message.method === 'Runtime.consoleAPICalled') {
			heard.push(message.params.args.map((arg) => String(arg.value ?? '')).join(' '));
		}
	});
	const assert = (condition, reason) => { if (!condition) throw new Error(reason); };
	const frames = (count = 6) => page.evaluate(frame_script(count));
	const since = (from, text) => heard.slice(from).filter((line) => line.includes(text));
	const key = async (type, code) => {
		const names = { Enter: ['Enter', 13], Escape: ['Escape', 27], Backspace: ['Backspace', 8],
			ArrowDown: ['ArrowDown', 40], ArrowRight: ['ArrowRight', 39] };
		const [name, number] = names[code] ?? [code.slice(3).toLowerCase(), code.charCodeAt(3)];
		await page.send('Input.dispatchKeyEvent', { type, code, key: name, windowsVirtualKeyCode: number });
		await frames();
	};
	const press = async (code) => { await key('keyDown', code); await key('keyUp', code); };
	await frames(10);
	const box = await page.evaluate(`(() => {
		const value = document.getElementById('canvas').getBoundingClientRect();
		return { x: value.x, y: value.y, width: value.width, height: value.height };
	})()`);
	const click = async (x, y) => {
		const point = { x: box.x + x * box.width / 1280, y: box.y + y * box.height / 720 };
		for (const [type, buttons] of [['mousePressed', 1], ['mouseReleased', 0]]) {
			await page.send('Input.dispatchMouseEvent', { ...point, type, button: 'left', buttons, clickCount: 1 });
			await frames();
		}
	};

	// Click the new left row without moving any of the original five targets.
	await click(500, 500);
	await press('Escape');

	// Layout facts are display-only. These explicit DOM fixtures choose a
	// character independently of its physical code; they do not change the
	// host's layout or establish physical non-US keyboard acceptance.
	const observed = async (code, value, extra = {}) => {
		for (const type of ['keydown', 'keyup']) {
			await page.evaluate(`document.getElementById('canvas').dispatchEvent(
				new KeyboardEvent(${JSON.stringify(type)}, {
					code: ${JSON.stringify(code)}, key: ${JSON.stringify(value)},
					bubbles: true, cancelable: true, ...${JSON.stringify(extra)}
				})); true`);
			await frames();
		}
	};
	let label_from = heard.length;
	await press('ArrowRight');
	assert(since(label_from, 'Label: d / token d (unavailable)').length === 1,
		'unseen default key did not use a stable-token fallback');
	await press('ArrowRight');
	label_from = heard.length;
	await observed('KeyR', 'q');
	assert(since(label_from, 'Label: q / token r (layout)').length === 1,
		'observed label was not displayed separately from its token');
	assert(since(label_from, 'Both animation previews restarted.').length === 1,
		'label text remapped the physical restart binding');
	label_from = heard.length;
	await observed('KeyQ', 'a');
	assert(since(label_from, 'Both animation previews restarted.').length === 0,
		'the displayed character became the action key');
	label_from = heard.length;
	await observed('KeyR', '\u00E9');
	assert(since(label_from, 'Label: r / token r (font fallback)').length === 1,
		'the ASCII gallery font accepted an unavailable accent glyph');
	if (process.env.LABRADOR_COMPONENTS_LABEL_CAPTURE) {
		const screenshot = await page.send('Page.captureScreenshot', { format: 'png' });
		writeFileSync(process.env.LABRADOR_COMPONENTS_LABEL_CAPTURE,
			Buffer.from(screenshot.data, 'base64'));
	}
	await observed('KeyR', 'z');
	label_from = heard.length;
	await observed('KeyR', 'Z', { shiftKey: true });
	await observed('KeyR', 'Z', { modifierCapsLock: true });
	await observed('KeyR', 'x', { isComposing: true });
	await observed('KeyR', 'Dead');
	await observed('KeyR', 'ab');
	await observed('KeyR', '\uD800x');
	await observed('KeyR\u0000junk', 'x');
	await observed('UnknownPosition', 'x');
	assert(since(label_from, 'Components label:').length === 0,
		'modified, composing, dead, ambiguous or unknown input changed a label');
	await page.evaluate('document.getElementById("canvas").blur(); true');
	await frames();
	assert(since(label_from, 'Label: r / token r (unavailable)').length === 1,
		'focus loss retained an observed layout label');
	await page.evaluate('document.getElementById("canvas").focus(); true');
	await frames();
	label_from = heard.length;
	await press('KeyR');
	assert(since(label_from, 'Label: r / token r (layout)').length === 1,
		'a fresh real canvas key event did not restore its observed label');
	if (await page.evaluate('Boolean(window.componentsLabelKeyboard)')) {
		label_from = heard.length;
		await page.evaluate('window.componentsLabelKeyboard.dispatchEvent(new Event("layoutchange")); true');
		await frames();
		assert(since(label_from, 'Label: r / token r (unavailable)').length === 1,
			'optional layoutchange notification did not invalidate labels');
	}
	assert((await page.evaluate('window.componentsBinding.layoutRequests')) === 0,
		'the label route requested a layout map or keyboard lock');
	console.log('ok - observed labels, font/unavailable fallback and refresh preserve stable binding identities');

	let from = heard.length;
	await key('keyDown', 'Enter');
	await key('keyDown', 'KeyT');
	await key('keyUp', 'KeyT');
	assert(since(from, 'Binding saved:').length === 0, 'held opener accepted a candidate');
	await key('keyUp', 'Enter');
	await frames();

	from = heard.length;
	await key('keyDown', 'KeyD');
	await frames(20);
	assert(since(from, 'Binding rejected: duplicate').length === 1,
		`duplicate/held candidate did not refuse once: ${JSON.stringify(heard.slice(from))}`);
	assert(since(from, 'Binding saved:').length === 0, 'duplicate mutated the table');
	if (process.env.LABRADOR_COMPONENTS_BINDING_CAPTURE) {
		const screenshot = await page.send('Page.captureScreenshot', { format: 'png' });
		writeFileSync(process.env.LABRADOR_COMPONENTS_BINDING_CAPTURE,
			Buffer.from(screenshot.data, 'base64'));
	}
	await key('keyUp', 'KeyD');
	from = heard.length;
	await press('ArrowDown');
	assert(since(from, 'Binding rejected: protected').length === 1, 'navigation key became a binding');
	from = heard.length;
	await press('KeyT');
	assert(since(from, 'Binding saved: restart_previews=t').length === 1, 'valid key did not bind');
	assert(since(from, 'Both animation previews restarted.').length === 0,
		'captured key also ran the action on its commit tick');
	console.log('ok - held opener, duplicate and protected keys preserve capture and action policy');

	from = heard.length;
	await press('KeyR');
	assert(since(from, 'Both animation previews restarted.').length === 0, 'old binding still fires');
	await press('KeyT');
	assert(since(from, 'Both animation previews restarted.').length === 1, 'new binding did not fire');
	await press('ArrowRight');
	await press('Enter');
	from = heard.length;
	await press('Escape');
	assert(since(from, 'Binding capture cancelled.').length === 1, 'Escape did not cancel capture');
	assert(!(await page.evaluate('window.componentsBinding.quit')), 'capture cancellation quit the gallery');
	await press('Backspace');
	from = heard.length;
	await press('KeyR');
	assert(since(from, 'Both animation previews restarted.').length === 1, 'reset did not restore restart');
	console.log('ok - valid rebinding, cancellation and reset keep the original quit route');

	await press('Enter');
	from = heard.length;
	await page.evaluate('document.getElementById("canvas").blur(); true');
	await frames();
	assert(since(from, 'Binding capture cancelled: focus lost.').length === 1,
		'actual canvas blur did not cancel binding capture');
	await page.evaluate('document.getElementById("canvas").focus(); true');
	await frames();

	// The neighbouring text field owns letters while editing: R and D must
	// neither restart the previews nor reset difficulty behind that editor.
	await press('ArrowDown');
	await press('Enter');
	from = heard.length;
	await press('KeyR');
	await press('KeyD');
	await press('Escape');
	assert(since(from, 'Both animation previews restarted.').length === 0,
		'a bound action fired while the neighbouring Name field edited');
	assert(since(from, 'Difficulty reset to Normal.').length === 0,
		'difficulty reset fired while Name edited');
	assert(!(await page.evaluate('window.componentsBinding.quit')), 'Name cancellation quit the gallery');
	await click(500, 500);
	await press('Escape');
	console.log('ok - canvas focus loss cancels and Name editing owns the rebound action keys');

	// Moving focus with the pointer cancels capture; the original row remains usable.
	await press('Enter');
	from = heard.length;
	await click(170, 250);
	await press('ArrowRight');
	assert(since(from, 'Binding capture cancelled: another row.').length === 1, 'pointer focus did not cancel');
	assert(since(from, 'Difficulty is now Hard').length === 1, 'navigation failed after capture refusal/cancel');
	await click(170, 450);
	const deadline = Date.now() + 5000;
	while (!(await page.evaluate('window.componentsBinding.quit'))) {
		assert(Date.now() < deadline, 'original Quit row did not quit');
		await frames();
	}
	console.log('ok - changing rows cancels capture and original pointer navigation/quit still work');
}
