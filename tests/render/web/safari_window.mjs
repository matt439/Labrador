// The shared WebWindow sequence over classic WebDriver. Mouse, wheel and
// keyboard go through Perform Actions. The script hooks supply the two
// inputs desktop Safari's driver cannot deliver: a DPR override and touch
// pointers (desktop WebKit aliases a touch source to a mouse source).
import { drive_window, expected_complaint } from '../../app/web/window_input.mjs';
import { Keys } from '../../../website/acceptance/webdriver.mjs';
import { WEBDRIVER_CONTROL, WEBDRIVER_OPTION, key_table } from './page_harness.mjs';

const KEYS = Object.freeze({
	...key_table(['ArrowLeft', 'KeyA', 'Tab']),
	// Cocoa gives the virtual Space a native key code.
	Space: Keys.Space,
});

export const SAFARI_WINDOW_HOOK = `(() => {
	const fixture = { native_ratio: devicePixelRatio, activation: null, exit: null };
	window.labradorWindowFixture = fixture;
	// The fixture edits page-owned globals that the wasm bridge reads in the
	// document's realm. A WebDriver implementation may execute in a sandbox.
	fixture.evaluate = (expression) => {
		if (fixture.exit !== null) throw new Error('Early WebWindow exit: ' + fixture.exit);
		return (0, eval)('(' + expression + ')');
	};
	fixture.set_ratio = (ratio) => Object.defineProperty(window, 'devicePixelRatio', {
		configurable: true, get: () => ratio ?? 1,
	});
	fixture.set_ratio(null);
	let exit;
	Object.defineProperty(window, 'labradorExit', {
		configurable: true, enumerable: true,
		get: () => exit,
		set(value) {
			exit = value;
			Promise.resolve(value).then((settled) => { fixture.exit = JSON.stringify(settled); },
				(error) => { fixture.exit = JSON.stringify({ abort: String(error) }); });
		},
	});
	// Execute a fullscreen probe in the actual trusted input handler. Classic
	// script execution has no per-command user activation option. Capturing
	// both key edges keeps this control input out of the application's log.
	const activate = (event) => {
		const activation = fixture.activation;
		if (!activation || !event.isTrusted || event.code !== 'ArrowLeft') return;
		event.stopImmediatePropagation();
		event.preventDefault();
		if (event.type === 'keydown' && !activation.result) {
			activation.result = (async () => (0, eval)(activation.expression))()
				.then((value) => ({ value }), (error) => ({ error: String(error) }));
		}
	};
	window.addEventListener('keydown', activate, true);
	window.addEventListener('keyup', activate, true);
})();`;

