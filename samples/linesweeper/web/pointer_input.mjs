import assert from 'node:assert/strict';
import { module_hook } from '../../../tests/render/web/page_harness.mjs';

// The development page supplies only state depth and the actual camera's
// board-to-client transform. The sample owns its target sizes; this driver
// supplies probe points and real touch events. The packaged module has neither
// observation hook.
const HOOK = `(() => {
	const heard = { initialized: false, quit: false, errors: [], has_quit: false };
	window.linesweeperPointer = heard;
	${module_hook('linesweeperPointerHooked')}
})()`;

export const ready = 'window.linesweeperPointer?.initialized === true';
export const strict_complaints = true;

export async function prepare({ send }) {
	await send('Page.addScriptToEvaluateOnNewDocument', { source: HOOK });
	await send('Emulation.setFocusEmulationEnabled', { enabled: true });
	await send('Emulation.setDeviceMetricsOverride',
		{ width: 1600, height: 1100, deviceScaleFactor: 1, mobile: false });
	await send('Page.bringToFront');
}

export default async function drive({ evaluate, send, wait, on }) {
	const heard = [];
	on((message) => {
		if (message.method === 'Runtime.consoleAPICalled') {
			heard.push(message.params.args.map((argument) => String(argument.value ?? '')).join(' '));
		}
	});
	const until = async (expression) => {
		while (await evaluate(expression) !== true) await wait(20);
	};
	const frames = (count = 4) => evaluate(`new Promise((done) => {
		let remaining = ${count};
		const step = () => --remaining > 0 ? requestAnimationFrame(step) : done(true);
		requestAnimationFrame(step);
	})`);
	const at = (x, y) => evaluate(`(() => {
		Module._linesweeper_test_map_pointer(${x}, ${y});
		const point = Module.labradorTestPointerPoint;
		const canvas = Module.canvas;
		const box = canvas.getBoundingClientRect();
		return { x: box.left + point.x * box.width / canvas.width,
			y: box.top + point.y * box.height / canvas.height };
	})()`);
	const resize = async (width, height) => {
		await evaluate(`Object.assign(Module.canvas.style, { width: '${width}px',
			height: '${height}px', maxWidth: 'none', maxHeight: 'none' }), true`);
		await until(`Module.canvas.width === ${width} && Module.canvas.height === ${height}`);
		await frames();
	};
	const key = async (code, key_code) => {
		await send('Input.dispatchKeyEvent', { type: 'rawKeyDown', code, key: code,
			windowsVirtualKeyCode: key_code, nativeVirtualKeyCode: key_code });
		await until('Module._linesweeper_test_stack_depth() === 2');
		await send('Input.dispatchKeyEvent', { type: 'keyUp', code, key: code,
			windowsVirtualKeyCode: key_code, nativeVirtualKeyCode: key_code });
		await frames();
	};
	const down = async (point) => {
		await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ ...point, id: 1 }] });
		await frames();
	};
	const up = async () => {
		await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
		await frames();
	};
	await until('typeof Module._linesweeper_test_stack_depth === "function" && Module._linesweeper_test_stack_depth() === 1');
	await resize(1280, 720);
	await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
	await key('Escape', 27);
	await down(await at(900, 322));
	assert.equal(await evaluate('Module._linesweeper_test_stack_depth()'), 2,
		'a touch outside the expanded resume band does not choose it');
	await up();
	await down(await at(490, 322));
	await until('Module._linesweeper_test_stack_depth() === 1');
	console.log('ok - a touch outside the pause band misses and an expanded-band press resumes');
	await up();
	await key('Escape', 27);
	await down(await at(790, 410));
	assert.equal(await evaluate('Module._linesweeper_test_stack_depth()'), 2,
		'the expanded COLOURS band keeps the pause menu open');
	await frames(8);
	await up();
	await down(await at(490, 322));
	await until('Module._linesweeper_test_stack_depth() === 1');
	await up();
	assert.ok(heard.some((line) => line === 'LineSweeper: palette Protanomaly / Protanopia'),
		`the touch beyond the text did not change the palette: ${JSON.stringify(heard)}`);
	console.log('ok - the expanded colour target reaches beyond the glyphs and the selected palette resumes');
	await resize(640, 360);
	await key('Escape', 27);
	await down(await at(490, 322));
	await until('Module._linesweeper_test_stack_depth() === 1');
	await up();
	console.log('ok - the same authored target follows a resized camera and real browser coordinates');
	await key('Escape', 27);
	await down(await at(790, 454));
	await until('window.linesweeperPointer.quit === true');
	assert.deepEqual(await evaluate('window.linesweeperPointer.errors'), []);
	console.log('ok - a touch on the expanded Quit band calls Module.onQuit');
}
