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


test('resume cancellation waits for asynchronous own close before accepting it', async () => {
	const f = fixture();
	let finish_close;
	f.context.close = () => new Promise((resolve) => {
		finish_close = () => { f.context.state = 'closed'; resolve(); };
	});
	const pending = f.device.resume_pending;
	f.api.close(f.id);
	f.context.reject(closed_error());
	let settled = false;
	pending.then(() => { settled = true; });
	await Promise.resolve();
	await Promise.resolve();
	assert.equal(settled, false);
	assert.equal(f.context.state, 'suspended');
	finish_close();
	await pending;
	assert.equal(settled, true);
});

test('asynchronous close failure preserves both errors instead of accepting cancellation', async () => {
	const f = fixture();
	let reject_close;
	f.context.close = () => new Promise((resolve, reject) => { reject_close = reject; });
	const pending = f.device.resume_pending;
	f.api.close(f.id);
	const resume_error = closed_error();
	f.context.reject(resume_error);
	await Promise.resolve();
	const close_error = new Error('close graph failed');
	reject_close(close_error);
	await assert.rejects(pending, (error) =>
		error.errors[0] === resume_error && error.errors[1] === close_error);
});

test('close resolution without a closed context still rejects resume cancellation', async () => {
	const f = fixture();
	f.context.close = () => Promise.resolve();
	const pending = f.device.resume_pending;
	f.api.close(f.id);
	f.context.reject(closed_error());
	await assert.rejects(pending, /Closed before resume completed/);
});

// The decoded-bank functions from that same production body, with actual
// owned Float32Array channel storage and a small stand-in for the wasm heap.
function bank_fixture({ heap = new Int16Array(16), refuse_buffer = false, refuse_copy = false } = {}) {
	const sources = [];
	const gains = [];
	const panners = [];
	const listeners = new Map();
	class Node {
		connect() {}
		disconnect() {}
	}
	class Buffer {
		constructor(options) {
			if (refuse_buffer) throw new RangeError('browser buffer limit');
			this.channels = Array.from({ length: options.numberOfChannels }, () => new Float32Array(options.length));
		}
		getChannelData(channel) {
			if (refuse_copy) throw new Error('channel copy refused');
			return this.channels[channel];
		}
	}
	class Context {
		constructor() { this.state = 'running'; this.currentTime = 0; this.destination = new Node(); }
		suspend() { this.state = 'suspended'; return Promise.resolve(); }
		resume() { this.state = 'running'; return Promise.resolve(); }
		close() { this.state = 'closed'; return Promise.resolve(); }
	}
	class Source extends Node {
		constructor(context, options) { super(); Object.assign(this, options); sources.push(this); }
		start(when, offset = 0) { this.offset = offset; }
		stop() { this.stopped = true; }
	}
	class Gain extends Node {
		constructor(context, options) { super(); this.gain = { value: options.gain }; gains.push(this); }
	}
	class Panner extends Node {
		constructor(context, options) { super(); this.pan = { value: options.pan }; panners.push(this); }
	}
	const Module = {};
	vm.runInNewContext('(function() {' + body + '})()', {
		Module, HEAP16: heap, AudioContext: Context, AudioBuffer: Buffer,
		AudioBufferSourceNode: Source, GainNode: Gain, StereoPannerNode: Panner,
		window: {
			addEventListener(name, listener) { listeners.set(name, listener); },
			removeEventListener(name) { listeners.delete(name); },
		},
	});
	const api = Module.labradorAudio;
	const id = api.open();
	return { api, id, heap, sources, gains, panners, listeners };
}

test('decoded PCM is copied before caller storage changes and reaches nodes in order', () => {
	const heap = new Int16Array([-32768, 32767, -16384, 16384, 0, 8192, 16384, -8192]);
	const f = bank_fixture({ heap });
	const bank = f.api.add_bank(f.id);
	assert.equal(f.api.add_wave(f.id, bank, 2, 8000, 4, 0, 1, 2), 1);
	const voice = f.api.create_voice(f.id, bank, 0);
	heap.fill(0);
	f.api.play(f.id, voice, 1);
	assert.equal(f.sources.length, 0);
	f.listeners.get('keydown')({ type: 'keydown', key: 'a' });
	assert.equal(f.sources.length, 1);
	assert.deepEqual(Array.from(f.sources[0].buffer.channels[0]), [-1, -0.5, 0, 0.5]);
	assert.deepEqual(Array.from(f.sources[0].buffer.channels[1]), [32767 / 32768, 0.5, 0.25, -0.25]);
	assert.equal(f.sources[0].loopStart, 1 / 8000);
	assert.equal(f.sources[0].loopEnd, 3 / 8000);
	f.api.play_wave(f.id, bank, 0, 0.25, 2, -0.5);
	assert.equal(f.sources.length, 2);
	assert.equal(f.gains[1].gain.value, 0.25);
	assert.equal(f.panners[1].pan.value, -0.5);
	assert.equal(f.sources[1].playbackRate, 2);
	assert.equal(f.sources[1].loop, false);
	f.api.close(f.id);
});

test('browser buffer and channel-copy refusals roll back the pending bank with their reason', () => {
	for (const [options, reason] of [[{ refuse_buffer: true }, 'browser buffer limit'],
		[{ refuse_copy: true }, 'channel copy refused']]) {
		const f = bank_fixture(options);
		const bank = f.api.add_bank(f.id);
		assert.equal(f.api.add_wave(f.id, bank, 1, 8000, 2, 0, 0, 0), 0);
		assert.equal(f.api.devices[f.id].banks.length, 0);
		assert.equal(f.api.devices[f.id].bank_error, reason);
		assert.equal(f.api.add_bank(f.id), bank);
		f.api.close(f.id);
	}
});

test('publication failure preserves existing banks and the next bank index', () => {
	const f = bank_fixture();
	const kept = f.api.add_bank(f.id);
	assert.equal(f.api.add_wave(f.id, kept, 1, 8000, 2, 0, 0, 0), 1);
	const rejected = f.api.add_bank(f.id);
	f.api.devices[f.id].banks[rejected].push = () => { throw new Error('publication refused'); };
	assert.equal(f.api.add_wave(f.id, rejected, 1, 8000, 2, 0, 0, 0), 0);
	assert.equal(f.api.devices[f.id].banks.length, 1);
	assert.equal(f.api.devices[f.id].banks[kept].length, 1);
	assert.equal(f.api.devices[f.id].bank_error, 'publication refused');
	assert.equal(f.api.add_bank(f.id), rejected);
	f.api.close(f.id);
});

test('PCM byte pointers keep their unsigned wasm32 address without a multi-gigabyte allocation', () => {
	const f = bank_fixture({ heap: { 1073741824: -32768, 1073741825: 16384 } });
	const bank = f.api.add_bank(f.id);
	assert.equal(f.api.add_wave(f.id, bank, 1, 8000, 2, -2147483648, 999, 0), 1);
	const wave = f.api.devices[f.id].banks[bank][0];
	assert.deepEqual(Array.from(wave.buffer.channels[0]), [-1, 0.5]);
	const voice = f.api.create_voice(f.id, bank, 0);
	f.api.play(f.id, voice, 1);
	f.listeners.get('keydown')({ type: 'keydown', key: 'a' });
	assert.equal(f.sources[0].loopStart, 0);
	assert.equal(f.sources[0].loopEnd, 0);
	f.api.close(f.id);
});
