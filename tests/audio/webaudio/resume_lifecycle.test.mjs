// Execute the production EM_JS body with a deferred browser context. This
// isolates promise ordering, not browser playback or an audio endpoint.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import test from 'node:test';

const source = readFileSync(new URL('../../../engine/audio/webaudio/audio_device.cpp', import.meta.url), 'utf8');
const marker = 'EM_JS(void, labrador_audio_install, (), {';
const start = source.indexOf(marker) + marker.length;
assert.ok(start >= marker.length);
const body = source.slice(start, source.indexOf('\n});', start));

function fixture() {
	const listeners = new Map();
	const contexts = [];
	class Context {
		constructor() { this.state = 'suspended'; this.calls = 0; contexts.push(this); }
		resume() {
			++this.calls;
			return new Promise((resolve, reject) => {
				this.resolve = () => { if (this.state !== 'closed') this.state = 'running'; resolve(); };
				this.reject = reject;
			});
		}
		suspend() { this.state = 'suspended'; return Promise.resolve(); }
		close() { this.state = 'closed'; return Promise.resolve(); }
	}
	const Module = {};
	vm.runInNewContext('(function() {' + body + '})()', {
		Module, AudioContext: Context, AudioBuffer: class {},
		navigator: { userActivation: { isActive: true, hasBeenActive: false } },
		window: {
			addEventListener(name, listener) { listeners.set(name, listener); },
			removeEventListener(name) { listeners.delete(name); },
		},
	});
	const api = Module.labradorAudio;
	const id = api.open();
	api.add_bank(id);
	const device = api.devices[id];
	const gesture = listeners.get('keydown');
	gesture({ type: 'keydown', key: 's' });
	return { api, id, device, context: contexts[0], gesture, listeners };
}

const closed_error = () => Object.assign(new Error('Closed before resume completed'), { name: 'InvalidStateError' });

test('concurrent trusted events and explicit resume share one pending request', async () => {
	const f = fixture();
	const pending = f.device.resume_pending;
	f.gesture({ type: 'keydown', key: 'Escape' });
	f.api.resume_device(f.id);
	assert.equal(f.context.calls, 1);
	assert.equal(f.device.resume_pending, pending);
	assert.equal(f.context.state, 'suspended');
	f.context.resolve();
	await pending;
	assert.equal(f.context.state, 'running');
	assert.equal(f.device.resume_pending, null);
	f.api.suspend(f.id);
	f.api.resume_device(f.id);
	assert.equal(f.context.calls, 2);
	f.context.resolve();
	await f.device.resume_pending;
});

test('fast quit consumes only its known close cancellation and stale gestures do nothing', async () => {
	const f = fixture();
	const pending = f.device.resume_pending;
	f.api.close(f.id);
	assert.equal(f.api.devices[f.id], null);
	assert.equal(f.listeners.size, 0);
	f.gesture({ type: 'keydown', key: 's' });
	assert.equal(f.context.calls, 1);
	f.context.reject(closed_error());
	await pending;
	assert.equal(f.device.resume_pending, null);
});

test('late successful resume does not resurrect a closed slot or alter a new device', async () => {
	const f = fixture();
	const pending = f.device.resume_pending;
	f.api.close(f.id);
	const next = f.api.open();
	f.context.resolve();
	await pending;
	assert.equal(f.api.devices[f.id], null);
	assert.equal(f.context.state, 'closed');
	assert.equal(f.api.devices[next].closing, false);
	assert.equal(f.api.devices[next].context, null);
});

test('same error without own close and unrelated errors during close stay rejected', async () => {
	for (const [close, error] of [[false, closed_error()],
		[true, new Error('audio endpoint failed')],
		[true, Object.assign(new Error('another invalid state'), { name: 'InvalidStateError' })]]) {
		const f = fixture();
		const pending = f.device.resume_pending;
		if (close) f.api.close(f.id);
		f.context.reject(error);
		await assert.rejects(pending, (heard) => heard === error);
		assert.equal(f.device.resume_pending, null);
	}
});

test('an attempted close that did not close the context cannot swallow rejection', async () => {
	const f = fixture();
	const pending = f.device.resume_pending;
	f.context.close = () => { throw new Error('close refused'); };
	assert.throws(() => f.api.close(f.id), /close refused/);
	f.context.reject(closed_error());
	await assert.rejects(pending, /Closed before resume completed/);
});
