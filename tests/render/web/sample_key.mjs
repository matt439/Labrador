// Keep each input level observable by the page's fixed-step loop. A Node
// timer can expire while the renderer delivers no frames, so wall time alone
// does not ensure that a press or release reaches Keyboard::poll().
const INTERVAL = `new Promise((done) => {
	const started = performance.now();
	let frames = 0;
	const step = (timestamp) => {
		if (++frames >= 2 && timestamp - started >= 150) { done(true); }
		else { requestAnimationFrame(step); }
	};
	requestAnimationFrame(step);
})`;

// The development LineSweeper page implements this protocol at its real
// fixed-update boundary. It is absent from packaged modules and other samples.
export const SAMPLE_INPUT_HOOK = `({
	version: 1, started: false, pending: null, ack: null, error: null
})`;

const SAMPLED_CODES = [
	'Escape', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight',
	'Enter', 'Space', 'KeyP', 'KeyR',
];

async function arm_phase(page, code_index, down) {
	return page.evaluate(`(() => {
		const heard = window.labradorRun;
		const input = heard.input;
		if (!input || input.version !== 1) { throw new Error('sample input protocol is unavailable'); }
		if (input.error) { throw new Error(input.error); }
		if (heard.errors?.length) { throw new Error('Module.onError: ' + heard.errors.join(' | ')); }
		if (heard.quit) { return null; }
		if (input.pending && input.ack?.id !== input.pending.id) {
			throw new Error('the preceding sample input phase was not acknowledged');
		}
		const id = (input.pending?.id ?? 0) + 1;
		input.started = true;
		input.pending = { id, code_index: ${code_index}, down: ${down} };
		return id;
	})()`);
}

async function wait_phase(page, id) {
	return page.evaluate(`new Promise((resolve, reject) => {
		const poll = () => {
			const heard = window.labradorRun;
			const input = heard.input;
			if (input.error) { reject(new Error(input.error)); return; }
			if (heard.errors?.length) { reject(new Error('Module.onError: ' + heard.errors.join(' | '))); return; }
			if (heard.quit) { resolve({ quit: true }); return; }
			if (input.ack?.id === ${id}) { resolve({ quit: false, phase: input.ack }); return; }
			requestAnimationFrame(poll);
		};
		poll();
	})`);
}

async function press_sampled_key(page, event) {
	const code_index = SAMPLED_CODES.indexOf(event.code);
	if (code_index < 0) { throw new Error(`sample input does not support ${event.code}`); }
	const phases = [];
	const down_id = await arm_phase(page, code_index, true);
	if (down_id === null) { return { quit: true, phases }; }
	let release_needed = false;
	let failure;
	try {
		await page.send('Input.dispatchKeyEvent', { type: 'rawKeyDown', ...event });
		release_needed = true;
		const held = await wait_phase(page, down_id);
		if (held.phase) { phases.push(held.phase); }
		if (held.quit) { return { quit: true, phases }; }
		const up_id = await arm_phase(page, code_index, false);
		if (up_id === null) { return { quit: true, phases }; }
		// Do not retry an uncertain key-up dispatch in the cleanup path.
		release_needed = false;
		await page.send('Input.dispatchKeyEvent', { type: 'keyUp', ...event });
		const released = await wait_phase(page, up_id);
		if (released.phase) { phases.push(released.phase); }
		return { quit: released.quit, phases };
	} catch (error) {
		failure = error;
		throw error;
	} finally {
		if (release_needed) {
			try {
				await page.send('Input.dispatchKeyEvent', { type: 'keyUp', ...event });
			} catch (cleanup) {
				if (failure) {
					throw new AggregateError([failure, cleanup],
						`${failure.message}; key release also failed: ${cleanup.message}`);
				}
				throw cleanup;
			}
		}
	}
}

export async function press_sample_key(page, event, { sampled = false } = {}) {
	if (sampled) { return press_sampled_key(page, event); }
	await page.send('Input.dispatchKeyEvent', { type: 'rawKeyDown', ...event });
	await page.evaluate(INTERVAL);
	await page.send('Input.dispatchKeyEvent', { type: 'keyUp', ...event });
	await page.evaluate(INTERVAL);
}
