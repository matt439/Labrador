// Observer/control fixtures only. These are not browser or playback evidence.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';
import { PROBE, assert_playback, prepare, reference_wave } from './playback_input.mjs';

const bytes = readFileSync(new URL('../../../samples/audio/source/ping.wav', import.meta.url));
const reference = reference_wave(bytes);

function fixture({ legacy = false, connected = true, trusted = true, stopped = false, corrupt = false } = {}) {
	class AudioNode {
		constructor(context) { this.context = context; }
		connect(destination) { return destination; }
	}
	class AudioContext {
		constructor() { this.currentTime = 0; this.state = 'running'; this.sampleRate = 48000; this.destination = new AudioNode(this); }
	}
	class AudioBuffer {
		constructor() { this.numberOfChannels = 1; this.length = 3528; this.sampleRate = 22050; this.samples = Float32Array.from(reference.samples); }
		getChannelData() { return this.samples; }
	}
	class GainNode extends AudioNode { constructor(context) { super(context); this.gain = { value: 0.4 }; } }
	class StereoPannerNode extends AudioNode { constructor(context) { super(context); this.pan = { value: 0 }; } }
	class AudioBufferSourceNode extends AudioNode {
		constructor(context) { super(context); this.listeners = {}; this.playbackRate = { value: 1 }; this.loop = false; this.starts = 0; }
		start() { ++this.starts; return 'native start result'; }
		stop() { return 'native stop result'; }
		addEventListener(name, listener) { this.listeners[name] = listener; }
	}
	const sandbox = vm.createContext({ AudioNode, AudioContext, AudioBuffer, GainNode, StereoPannerNode, AudioBufferSourceNode });
	sandbox.window = sandbox;
	vm.runInContext(PROBE, sandbox);
	const context = new AudioContext();
	const wav = new AudioBuffer();
	const xwb = new AudioBuffer();
	let quit_calls = 0;
	sandbox.Module = { onRuntimeInitialized() {}, onQuit() { ++quit_calls; },
		labradorAudio: { devices: [{ banks: [[{ buffer: xwb }, {}, {}], [{ buffer: wav }]] }] } };
	sandbox.Module.onRuntimeInitialized();
	const source = new AudioBufferSourceNode(context);
	source.buffer = legacy ? xwb : wav;
	const gain = new GainNode(context);
	const panner = new StereoPannerNode(context);
	assert.equal(source.connect(gain), gain);
	gain.connect(panner);
	if (connected) panner.connect(context.destination);
	assert.equal(source.start(), 'native start result');
	assert.equal(source.starts, 1);
	if (stopped) assert.equal(source.stop(), 'native stop result');
	context.currentTime = 0.2;
	source.listeners.ended({ isTrusted: trusted });
	if (corrupt) wav.samples[7] = 1;
	return { report: sandbox.labradorAudioPlayback.report(reference.samples), quit() {
		sandbox.Module.onQuit();
		assert.equal(quit_calls, 1);
		assert.equal(sandbox.labradorAudioPlayback.quit, true);
	} };
}

test('prepare observes before navigation and enables genuine key focus without generating activation', async () => {
	const calls = [];
	await prepare({ send: async (...arguments_) => calls.push(arguments_) });
	assert.deepEqual(calls, [['Page.addScriptToEvaluateOnNewDocument', { source: PROBE }],
		['Emulation.setFocusEmulationEnabled', { enabled: true }]]);
});

test('the observer forwards native calls, identifies the WAV buffer and preserves real quit', () => {
	const f = fixture();
	assert_playback(f.report, reference);
	assert.equal(f.report.wav_bank, 1);
	assert.equal(f.report.samples_equal, true);
	assert.equal(f.report.context_elapsed, 0.2);
	f.quit();
});

test('equal PCM from the XWB bank, disconnected output, forced/synthetic ends and altered samples cannot pass', () => {
	for (const options of [{ legacy: true }, { connected: false }, { stopped: true }, { trusted: false }, { corrupt: true }]) {
		assert.throws(() => assert_playback(fixture(options).report, reference), /did not meet the pipeline contract/);
	}
	const good = fixture().report;
	for (const changed of [{ context_state: 'suspended' }, { gain: 0 }, { context_elapsed: 0 }, { wav_source_count: 2 }]) {
		assert.throws(() => assert_playback({ ...good, ...changed }, reference), /did not meet the pipeline contract/);
	}
	assert.throws(() => assert_playback({ ...good, errors: ['audio failed'] }, reference), /Module.onError/);
});

test('the reference pins the checked mono PCM16 ping and rejects truncated or altered formats', () => {
	assert.equal(reference.frames, 3528);
	assert.equal(reference.samples.length, 3528);
	assert.throws(() => reference_wave(bytes.subarray(0, bytes.length - 1)), /truncated/);
	const stereo = Buffer.from(bytes);
	stereo.writeUInt16LE(2, 22);
	assert.throws(() => reference_wave(stereo), /mono PCM16/);
});
