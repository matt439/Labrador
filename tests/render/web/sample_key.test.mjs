import assert from 'node:assert/strict';
import test from 'node:test';
import { runInNewContext } from 'node:vm';
import { press_sample_key, SAMPLE_INPUT_HOOK } from './sample_key.mjs';

const settled = () => new Promise((done) => setImmediate(done));
const escape = { code: 'Escape', key: 'Escape', windowsVirtualKeyCode: 27, nativeVirtualKeyCode: 27 };

class PageFixture {
	constructor() {
		this.time = 0;
		this.frames = [];
		this.events = [];
		this.sampled_levels = [];
		this.down = false;
	}

	async send(method, event) {
		assert.equal(method, 'Input.dispatchKeyEvent');
		this.events.push(event);
		this.down = event.type === 'rawKeyDown';
	}

	evaluate(expression) {
		return runInNewContext(expression, {
			performance: { now: () => this.time },
			requestAnimationFrame: (callback) => this.frames.push(callback),
		});
	}

	async frame(time) {
		this.time = time;
		// The fixed-step consumer samples the live input level on a page frame.
		this.sampled_levels.push(this.down);
		for (const callback of this.frames.splice(0)) { callback(time); }
		await settled();
	}
}

test('a stalled page must sample the held and released levels before the next input phase', async () => {
	const page = new PageFixture();
	let finished = false;
	const pressed = press_sample_key(page, escape).then(() => { finished = true; });
	await settled();
	assert.deepEqual(page.events, [{ type: 'rawKeyDown', ...escape }]);
	// Host time can pass without a renderer callback. A host timer would have
	// released the key here, leaving no press for the next engine poll.
	page.time = 1000;
	await settled();
	assert.equal(page.events.length, 1);
	await page.frame(1000);
	assert.equal(page.events.length, 1);
	await page.frame(1017);
	assert.deepEqual(page.events, [{ type: 'rawKeyDown', ...escape }, { type: 'keyUp', ...escape }]);
	page.time = 3000;
	await settled();
	assert.equal(finished, false);
	await page.frame(3000);
	assert.equal(finished, false);
	await page.frame(3017);
	await pressed;
	assert.deepEqual(page.sampled_levels, [true, true, false, false]);
});

test('fast frames preserve the existing 150 ms minimum for both input levels', async () => {
	const page = new PageFixture();
	let finished = false;
	const pressed = press_sample_key(page, escape).then(() => { finished = true; });
	await settled();
	for (const time of [1, 2, 50, 149]) { await page.frame(time); }
	assert.equal(page.events.length, 1);
	await page.frame(150);
	assert.equal(page.events.length, 2);
	for (const time of [151, 152, 299]) { await page.frame(time); }
	assert.equal(finished, false);
	await page.frame(300);
	await pressed;
});

test('dispatch and page failures propagate without advancing the key sequence', async () => {
	const page = new PageFixture();
	page.send = async () => { throw new Error('dispatch refused'); };
	await assert.rejects(press_sample_key(page, escape), /dispatch refused/);
	assert.equal(page.frames.length, 0);
	const evaluation = new PageFixture();
	evaluation.evaluate = async () => { throw new Error('the page crashed'); };
	await assert.rejects(press_sample_key(evaluation, escape), /the page crashed/);
	assert.deepEqual(evaluation.events, [{ type: 'rawKeyDown', ...escape }]);
});

// A protocol producer standing in for the development state's fixed updates.
// Replies can be held while further rendering/catch-up work continues, which
// is the case a minimum-duration rAF wait cannot bound.
class SampledPageFixture {
	constructor() {
		this.heard = { quit: false, input: runInNewContext(SAMPLE_INPUT_HOOK) };
		this.frames = [];
		this.replies = [];
		this.events = [];
		this.down = false;
		this.previous = false;
		this.updates = [];
		this.row = 0;
		this.hold_time = 0;
		this.delay_replies = false;
	}

	async send(method, event) {
		assert.equal(method, 'Input.dispatchKeyEvent');
		if (!this.heard.quit && !this.heard.input.error) {
			assert.equal(this.heard.input.pending.down, event.type === 'rawKeyDown',
				'the phase must be armed before its genuine key event');
		}
		this.events.push(event);
		this.down = event.type === 'rawKeyDown';
	}

	evaluate(expression) {
		const result = runInNewContext(expression, {
			window: { labradorRun: this.heard },
			requestAnimationFrame: (callback) => this.frames.push(callback),
		});
		if (this.delay_replies && expression.startsWith('new Promise')) {
			return Promise.resolve(result).then((value) =>
				new Promise((resolve) => this.replies.push(() => resolve(value))));
		}
		return result;
	}

	async frame(ticks = 1) {
		for (let tick = 0; tick < ticks; ++tick) {
			const pressed = this.down && !this.previous;
			this.previous = this.down;
			const input = this.heard.input;
			const command = input.pending;
			if (!command || input.ack?.id === command.id || this.down !== command.down) { continue; }
			if (command.down && !pressed) { input.error = 'down had no sampled edge'; continue; }
			this.updates.push(command.down);
			// A menu which repeats after 0.4s. Forty catch-up updates would
			// cross it if an acknowledged phase left UI updates running.
			if (command.down) {
				if (this.hold_time === 0 || this.hold_time >= 0.4) { ++this.row; }
				this.hold_time += 1 / 60;
			} else { this.hold_time = 0; }
			input.ack = { id: command.id, state: 'pause', row: this.row, palette: 0, result: '' };
		}
		for (const callback of this.frames.splice(0)) { callback(); }
		await settled();
	}

