// Extends ComponentsPointerPageTests through the internal touch-controls page.
// CDP supplies multiple contacts to Chromium's real pointer feed. The individual
// pointercancel and same-turn blur/focus cases are deliberate DOM injections;
// neither those nor touch emulation qualify real glass, a pen or a phone.
export async function drive_touch_controls(page) {
	const { evaluate, send } = page;
	const readings = [];
	const unsubscribe = page.on((message) => {
		if (message.method !== 'Runtime.consoleAPICalled') return;
		const text = message.params.args.map((arg) => String(arg.value ?? '')).join(' ');
		const match = /Components: Touch controls: A (held|up)  B (held|up)  Stick (-?\d+)%, (-?\d+)%/.exec(text);
		if (match) readings.push({ a: match[1] === 'held', b: match[2] === 'held', x: +match[3], y: +match[4] });
	});
	const assert = (condition, message) => { if (!condition) throw new Error(message); };
	const frames = (count = 6) => evaluate(`new Promise((done) => {
		let left = ${count};
		const next = () => --left <= 0 ? done(true) : requestAnimationFrame(next);
		requestAnimationFrame(next);
	})`);
	const expect = async (a, b, x, y, label, stick_slack = 0) => {
		await frames();
		const got = readings.at(-1);
		assert(got?.a === a && got?.b === b && Math.abs(got?.x - x) <= stick_slack && Math.abs(got?.y - y) <= stick_slack,
			`${label}: expected ${JSON.stringify({ a, b, x, y })}, got ${JSON.stringify(got)}`);
	};
	const at = async (x, y) => evaluate(`(() => {
		const r = document.getElementById('canvas').getBoundingClientRect();
		return { x: r.left + ${x} * r.width / 1280, y: r.top + ${y} * r.height / 720 };
	})()`);
	const key = async (code, vk) => {
		await send('Input.dispatchKeyEvent', { type: 'keyDown', code, key: code, windowsVirtualKeyCode: vk });
		await frames();
		await send('Input.dispatchKeyEvent', { type: 'keyUp', code, key: code, windowsVirtualKeyCode: vk });
		await frames();
	};
	const touch = async (type, points) => {
		const touchPoints = [];
		for (const [id, x, y] of points) touchPoints.push({ id, ...await at(x, y) });
		await send('Input.dispatchTouchEvent', { type, touchPoints });
		await frames();
	};
	const mouse = async (type, x, y, buttons) => {
		await send('Input.dispatchMouseEvent', { type, ...await at(x, y), button: 'left', buttons, clickCount: 1 });
		await frames();
	};
	const clear = async () => { await touch('touchEnd', []); await frames(8); };
	const open = async () => {
		const before = readings.length;
		await key('F2', 113);
		assert(readings.length > before, 'F2 did not open the touch-controls page');
		await expect(false, false, 0, 0, 'new page starts neutral');
	};

	try {
		assert(await evaluate("getComputedStyle(document.getElementById('canvas')).touchAction") === 'none',
			'the game canvas must reserve touch gestures instead of starting browser pan/zoom');
		await open();
		await evaluate(`(() => {
			window.componentsTouchDowns = [];
			document.getElementById('canvas').addEventListener('pointerdown', (event) => {
				if (event.pointerType === 'touch') window.componentsTouchDowns.push({
					id: event.pointerId, x: event.clientX, y: event.clientY,
				});
			});
		})()`);

		// Integer client pixels and the displayed whole percentages introduce
		// at most two percentage points here. Cancelled neutral values stay exact.
		await touch('touchStart', [[1, 290, 390], [2, 800, 360], [3, 1010, 360]]);
		await expect(true, true, 0, 0, 'three fingers hold both buttons and the stick', 2);
		await touch('touchMove', [[1, 590, 390], [2, 800, 360], [3, 1010, 360]]);
		await expect(true, true, 100, 0, 'stick stays owned outside its region and clamps', 2);
		await touch('touchStart', [[1, 590, 390], [2, 800, 360], [3, 1010, 360], [4, 810, 360]]);
		await touch('touchMove', [[1, 590, 390], [2, 700, 360], [3, 1010, 360], [4, 810, 360]]);
		await expect(false, true, 100, 0, 'sliding A outside keeps its owner and rejects another held finger', 2);
		await touch('touchMove', [[1, 590, 390], [2, 800, 360], [3, 1010, 360], [4, 810, 360]]);
		await expect(true, true, 100, 0, 'the original A finger re-enters', 2);
		// The existing WebWindowTests sequence also uses a selected-point end:
		// Chromium releases that point while the other contacts remain down.
		await touch('touchEnd', [[2, 800, 360]]);
		await expect(false, true, 100, 0, 'releasing A does not adopt the rejected fourth finger', 2);
		await touch('touchStart', [[1, 590, 390], [3, 1010, 360], [4, 810, 360], [5, 800, 360]]);
		await expect(true, true, 100, 0, 'a fresh finger can acquire the released button', 2);

		const a = await at(800, 360);
		const cancelled = await evaluate(`(() => {
			const point = window.componentsTouchDowns.findLast((p) => Math.abs(p.x - ${a.x}) < 2);
			if (!point) return false;
			return document.getElementById('canvas').dispatchEvent(new PointerEvent('pointercancel', {
				pointerType: 'touch', pointerId: point.id, clientX: point.x, clientY: point.y,
				bubbles: true, cancelable: true,
			})) === false;
		})()`);
		assert(cancelled, 'the engine did not consume the injected individual pointercancel');
		await expect(false, true, 100, 0, 'individual cancel leaves the other controls held', 2);
		await touch('touchMove', [[1, 580, 390], [3, 1010, 360], [4, 810, 360], [5, 800, 360]]);
		await expect(false, true, 100, 0, 'a cancelled or rejected contact cannot be adopted on movement', 2);
		console.log('ok - simultaneous touch controls preserve ownership, outside motion, release and individual cancel');

		await evaluate(`(() => { const canvas = document.getElementById('canvas'); canvas.blur(); canvas.focus(); })()`);
		await expect(false, false, 0, 0, 'a blur and focus between frames cancels every control');
		await touch('touchMove', [[1, 570, 390], [3, 1010, 360], [4, 810, 360], [5, 800, 360]]);
		await expect(false, false, 0, 0, 'focus return does not replay still-held contacts');
		await clear();
		await touch('touchStart', [[6, 800, 360]]);
		await expect(true, false, 0, 0, 'a fresh contact works after focus cleanup');

		const original = await evaluate('({ width: innerWidth, height: innerHeight, deviceScaleFactor: devicePixelRatio })');
		await send('Emulation.setDeviceMetricsOverride', { width: 1100, height: 900, deviceScaleFactor: 1, mobile: false });
		await expect(false, false, 0, 0, 'resize cancels the held contact');
		await touch('touchMove', [[6, 800, 360]]);
		await expect(false, false, 0, 0, 'resize waits for old contacts to clear');
		await clear();
		await touch('touchStart', [[7, 1010, 360]]);
		await expect(false, true, 0, 0, 'the resized canvas maps a fresh contact to B');
		await send('Emulation.setDeviceMetricsOverride', { ...original, mobile: false });
		await expect(false, false, 0, 0, 'restoring the original canvas cancels again');
		await clear();
		console.log('ok - focus and resize cancel touch values and require fresh contacts');

		await mouse('mousePressed', 800, 360, 1);
		await expect(true, false, 0, 0, 'left mouse previews A');
		await mouse('mouseMoved', 700, 360, 1);
		await expect(false, false, 0, 0, 'mouse slide outside clears held');
		await mouse('mouseMoved', 800, 360, 1);
		await expect(true, false, 0, 0, 'mouse re-entry restores held');
		await mouse('mouseReleased', 800, 360, 0);
		await expect(false, false, 0, 0, 'mouse release clears held');

		await touch('touchStart', [[8, 1100, 88]]);
		await clear();
		assert(await evaluate('window.componentsPointer.quit') !== true, 'Back closed the whole gallery');
		await open();
		await key('Escape', 27);
		await mouse('mousePressed', 1000, 190, 1);
		const before = readings.length;
		await mouse('mouseReleased', 1000, 190, 0);
		assert(readings.length > before, 'the separate Gallery entry did not open the touch page');
		await expect(false, false, 0, 0, 'click entry opens a neutral touch page');
		await key('Escape', 27);
		assert(await evaluate('window.componentsPointer.quit') !== true, 'Escape from the touch page closed the gallery');
		await frames(8);
		console.log('ok - mouse preview and the F2, entry, Back and Escape routes preserve Gallery navigation');
	} finally {
		unsubscribe();
	}
}
