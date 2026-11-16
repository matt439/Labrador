// Real-browser AudioSample playback qualification through run_page --input.
// No fake audio implementation: these hooks forward the native node methods.
// tests/audio/webaudio/README.md records the command and evidence boundaries.
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { module_hook } from '../../render/web/page_harness.mjs';
import { press_sample_key } from '../../render/web/sample_key.mjs';

export const strict_complaints = true;
export const ready = 'window.labradorAudioPlayback?.initialized === true';

export function reference_wave(bytes) {
	if (bytes.length < 44 || bytes.toString('ascii', 0, 4) !== 'RIFF' || bytes.toString('ascii', 8, 12) !== 'WAVE') {
		throw new Error('playback reference is not the checked PCM WAV');
	}
	let format;
	let data;
	for (let at = 12; at < bytes.length;) {
		if (at + 8 > bytes.length) throw new Error('truncated playback reference chunk');
		const size = bytes.readUInt32LE(at + 4);
		const end = at + 8 + size;
		if (end > bytes.length) throw new Error('truncated playback reference payload');
		const name = bytes.toString('ascii', at, at + 4);
		if (name === 'fmt ') format = bytes.subarray(at + 8, end);
		if (name === 'data') data = bytes.subarray(at + 8, end);
		at = end + (size & 1);
	}
	if (!format || format.length < 16 || !data || data.length % 2 || format.readUInt16LE(0) !== 1 ||
		format.readUInt16LE(2) !== 1 || format.readUInt32LE(4) !== 22050 || format.readUInt16LE(14) !== 16) {
		throw new Error('playback reference must be mono PCM16 at 22050 Hz');
	}
	if (data.length !== 3528 * 2) throw new Error('playback reference must contain 3528 ping frames');
	return { channels: 1, sample_rate: 22050, frames: 3528,
		pcm_sha256: createHash('sha256').update(data).digest('hex'),
		samples: Array.from({ length: 3528 }, (_, index) => data.readInt16LE(index * 2) / 32768) };
}

export const PROBE = `(() => {
	const heard = { initialized: false, quit: false, errors: [], has_quit: false };
	window.labradorAudioPlayback = heard;
	${module_hook('labradorAudioPlaybackHooked')}
	const NativeBuffer = AudioBuffer;
	const NativeContext = AudioContext;
	const NativeGain = GainNode;
	const NativePanner = StereoPannerNode;
	const edges = [];
	const sources = [];
	const by_source = new Map();
	const connect = AudioNode.prototype.connect;
	AudioNode.prototype.connect = function (...arguments_) {
		const result = connect.apply(this, arguments_);
		edges.push({ from: this, to: arguments_[0] });
		return result;
	};
	const start = AudioBufferSourceNode.prototype.start;
	AudioBufferSourceNode.prototype.start = function (...arguments_) {
		const started = this.context.currentTime;
		const result = start.apply(this, arguments_);
		const gain = edges.find((edge) => edge.from === this)?.to;
		const panner = edges.find((edge) => edge.from === gain)?.to;
		const destination = edges.find((edge) => edge.from === panner)?.to;
		const record = { source: this, context: this.context, buffer: this.buffer,
			native: this.buffer instanceof NativeBuffer && this.context instanceof NativeContext,
			connected: gain instanceof NativeGain && panner instanceof NativePanner && destination === this.context.destination,
			gain: gain?.gain?.value, pan: panner?.pan?.value, rate: this.playbackRate.value,
			loop: this.loop, started, ended: null, ended_trusted: false, stops: 0,
			complete_wave: arguments_.length < 3 && (arguments_[1] ?? 0) === 0 };
		sources.push(record);
		by_source.set(this, record);
		this.addEventListener('ended', (event) => {
			record.ended = this.context.currentTime;
			record.ended_trusted = event.isTrusted;
		}, { once: true });
		return result;
	};
	const stop = AudioBufferSourceNode.prototype.stop;
	AudioBufferSourceNode.prototype.stop = function (...arguments_) {
		const result = stop.apply(this, arguments_);
		const record = by_source.get(this);
		if (record) ++record.stops;
		return result;
	};
	heard.report = (expected) => {
		const devices = (module?.labradorAudio?.devices ?? []).filter(Boolean);
		const candidates = [];
		for (const device of devices) {
			if (!device.banks.some((bank) => bank.length === 3)) continue;
			for (const [index, bank] of device.banks.entries()) {
				if (bank.length === 1 && bank[0].buffer instanceof NativeBuffer) {
					candidates.push({ index, buffer: bank[0].buffer });
				}
			}
		}
		const candidate = candidates.length === 1 ? candidates[0] : null;
		const matches = candidate ? sources.filter((record) => record.buffer === candidate.buffer) : [];
		const record = matches.length === 1 ? matches[0] : null;
		const buffer = candidate?.buffer;
		const channel = buffer?.numberOfChannels === 1 ? buffer.getChannelData(0) : null;
		return { errors: heard.errors, has_quit: heard.has_quit, quit: heard.quit,
			candidate_count: candidates.length, source_count: sources.length, wav_source_count: matches.length,
			wav_bank: candidate?.index, channels: buffer?.numberOfChannels, sample_rate: buffer?.sampleRate, frames: buffer?.length,
			samples_equal: !!expected && !!channel && channel.length === expected.length && channel.every((value, index) => value === expected[index]),
			native: record?.native, destination_connected: record?.connected, gain: record?.gain, pan: record?.pan,
			playback_rate: record?.rate, loop: record?.loop, stop_calls: record?.stops,
			context_state: record?.context.state, context_sample_rate: record?.context.sampleRate,
			ended: record?.ended !== null && record?.ended !== undefined,
			ended_trusted: record?.ended_trusted, complete_wave: record?.complete_wave,
			context_elapsed: record?.ended === null ? null : record?.ended - record?.started };
	};
})();`;

