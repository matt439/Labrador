// Development-page pre-js only. The packaged module does not load this file.
// There is no timer/pacer or per-frame console/DOM write: a separate rAF
// callback supplies Application's already completed CPU-frame snapshot.
(function () {
	const query = new URLSearchParams(location.search);
	const modeName = query.get('pacing');
	if (modeName === null) return;
	const modes = { play: 1, topout: 2, 'topout-frozen': 3 };
	if (!Object.hasOwn(modes, modeName)) throw new Error('pacing must be play, topout or topout-frozen.');
	const fixture = modeName !== 'play';
	const duration = Number(query.get('pacing_duration_ms') ?? (fixture ? 3000 : 120000));
	if (!Number.isFinite(duration) || duration < 100 || duration > 600000) {
		throw new Error('pacing_duration_ms must be between 100 and 600000.');
	}
	const canvas = Module['canvas'];
	if (!canvas) throw new Error('The pacing development page requires its canvas.');
	const capacity = fixture ? 8192 : 65536;
	const width = 15;
	const rows = new Float64Array(capacity * width);
	const observations = new Float64Array(capacity * 4);
	const events = [];
	const gaps = [];
	const identity = {
		id: query.get('pacing_id') ?? 'unassigned',
		device: 'unrecorded', display: 'unrecorded', refresh_hz: null,
		refresh_source: 'unrecorded', browser: navigator.userAgent,
		page_url: location.href, source_commit: 'unknown', source_dirty: null,
		build: 'unknown', emscripten: 'unrecorded',
		render_backend: null, render_api: null, render_device_name: null,
		workload: modeName === 'play' ? 'played_match' : fixtureId(),
		fixture_mode: modeName, touch_controls: fixture ? false : 'follow ordinary play',
		presentation_profile: fixture ? 'benchmark_board_fixed_hints_no_touch_v1' : 'ordinary_play',
		viewport_css: null, dpr: null, canvas_css: null, drawing_buffer: null,
		instrument: 'sm08-engine-v1',
		buffer_capacity_bytes: rows.byteLength + observations.byteLength,
	};
	let count = 0;
	let observed = 0;
	let lastFrame = 0;
	let duplicates = 0;
	let segment = 0;
	let started = null;
	let measured = null;
	let done = false;
	let buildStamped = false;
	let geometryDirty = true;
	let geometry = null;
	const resizeObserver = typeof ResizeObserver === 'function' ?
		new ResizeObserver(() => { geometryDirty = true; }) : null;
	const targets = [[document, 'visibilitychange'], [window, 'resize'],
		[canvas, 'webglcontextlost'], [canvas, 'webglcontextrestored']];
	function fixtureId() {
		return modeName === 'topout-frozen' ? 'web_full_well_top_out_frozen_v1' :
			'web_full_well_top_out_first_burst_v1';
	}
	function eventOccurred(event) {
		++segment;
		geometryDirty = true;
		events.push({ event: event.type, timestamp_ms: performance.now(),
			visibility: document.visibilityState });
	}
	function updateGeometry(timestamp, frame) {
		if (!geometryDirty && geometry && canvas.width === geometry.drawing_buffer[0] &&
			canvas.height === geometry.drawing_buffer[1] && devicePixelRatio === geometry.dpr &&
			innerWidth === geometry.viewport_css[0] && innerHeight === geometry.viewport_css[1]) return;
		const box = canvas.getBoundingClientRect();
		geometryDirty = false;
		if (geometry && canvas.width === geometry.drawing_buffer[0] &&
			canvas.height === geometry.drawing_buffer[1] && devicePixelRatio === geometry.dpr &&
			innerWidth === geometry.viewport_css[0] && innerHeight === geometry.viewport_css[1] &&
			box.width === geometry.canvas_css[0] && box.height === geometry.canvas_css[1]) return;
		if (geometry) ++segment;
		geometry = { event: 'geometry', timestamp_ms: timestamp, frame, segment,
			viewport_css: [innerWidth, innerHeight], dpr: devicePixelRatio,
			canvas_css: [box.width, box.height], drawing_buffer: [canvas.width, canvas.height],
			buffer_orientation: canvas.height > canvas.width ? 'portrait' : 'landscape' };
		if (!identity.initial_geometry) identity.initial_geometry = geometry;
		for (const key of ['viewport_css', 'dpr', 'canvas_css', 'drawing_buffer', 'buffer_orientation']) identity[key] = geometry[key];
		events.push(geometry);
	}
	function materialize(index) {
		const base = index * width;
		return {
			observer_timestamp_ms: rows[base], frame: rows[base + 1], updates: rows[base + 2],
			interval_ns: rows[base + 3], update_ns: rows[base + 4], begin_frame_ns: rows[base + 5],
			draw_ns: rows[base + 6], submit_ns: rows[base + 7], end_frame_ns: rows[base + 8],
			top_out_frame: rows[base + 9], live_particles: fixture ? rows[base + 10] : null,
			dropped_particles: fixture ? rows[base + 11] : null,
			segment: rows[base + 12], visible: !!rows[base + 13],
			phase: rows[base + 14] ? 'measure' : 'warmup',
		};
	}
	function finish(status) {
		if (done) return;
		done = true;
		for (const [target, event] of targets) target.removeEventListener(event, eventOccurred);
		resizeObserver?.disconnect();
		const samples = Array.from({ length: count }, (_, index) => materialize(index));
		const marked = samples.find((row) => row.top_out_frame !== 0 && row.frame === row.top_out_frame);
		let topOut = null;
		if (marked) {
			const next = samples.find((row) => row.frame === marked.frame + 1);
			topOut = {
				...marked,
				geometry: events.filter((event) => event.event === 'geometry' && event.frame <= marked.frame).at(-1) ?? null,
				counts_match: marked.live_particles === 9600 && marked.dropped_particles === 0,
				draw_submit_ns: marked.draw_ns + marked.submit_ns,
				cpu_total_ns: marked.update_ns + marked.begin_frame_ns + marked.draw_ns +
					marked.submit_ns + marked.end_frame_ns,
				next_interval_ns: next?.interval_ns ?? null,
			};
		}
		if (status === 'complete' && fixture && !topOut?.counts_match) status = 'incomplete_fixture';
		const rafSamples = Array.from({ length: observed }, (_, frame) => ({
			frame, timestamp_ms: observations[frame * 4], segment: observations[frame * 4 + 1],
			visible: !!observations[frame * 4 + 2],
			phase: observations[frame * 4 + 3] ? 'measure' : 'warmup',
		}));
		Module['labradorPacingResult'] = {
			schema: 'labrador-web-engine-pacing-v1', status, run: identity,
			samples, top_out: topOut, gaps, duplicate_observations: duplicates,
			requested_measurement_ms: duration,
			raf_capture: { schema: 'labrador-web-pacing-v1', instrument: 'requestAnimationFrame',
				status: status === 'complete' ? 'complete' : 'truncated', run: identity,
				events, samples: rafSamples },
			scope: 'CPU phase wall times and rAF observer delivery; no GPU time or scan-out measurement',
		};
		console.log('labrador-pacing ' + JSON.stringify(Module['labradorPacingResult']));
	}
	Module['labradorPacingMode'] = modes[modeName];
	Module['labradorPacingBuild'] = function (revision, build, backend, api, deviceName) {
		if (done || buildStamped) throw new Error('The pacing build identity is already recorded.');
		const stamp = {
			source_commit: revision.replace(/-dirty$/, ''),
			source_dirty: revision === 'unknown' ? null : revision.endsWith('-dirty'), build,
			render_backend: backend, render_api: api, render_device_name: deviceName,
		};
		for (const [key, value] of Object.entries(stamp)) {
			Object.defineProperty(identity, key, { value, enumerable: true, writable: false, configurable: false });
		}
		buildStamped = true;
	};
	// Optional operator metadata, never inferred from the observed callback rate.
	Module['labradorPacingSetIdentity'] = function (fields) {
		if (done) throw new Error('The pacing capture has finished.');
		for (const key of ['id', 'device', 'display', 'refresh_hz', 'refresh_source',
			'emscripten', 'zoom', 'power', 'thermal', 'notes', 'artifact_hash']) {
			if (Object.hasOwn(fields, key)) identity[key] = fields[key];
		}
	};
	Module['labradorPacingStop'] = function () { finish('truncated'); };
	Module['labradorPacingFrame'] = function (timestamp, frame, updates, interval,
		update, begin, draw, submit, end, topOutFrame, live, dropped) {
		if (done) return false;
		if (started === null) started = timestamp;
		const measuring = fixture ? topOutFrame > 0 && frame >= topOutFrame : timestamp - started >= 5000;
		if (measuring && measured === null) measured = timestamp;
		const visible = document.visibilityState === 'visible' ? 1 : 0;
		if (frame > lastFrame && visible) updateGeometry(timestamp, frame);
		const offset = observed++ * 4;
		observations[offset] = timestamp;
		observations[offset + 1] = segment;
		observations[offset + 2] = visible;
		observations[offset + 3] = measuring ? 1 : 0;
		if (frame > lastFrame) {
			if (frame !== lastFrame + 1) {
				gaps.push({ after_frame: lastFrame, before_frame: frame, observer_timestamp_ms: timestamp });
			}
			const base = count++ * width;
			rows[base] = timestamp;
			rows[base + 1] = frame;
			rows[base + 2] = updates;
			rows[base + 3] = interval;
			rows[base + 4] = update;
			rows[base + 5] = begin;
			rows[base + 6] = draw;
			rows[base + 7] = submit;
			rows[base + 8] = end;
			rows[base + 9] = topOutFrame;
			rows[base + 10] = live;
			rows[base + 11] = dropped;
			rows[base + 12] = segment;
			rows[base + 13] = visible;
			rows[base + 14] = measuring ? 1 : 0;
			lastFrame = frame;
		} else if (frame !== 0) {
			if (frame < lastFrame) { finish('frame_regression'); return false; }
			++duplicates;
		}
		if (measured !== null && timestamp - measured >= duration) finish('complete');
		else if (count === capacity || observed === capacity) finish('truncated');
		return !done;
	};
	for (const [target, event] of targets) target.addEventListener(event, eventOccurred);
	resizeObserver?.observe(canvas);
	for (const name of ['onQuit', 'onError']) {
		const original = Module[name];
		Module[name] = function (...args) {
			finish(name === 'onQuit' ? 'quit' : 'error');
			return typeof original === 'function' ? original.apply(this, args) : undefined;
		};
	}
})();