	async reply() {
		assert.equal(this.replies.length, 1);
		this.replies.shift()();
		await settled();
	}
}

test('sampled phases survive delayed replies and catch-up without repeating a menu move', async () => {
	const page = new SampledPageFixture();
	page.delay_replies = true;
	let finished = false;
	const pressed = press_sample_key(page, { ...escape, code: 'ArrowDown' }, { sampled: true })
		.then((result) => { finished = true; return result; });
	await settled();
	assert.equal(page.events.length, 1);
	await page.frame(40);
	await page.frame(40);
	assert.equal(page.row, 1);
	assert.deepEqual(page.updates, [true]);
	assert.equal(page.events.length, 1);
	await page.reply();
	assert.equal(page.events.length, 2);
	assert.equal(finished, false);
	await page.frame(40);
	await page.frame(40);
	assert.deepEqual(page.updates, [true, false]);
	assert.equal(finished, false);
	await page.reply();
	const result = await pressed;
	assert.equal(result.quit, false);
	assert.deepEqual(result.phases.map((phase) => phase.id), [1, 2]);
	assert.equal(page.hold_time, 0);
	assert.equal(page.row, 1);
});

test('the next sampled press waits for a neutral update before receiving a fresh edge', async () => {
	const page = new SampledPageFixture();
	for (let count = 0; count < 2; ++count) {
		const pressed = press_sample_key(page, escape, { sampled: true });
		await settled();
		await page.frame(30);
		await page.frame(30);
		await pressed;
	}
	assert.deepEqual(page.updates, [true, false, true, false]);
	assert.equal(page.heard.input.pending.id, 4);
});

test('genuine onQuit is terminal and still releases the physical key', async () => {
	const page = new SampledPageFixture();
	const pressed = press_sample_key(page, escape, { sampled: true });
	await settled();
	page.heard.quit = true;
	await page.frame();
	const result = await pressed;
	assert.equal(result.quit, true);
	assert.deepEqual(page.events.map((event) => event.type), ['rawKeyDown', 'keyUp']);
	assert.equal(page.heard.input.pending.id, 1);
});

test('sample input errors propagate and release the key without arming another phase', async () => {
	const page = new SampledPageFixture();
	const pressed = press_sample_key(page, escape, { sampled: true });
	const rejected = assert.rejects(pressed, /stale request/);
	await settled();
	page.heard.input.error = 'stale request';
	await page.frame();
	await rejected;
	assert.deepEqual(page.events.map((event) => event.type), ['rawKeyDown', 'keyUp']);
	assert.equal(page.heard.input.pending.id, 1);
});

test('unsupported or missing sampled protocols fail before dispatch', async () => {
	const page = new SampledPageFixture();
	await assert.rejects(press_sample_key(page, { code: 'KeyZ' }, { sampled: true }), /does not support/);
	page.heard.input = undefined;
	await assert.rejects(press_sample_key(page, escape, { sampled: true }), /protocol is unavailable/);
	assert.equal(page.events.length, 0);
});

test('an unacknowledged request cannot be overwritten by the next press', async () => {
	const page = new SampledPageFixture();
	page.heard.input.pending = { id: 1, code_index: 0, down: true };
	await assert.rejects(press_sample_key(page, escape, { sampled: true }), /not acknowledged/);
	assert.equal(page.heard.input.pending.id, 1);
	assert.equal(page.events.length, 0);
});

test('quit cannot hide a protocol error or Module.onError', async () => {
	for (const kind of ['protocol', 'module']) {
		const page = new SampledPageFixture();
		const pressed = press_sample_key(page, escape, { sampled: true });
		const rejected = assert.rejects(pressed, /failure before quit/);
		await settled();
		page.heard.quit = true;
		if (kind === 'protocol') { page.heard.input.error = 'failure before quit'; }
		else { page.heard.errors = ['failure before quit']; }
		await page.frame();
		await rejected;
		assert.equal(page.events.length, 2);
	}
});

test('a failed cleanup preserves the primary protocol failure without retrying dispatch', async () => {
	const page = new SampledPageFixture();
	const send = page.send.bind(page);
	page.send = async (method, event) => {
		await send(method, event);
		if (event.type === 'keyUp') { throw new Error('cleanup refused'); }
	};
	const pressed = press_sample_key(page, escape, { sampled: true });
	const rejected = assert.rejects(pressed, (error) => {
		assert.ok(error instanceof AggregateError);
		assert.match(error.errors[0].message, /primary failure/);
		assert.match(error.errors[1].message, /cleanup refused/);
		return true;
	});
	await settled();
	page.heard.input.error = 'primary failure';
	await page.frame();
	await rejected;
	assert.deepEqual(page.events.map((event) => event.type), ['rawKeyDown', 'keyUp']);
});
