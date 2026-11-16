// The game-owned binding row through real browser keyboard/pointer events.
// No gamepad mapping or persistence is implied by this desktop capture flow.
import { writeFileSync } from 'node:fs';
const HOOK = `(() => {
	const state = { initialized: false, quit: false };
	window.componentsBinding = state;
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
