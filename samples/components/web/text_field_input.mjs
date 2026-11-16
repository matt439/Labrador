// Desktop text-field behavior through the real canvas keydown/focus feed.
// The gallery's font is ASCII-only. Richer BMP glyphs and UTF-8 deletion are
// portable component tests; this is not an IME or phone keyboard check.
export { prepare, ready, strict_complaints } from './pointer_input.mjs';

export async function exercise_text_field(page, helpers) {
	const { at, mouse, frames, heard, assert, status_since } = helpers;
	const click = async (point) => {
		await mouse('mousePressed', point, 1);
		await frames();
		await mouse('mouseReleased', point, 0);
		await frames();
	};
	const key = async (value, code, key_code) => {
		await page.send('Input.dispatchKeyEvent', {
			type: 'keyDown', key: value, code,
			windowsVirtualKeyCode: key_code, nativeVirtualKeyCode: key_code,
		});
		await frames();
		await page.send('Input.dispatchKeyEvent', {
			type: 'keyUp', key: value, code,
			windowsVirtualKeyCode: key_code, nativeVirtualKeyCode: key_code,
		});
		await frames();
	};
	const last = (from) => status_since(from).at(-1);
	const name = at(980, 510);
	let from = heard.length;
	await click(name);
	assert(last(from)?.includes('Name editing: Player'), 'clicking Name did not start editing');
	from = heard.length;
	await key('R', 'KeyR', 82);
	assert(last(from)?.includes('Name editing: PlayerR'), 'R was not fed as typed text');
	assert(!status_since(from).some((line) => line.includes('animations restarted')),
		'R restarted previews while editing');
	from = heard.length;
	await key('é', 'KeyE', 69);
	assert(last(from)?.includes('Name: missing font glyph'), 'unsupported glyph was not refused');
	await key('Backspace', 'Backspace', 8);
	await key('a', 'KeyA', 65);
	from = heard.length;
	await key('Enter', 'Enter', 13);
	assert(last(from)?.includes('Name saved: Playera'), 'Enter did not commit accepted text');
	console.log('ok - text commit and unsupported-glyph refusal');

	await click(name);
	await key('Z', 'KeyZ', 90);
	from = heard.length;
	await key('Escape', 'Escape', 27);
	assert(last(from)?.includes('Name kept: Playera'), 'Escape did not restore the committed value');
	assert(await page.evaluate('window.componentsPointer.quit === false'), 'Escape quit while editing');
	console.log('ok - Escape cancels the draft without quitting');
	from = heard.length;
	await key('Enter', 'Enter', 13);
	assert(last(from)?.includes('Name editing: Playera'), 'opening Enter also confirmed the field');
	await key('Q', 'KeyQ', 81);
	from = heard.length;
	await page.evaluate('document.getElementById("canvas").blur(); true');
	await frames();
	assert(last(from)?.includes('Name kept: Playera'), 'canvas keyboard loss did not cancel editing');
	from = heard.length;
	await key('Z', 'KeyZ', 90);
	assert(status_since(from).length === 0, 'unfocused typed input changed the field');
	console.log('ok - canvas blur cancels and unfocused text is ignored');
	// A mousedown reaches the fed mouse before its DOM default focuses the
	// canvas. Settle keyboard/mouse focus before asking the next click to act.
	await page.evaluate('document.getElementById("canvas").focus(); true');
	await frames();

	await click(name);
	await key('?', 'Slash', 191);
	from = heard.length;
	await click(at(10, 10));
	assert(last(from)?.includes('Name kept: Playera'), 'outside press did not cancel the draft');
	console.log('ok - an outside press cancels the draft');
	await click(name);
	for (let digit = 1; digit <= 5; ++digit) {
		await key(String(digit), `Digit${digit}`, 48 + digit);
	}
	from = heard.length;
	await key('6', 'Digit6', 54);
	assert(last(from)?.includes('Name: 12 chars maximum'), 'length limit was not shown');
	from = heard.length;
	await key('Enter', 'Enter', 13);
	assert(last(from)?.includes('Name saved: Playera12345'), 'a refused character partially changed the draft');
	console.log('ok - desktop Name editing, end backspace, commit/cancel, glyph/length refusal and focus loss');
}

export default async function drive(page) {
	const heard = [];
	page.on((message) => {
		if (message.method === 'Runtime.consoleAPICalled') {
			heard.push(message.params.args.map((arg) => String(arg.value ?? '')).join(' '));
		}
	});
	const assert = (condition, message) => {
		if (!condition) { throw new Error(message); }
	};
	const frames = (count = 4) => page.evaluate(`new Promise((done) => {
		let left = ${count};
		const step = () => { if (--left <= 0) { done(true); } else { requestAnimationFrame(step); } };
		requestAnimationFrame(step);
	})`);
	await frames(8);
	const box = await page.evaluate(`(() => {
		const bounds = document.getElementById('canvas').getBoundingClientRect();
		return { x: bounds.left, y: bounds.top, width: bounds.width, height: bounds.height };
	})()`);
	const at = (x, y) => ({ x: box.x + x * box.width / 1280, y: box.y + y * box.height / 720 });
	const mouse = (type, point, buttons) => page.send('Input.dispatchMouseEvent', {
		type, x: point.x, y: point.y, button: 'left', buttons, clickCount: 1,
	});
	const status_since = (from) => heard.slice(from).filter((line) => line.startsWith('Components: '));
	await exercise_text_field(page, { at, mouse, frames, heard, assert, status_since });
	await page.send('Input.dispatchKeyEvent', {
		type: 'keyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27,
	});
	await frames();
	assert(await page.evaluate('window.componentsPointer.quit === true'), 'Escape after editing did not quit');
}