export async function prepare(page) {
	await page.send('Page.addScriptToEvaluateOnNewDocument', { source: PROBE });
	await page.send('Emulation.setFocusEmulationEnabled', { enabled: true });
}

export function assert_playback(report, reference) {
	if (report.errors.length) throw new Error('Module.onError: ' + report.errors.join('; '));
	const expected = report.candidate_count === 1 && report.source_count === 1 && report.wav_source_count === 1 &&
		report.channels === reference.channels && report.sample_rate === reference.sample_rate && report.frames === reference.frames &&
		report.samples_equal && report.native && report.destination_connected && !report.loop && report.stop_calls === 0 &&
		report.context_state === 'running' && report.ended && report.ended_trusted && report.complete_wave && report.context_elapsed > 0 &&
		Math.abs(report.gain - 0.4) < 0.000001 && Math.abs(report.pan) < 0.000001 && Math.abs(report.playback_rate - 1) < 0.000001;
	if (!expected) throw new Error('WAV playback evidence did not meet the pipeline contract: ' + JSON.stringify(report));
}

export default async function drive(page) {
	const reference = reference_wave(readFileSync(new URL('../../../samples/audio/source/ping.wav', import.meta.url)));
	const deadline = Date.now() + 15000;
	const report = () => page.evaluate(`window.labradorAudioPlayback.report(${JSON.stringify(reference.samples)})`);
	const until = async (test, description) => {
		for (;;) {
			const current = await report();
			if (current.errors.length) throw new Error('Module.onError: ' + current.errors.join('; '));
			if (test(current)) return current;
			if (Date.now() >= deadline) throw new Error(description + ': ' + JSON.stringify(current));
			await page.wait(25);
		}
	};
	const loaded = await until((value) => value.candidate_count === 1, 'the WAV and XWB banks did not load');
	if (!loaded.has_quit || loaded.quit || loaded.source_count !== 0) throw new Error('the sample was not idle and quit-capable');
	await page.evaluate('Module.canvas.focus()');
	if (!(await page.evaluate('document.hasFocus() && document.activeElement === Module.canvas'))) throw new Error('audio canvas did not take the keyboard');
	await press_sample_key(page, { code: 'KeyA', key: 'a', windowsVirtualKeyCode: 65, nativeVirtualKeyCode: 65 });
	const played = await until((value) => value.ended && value.context_state === 'running', 'the WAV source did not naturally finish');
	assert_playback(played, reference);
	await press_sample_key(page, { code: 'KeyS', key: 's', windowsVirtualKeyCode: 83, nativeVirtualKeyCode: 83 });
	const stopped = await report();
	if (stopped.quit || stopped.source_count !== 1) throw new Error('S unexpectedly quit or started another source');
	await press_sample_key(page, { code: 'Escape', key: 'Escape', windowsVirtualKeyCode: 27, nativeVirtualKeyCode: 27 });
	await until((value) => value.quit, 'the real Module.onQuit callback did not arrive');
	const { samples, ...identity } = reference;
	console.log('audio playback evidence: ' + JSON.stringify({
		scope: 'Web Audio backend/pipeline only; no listening or physical-speaker acceptance',
		asset: 'ping_wav', reference: identity, ...played, quit: true,
	}));
}
