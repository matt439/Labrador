// page_wait.mjs's bounded wait, against a fixture page and clock.
import assert from 'node:assert/strict';
import test from 'node:test';
import { is_navigation_race } from './devtools.mjs';
import { wait_for } from './page_wait.mjs';

// A clock that only the wait's own pauses move.
function clock() {
	let now = 0;
	const real_now = Date.now;
	Date.now = () => now;
	return {
		wait: async (milliseconds) => { now += milliseconds; },
		restore: () => { Date.now = real_now; },
	};
}

test('a wait answers the first value its condition accepts', async () => {
	const time = clock();
	try {
		const values = [null, { mode: 'other' }, { mode: 'topout' }];
		const asked = [];
		const found = await wait_for(async (expression) => { asked.push(expression); return values.shift(); },
			'Module.result', { done: (value) => value?.mode === 'topout', timeout_ms: 1000, wait: time.wait });
		assert.deepEqual(found, { mode: 'topout' });
		assert.deepEqual(asked, ['Module.result', 'Module.result', 'Module.result']);
	} finally {
		time.restore();
	}
});

test('a wait is bounded in milliseconds, however fast its evaluations are', async () => {
	const time = clock();
	try {
		let asked = 0;
		await assert.rejects(wait_for(async () => { ++asked; return false; }, 'never',
			{ timeout_ms: 1000, interval_ms: 20, wait: time.wait, message: 'the page never settled' }),
		/^Error: the page never settled \(after 1000 ms\)$/);
		assert.equal(asked, 51);
	} finally {
		time.restore();
	}
});

test('a navigation\'s failure is not yet only while navigating, and a check ends the wait', async () => {
	const time = clock();
	try {
		for (const text of ['Execution context was destroyed.', 'Cannot find default execution context',
			'Cannot find context with specified id', 'Cannot find context', 'Inspected target navigated or closed']) {
			assert.equal(is_navigation_race(new Error(text)), true, text);
		}
		const answers = [new Error('Execution context was destroyed.'), true];
		const evaluate = async () => {
			const answer = answers.shift();
			if (answer instanceof Error) throw answer;
			return answer;
		};
		assert.equal(await wait_for(evaluate, 'ready', { timeout_ms: 1000, navigating: true, wait: time.wait }), true);
		await assert.rejects(wait_for(async () => { throw new Error('Execution context was destroyed.'); }, 'ready',
			{ timeout_ms: 1000, wait: time.wait }), /Execution context was destroyed/);
		await assert.rejects(wait_for(async () => { throw new Error('ReferenceError: x'); }, 'ready',
			{ timeout_ms: 1000, navigating: true, wait: time.wait }), /ReferenceError/);
		await assert.rejects(wait_for(async () => false, 'ready', { timeout_ms: 1000, wait: time.wait,
			check: () => { throw new Error('interception failed'); } }), /interception failed/);
		await assert.rejects(wait_for(async () => true, 'ready', { wait: time.wait }), /needs a timeout/);
	} finally {
		time.restore();
	}
});
