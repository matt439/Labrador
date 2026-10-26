import assert from 'node:assert/strict';
import test from 'node:test';
import { runInNewContext } from 'node:vm';
import { press_sample_key } from './sample_key.mjs';

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