export function safari_window_input(session, evaluate, { option_tab = false } = {}) {
	const pointer = (actions) => session.perform([{ type: 'pointer', id: 'mouse',
		parameters: { pointerType: 'mouse' }, actions }]);
	const move_to = ({ x, y }) => ({ type: 'pointerMove', x, y, origin: 'viewport' });
	const fingers = new Map();
	const touch = async (type, points) => {
		const events = [];
		if (type === 'touchStart' || type === 'touchMove') {
			for (const [x, y, id] of points) {
				if (type === 'touchStart' && fingers.has(id)) continue;
				if (type === 'touchMove' && !fingers.has(id)) throw new Error(`Moving an unknown finger ${id}.`);
				fingers.set(id, [x, y]);
				events.push([type === 'touchStart' ? 'pointerdown' : 'pointermove', x, y, id]);
			}
		} else if (type === 'touchEnd' || type === 'touchCancel') {
			const lifting = points.length > 0 ? points.map(([, , id]) => id) : [...fingers.keys()];
			for (const id of lifting) {
				const last = fingers.get(id);
				if (!last) throw new Error(`Lifting an unknown finger ${id}.`);
				fingers.delete(id);
				events.push([type === 'touchEnd' ? 'pointerup' : 'pointercancel', ...last, id],
					['lostpointercapture', ...last, id]);
			}
		} else {
			throw new Error(`Unknown touch action ${type}.`);
		}
		await evaluate(`(() => {
			for (const [type, clientX, clientY, pointerId] of ${JSON.stringify(events)}) {
				Module.canvas.dispatchEvent(new PointerEvent(type, { pointerType: 'touch',
					pointerId, clientX, clientY, bubbles: true, cancelable: true }));
			}
			return true;
		})()`);
	};
	return {
		presses_modifiers: true,
		option_tab,
		snaps_device_box: false,
		cancels_touch: true,
		synthetic_touch: true,
		touch_slack: 0,
		prepare: async () => {
			const [width, height, ratio] = await evaluate('[innerWidth, innerHeight, devicePixelRatio]');
			if (width <= 660 || height <= 410 || ratio !== 1) {
				throw new Error(`The WebWindow fixture needs a CSS viewport above 660x410 and pixel ratio one; received ${width}x${height} at ${ratio}.`);
			}
		},
		move: (at) => pointer([move_to(at)]),
		press: (at) => pointer([move_to(at), { type: 'pointerDown', button: 0 }]),
		release: (at) => pointer([move_to(at), { type: 'pointerUp', button: 0 }]),
		wheel: ({ x, y }, deltaX, deltaY) => session.perform([{ type: 'wheel', id: 'wheel', actions: [
			{ type: 'scroll', x, y, deltaX, deltaY, origin: 'viewport' },
		] }]),
		key: (name, { control = false } = {}) => {
			if (!Object.hasOwn(KEYS, name)) throw new Error(`Unsupported window key ${name}.`);
			const value = KEYS[name];
			const modifier = control ? WEBDRIVER_CONTROL : name === 'Tab' && option_tab ? WEBDRIVER_OPTION : null;
			const actions = [{ type: 'keyDown', value }, { type: 'keyUp', value }];
			if (modifier !== null) {
				actions.unshift({ type: 'keyDown', value: modifier });
				actions.push({ type: 'keyUp', value: modifier });
			}
			return session.perform([{ type: 'key', id: 'keyboard', actions }]);
		},
		device_pixel_ratio: (ratio) => evaluate(`labradorWindowFixture.set_ratio(${JSON.stringify(ratio)}), true`),
		touch_begin: async () => {},
		touch,
		touch_end: async () => {
			if (fingers.size !== 0) throw new Error('The synthetic touch fixture left fingers down.');
		},
	};
}

export async function drive_safari_window(session, url, evaluate_page, { timeout_ms,
	option_tab, pause, log = console.log, label = 'Safari', drive = drive_window } = {}) {
	await session.navigate(url, timeout_ms);
	const check_page = (expression) => evaluate_page(`(() => {
		const fixture = window.labradorWindowFixture;
		if (!fixture) throw new Error('Missing WebWindow fixture');
		return fixture.evaluate(${JSON.stringify(expression)});
	})()`);
	const input = safari_window_input(session, check_page, { option_tab });
	const evaluate = async (expression, { user_gesture = false } = {}) => {
		if (!user_gesture) return check_page(expression);
		await check_page(`labradorWindowFixture.activation = { expression: ${JSON.stringify(expression)} }, true`);
		try {
			await input.key('ArrowLeft');
			const result = await check_page('labradorWindowFixture.activation.result ?? null');
			if (result === null) throw new Error('The fullscreen probe received no trusted keydown');
			if (Object.hasOwn(result, 'error')) throw new Error(`Fullscreen probe failed: ${result.error}`);
			return result.value;
		} finally {
			await check_page('labradorWindowFixture.activation = null, true');
		}
	};
	const native_ratio = await check_page('labradorWindowFixture.native_ratio');
	log(`${label} WebWindow: native DPR ${native_ratio}; synthetic DPR override and touch pointers.`);
	try {
		await drive({ evaluate, wait: pause }, input);
	} finally {
		await session.release();
	}
	const complaints = await check_page('window.labradorComplaints ?? []');
	const unexpected = complaints.filter((entry) => {
		const separator = entry.indexOf(': ');
		return !expected_complaint({ level: entry.slice(0, separator), text: entry.slice(separator + 2) }) &&
			entry !== 'unhandled rejection: TypeError: page-owned fullscreen refusal';
	});
	if (unexpected.length > 0) throw new Error(`WebWindow complaints: ${unexpected.join('; ')}`);
}
