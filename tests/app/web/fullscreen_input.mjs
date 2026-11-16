import assert from 'node:assert/strict';

// Browser permission denial is an expected Window result. Reject the native
// calls locally so this exercises the actual engine path in either browser,
// independently of a host's permission setting. Page-owned rejection remains
// observable; only that deliberate probe is acknowledged by its test listener.
export async function check_fullscreen_requests(page, input) {
	const { evaluate } = page;
	const frames = () => evaluate(`new Promise((done) => {
		requestAnimationFrame(() => requestAnimationFrame(() => requestAnimationFrame(() => done(true))));
	})`);
	const log = () => evaluate('JSON.stringify(Module.labradorWindowLog.splice(0))').then(JSON.parse);
	const fullscreen = (entries) => entries.filter((entry) => entry.startsWith('fullscreen '));
	const probe = () => evaluate(`JSON.stringify({ calls: labradorFullscreenProbe.calls,
		arguments_ok: labradorFullscreenProbe.arguments_ok,
		unhandled: labradorFullscreenProbe.unhandled,
		page_unhandled: labradorFullscreenProbe.page_unhandled })`).then(JSON.parse);
	const reopen = () => evaluate(`(() => {
		try { Module._web_window_test_reopen(); }
		catch (error) { if (error !== 'unwind') throw error; }
		return true;
	})()`);
	await evaluate(`(() => {
		const canvas = Module.canvas;
		const own = (object, key) => Object.getOwnPropertyDescriptor(object, key);
		const restore = (object, key, descriptor) => descriptor ?
			Object.defineProperty(object, key, descriptor) : delete object[key];
		const standard = own(canvas, 'requestFullscreen');
		const prefixed = own(canvas, 'webkitRequestFullscreen');
		const exit = own(document, 'exitFullscreen');
		const activation = own(navigator, 'userActivation');
		const native_request = canvas.requestFullscreen;
		const native_exit = document.exitFullscreen;
		const state = { calls: 0, arguments_ok: true, unhandled: [], page_unhandled: [] };
		state.install = (mode) => {
			state.calls = 0;
			state.arguments_ok = true;
			const request = function (...args) {
				++state.calls;
				state.arguments_ok &&= mode === 'prefixed' ?
					args.length === 1 && args[0] === Element.ALLOW_KEYBOARD_INPUT : args.length === 0;
				if (mode === 'pending') {
					return new Promise((resolve, reject) => { state.reject = reject; });
				}
				return Promise.reject(new TypeError(mode === 'page' ?
					'page-owned fullscreen refusal' : 'forced engine fullscreen refusal'));
			};
			canvas.requestFullscreen = mode === 'prefixed' ? undefined : request;
			canvas.webkitRequestFullscreen = mode === 'prefixed' ? request : undefined;
		};
		state.inactive = () => Object.defineProperty(navigator, 'userActivation', {
			configurable: true, value: { isActive: false }
		});
		state.restore_activation = () => restore(navigator, 'userActivation', activation);
		state.enter_native = () => native_request.call(canvas);
		state.exit_native = () => native_exit.call(document);
		state.reject_exit = () => {
			document.exitFullscreen = () => Promise.reject(new TypeError('forced engine exit refusal'));
		};
		state.restore_exit = () => restore(document, 'exitFullscreen', exit);
		state.listener = (event) => {
			if (event.reason?.message === 'page-owned fullscreen refusal') {
				state.page_unhandled.push({ message: event.reason.message, prevented: event.defaultPrevented });
				event.preventDefault();
			} else {
				state.unhandled.push(String(event.reason));
			}
		};
		state.restore = () => {
			restore(canvas, 'requestFullscreen', standard);
			restore(canvas, 'webkitRequestFullscreen', prefixed);
			state.restore_exit();
			state.restore_activation();
			window.removeEventListener('unhandledrejection', state.listener);
		};
		window.addEventListener('unhandledrejection', state.listener);
		window.labradorFullscreenProbe = state;
		return true;
	})()`);
	try {
		for (const mode of ['reject', 'prefixed']) {
			await log();
			await evaluate(`labradorFullscreenProbe.install('${mode}'),
				Module._web_window_test_enter_fullscreen(), true`, { user_gesture: true });
			await frames();
			const heard = await log();
			assert.deepEqual(fullscreen(heard), ['fullscreen 0'], `${mode}: refusal status`);
			assert.ok(heard.includes('tick'), `${mode}: the frame continues after refusal`);
			assert.deepEqual(await probe(), { calls: 1, arguments_ok: true, unhandled: [], page_unhandled: [] });
		}
		console.log('ok - standard and prefixed engine fullscreen refusals are handled without ending frames');

		await evaluate(`labradorFullscreenProbe.install('reject'), labradorFullscreenProbe.inactive(),
			Module._web_window_test_enter_fullscreen(), labradorFullscreenProbe.restore_activation(), true`);
		assert.equal((await probe()).calls, 0, 'an inactive request waits for input');
		await input.key('KeyA');
		await frames();
		assert.deepEqual(fullscreen(await log()), ['fullscreen 0'], 'deferred refusal status');
		assert.equal((await probe()).calls, 1, 'the trusted key runs the queued request once');
		console.log('ok - a deferred engine request handles refusal from the next trusted input');

		await evaluate(`labradorFullscreenProbe.install('reject'), labradorFullscreenProbe.inactive(),
			Module._web_window_test_enter_fullscreen(), Module._web_window_test_queue_other_activation_call(),
			Module._web_window_test_leave_fullscreen(),
			labradorFullscreenProbe.restore_activation(), true`);
		await input.key('KeyA');
		await frames();
		assert.equal((await probe()).calls, 0, 'leaving cancels the owned deferred request');
		assert.equal(await evaluate('Module.labradorOtherActivationCalls'), 1, 'leaving retains unrelated queued work');
		assert.deepEqual(fullscreen(await log()), []);

		await evaluate(`labradorFullscreenProbe.inactive(), Module._web_window_test_enter_fullscreen(),
			Module._web_window_test_queue_other_activation_call(),
			Module._web_window_test_close(), labradorFullscreenProbe.restore_activation(), true`);
		await frames();
		await log();
		await reopen();
		await frames();
		await log();
		await input.key('KeyA');
		await frames();
		assert.equal((await probe()).calls, 0, 'closing cancels the old window\'s queued request');
		assert.equal(await evaluate('Module.labradorOtherActivationCalls'), 1, 'closing retains unrelated queued work');
		assert.deepEqual(fullscreen(await log()), []);
		console.log('ok - leaving and close/reopen cancel only the window-owned queued request');

		await evaluate(`labradorFullscreenProbe.install('pending'),
			Module._web_window_test_enter_fullscreen(), true`, { user_gesture: true });
		assert.equal((await probe()).calls, 1);
		await evaluate('Module._web_window_test_close(), true');
		await frames();
		await log();
		await reopen();
		await frames();
		await log();
		await evaluate(`labradorFullscreenProbe.reject(new TypeError('old window refusal')), true`);
		await frames();
		assert.deepEqual(fullscreen(await log()), [], 'late rejection stays with the closed window');
		await evaluate(`labradorFullscreenProbe.install('reject'),
			Module._web_window_test_enter_fullscreen(), true`, { user_gesture: true });
		await frames();
		const replacement = await log();
		assert.deepEqual(fullscreen(replacement), ['fullscreen 0']);
		assert.ok(replacement.includes('tick'), 'the replacement window still runs');
		assert.deepEqual((await probe()).unhandled, []);
		console.log('ok - an old rejected Promise cannot report to a replacement window');

		await evaluate('labradorFullscreenProbe.enter_native().then(() => true)', { user_gesture: true });
		await frames();
		await log();
		await evaluate(`labradorFullscreenProbe.reject_exit(), Module._web_window_test_leave_fullscreen(), true`);
		await frames();
		assert.deepEqual(fullscreen(await log()), ['fullscreen 1'], 'a refused exit reports the actual fullscreen state');
		assert.deepEqual((await probe()).unhandled, []);
		await evaluate('labradorFullscreenProbe.restore_exit(), labradorFullscreenProbe.exit_native().then(() => true)');
		await frames();
		await log();
		console.log('ok - an engine-owned exit refusal is handled and preserves actual status');

		await evaluate(`labradorFullscreenProbe.install('page'), Module.canvas.requestFullscreen(), true`);
		await frames();
		assert.deepEqual((await probe()).page_unhandled,
			[{ message: 'page-owned fullscreen refusal', prevented: false }]);
		assert.deepEqual((await probe()).unhandled, []);
		console.log('ok - page-owned rejected requests still reach unhandledrejection');
	} finally {
		await evaluate('labradorFullscreenProbe.restore(), delete window.labradorFullscreenProbe, true');
	}
}
