// LineSweeper's flash check: WCAG 2.3.1, Three Flashes or Below Threshold,
// applied frame by frame to a directory of consecutive 60 fps PNGs.
//
//   node tools/flash_check/flash_check.mjs <frames-directory>
//       [--frames FIRST-LAST] [--field WxH] [--edge N]
//
// The frames are what LineSweeperCapture --sequence writes
// (tools/linesweeper_capture/README.md, Footage). It prints what it found and
// exits 0 when no one-second window has more than three general flashes or
// more than three red flashes, 1 when one has, and 2 on bad input.
// samples/linesweeper/README.md, The bursts against WCAG 2.3.1, records the
// method, the result and the commit it was taken at.
//
// It needs Node alone - zlib reads the PNGs - and nothing in it reads a clock
// or a random number, so the same frames give the same report. Its own cases
// are `node --test tools/flash_check/flash_check.test.mjs`.
//
// --frames analyses part of a sequence, numbering frames as the sequence
// does; every pixel's level is first read at FIRST, so a range that starts
// mid-burst does not give the figures a whole run gives. --field sets the
// field in pixels and --edge the frames a concurrent edge may span; both
// exist to price the assumptions below, not to change the verdict.
//
// THE DEFINITIONS, as WCAG 2.2 gives them and as this file applies them:
//
// - Relative luminance per pixel: each 8-bit sRGB channel c/255 linearised
//   with the 0.04045 knee, then L = 0.2126 R + 0.7152 G + 0.0722 B.
// - A general transition is a change in L of 0.1 or more where the darker of
//   the two states is below 0.80. A flash is a pair of opposing transitions.
//   A ramp counts once: a pixel's level is followed from its last extreme, so
//   a rise spread over several frames is one transition, not none.
// - A red transition follows the Understanding document's working
//   definition: the value (R - G - B) x 320, on linear channels with
//   negatives set to zero, changes by more than 20, and the red end is a
//   saturated red, R / (R + G + B) >= 0.8. A pixel that is not saturated red
//   counts as zero, so both transitions of a red flash involve one. WCAG
//   2.2's own note on saturated red measures the change differently, as a
//   difference of more than 0.2 in CIE 1976 UCS chromaticity, and this file
//   does not apply that measure.
// - Area: more than 25% of any 10 degree visual field, which WCAG equates to
//   a 341x256 rectangle on a 1024x768 screen - a third of its width and a
//   third of its height. The field is therefore taken as a third of the frame
//   each way, 427x240 at 1280x720, which assumes the frame fills a screen
//   viewed at the distance that equivalence assumes. --field overrides it.
//
// TWO COUNTS, because WCAG's area is the area of flashes "occurring
// concurrently", and a field of small moving sparks is where reading that word
// loosely and reading it strictly part company. Both look at every one-second
// window - sixty consecutive frame-to-frame changes, ending at every frame -
// and at every position of the field. More than six transitions in a window,
// more than three flashes, is a failure.
//
// - The concurrent count is the definition, and is the verdict. A field makes
//   a transition on a frame when more than 25% of its pixels completed a
//   transition in the same direction within the last CONCURRENT_FRAMES
//   frames, and its direction is the opposite of the field's last one; a
//   second rise with no fall between is the same rise continuing. Allowing
//   three frames rather than one counts an edge that motion or a ramp
//   smears across neighbouring frames, which can only add transitions. A
//   field where more than 25% rose and more than 25% fell in that span has
//   made neither transition: that is light moving across it, the way a burst
//   of sparks moves, and the field as a whole has not brightened or
//   darkened. This rule is the one the result rests on, and it is the
//   reading of "concurrently" a reviewer should argue with if any. WCAG
//   exempts balanced flashing only when it is a fine pattern, with elements
//   smaller than 0.1 degree, and that exemption counts against this rule
//   for anything coarser.
// - The pixel bound is an upper bound, and is reported beside it. The
//   window's count is the largest n for which more than 25% of some field's
//   pixels each made at least n transitions in that second, whenever they
//   made them. Flashes that are not concurrent are counted together, so it
//   can only overstate the rate: under it, a pass is beyond argument, and a
//   failure says only that the concurrent count has to be read.

import { readFileSync, readdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { inflateSync } from 'node:zlib';

export const FRAMES_PER_SECOND = 60;
export const GENERAL_STEP = 0.1;
export const GENERAL_DARKER_BELOW = 0.8;
export const RED_STEP = 20;
export const RED_SATURATION = 0.8;
export const AREA_FRACTION = 0.25;
export const MAX_FLASHES = 3;
export const CONCURRENT_FRAMES = 3;

// The most field transitions kept per position; a window that reaches it is
// reported as at least this many, which is already a failure.
const EVENT_RING = 32;

const LINEAR = new Float64Array(256);
for (let value = 0; value < 256; ++value) {
	const c = value / 255;
	LINEAR[value] = c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
}

// The default field: a third of the frame each way, as 341x256 is of 1024x768.
export function defaultField(width, height) {
	return { width: Math.round(width / 3), height: Math.round(height / 3) };
}

// Reads the PNGs LineSweeperCapture writes and any other 8-bit truecolour,
// non-interlaced one, with or without alpha. Returns RGB bytes; alpha is not
// what a screen shows and is dropped.
export function readPng(file) {
	const data = readFileSync(file);
	const signature = [137, 80, 78, 71, 13, 10, 26, 10];
	if (signature.some((byte, index) => data[index] !== byte)) {
		throw new Error(`${file} is not a PNG.`);
	}
	let width = 0;
	let height = 0;
	let channels = 0;
	const compressed = [];
	for (let offset = 8; offset < data.length;) {
		const length = data.readUInt32BE(offset);
		const type = data.toString('latin1', offset + 4, offset + 8);
		const body = data.subarray(offset + 8, offset + 8 + length);
		if (type === 'IHDR') {
			width = body.readUInt32BE(0);
			height = body.readUInt32BE(4);
			const depth = body[8];
			const colour = body[9];
			if (depth !== 8 || (colour !== 2 && colour !== 6) || body[12] !== 0) {
				throw new Error(`${file} is not 8-bit RGB or RGBA without interlacing.`);
			}
			channels = colour === 2 ? 3 : 4;
		} else if (type === 'IDAT') {
			compressed.push(body);
		} else if (type === 'IEND') {
			break;
		}
		offset += 12 + length;
	}
	const raw = inflateSync(Buffer.concat(compressed));
	const stride = width * channels;
	const rows = new Uint8Array(stride * height);
	for (let y = 0; y < height; ++y) {
		const filter = raw[y * (stride + 1)];
		const source = y * (stride + 1) + 1;
		const row = y * stride;
		const above = row - stride;
		for (let x = 0; x < stride; ++x) {
			const a = x >= channels ? rows[row + x - channels] : 0;
			const b = y > 0 ? rows[above + x] : 0;
			const c = x >= channels && y > 0 ? rows[above + x - channels] : 0;
			let predictor = 0;
			if (filter === 1) {
				predictor = a;
			} else if (filter === 2) {
				predictor = b;
			} else if (filter === 3) {
				predictor = (a + b) >> 1;
			} else if (filter === 4) {
				const p = a + b - c;
				const pa = Math.abs(p - a);
				const pb = Math.abs(p - b);
				const pc = Math.abs(p - c);
				predictor = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
			} else if (filter !== 0) {
				throw new Error(`${file} has an unknown PNG filter ${filter}.`);
			}
			rows[row + x] = (raw[source + x] + predictor) & 0xff;
		}
	}
	if (channels === 3) {
		return { width, height, rgb: rows };
	}
	const rgb = new Uint8Array(width * height * 3);
	for (let i = 0, j = 0; i < rows.length; i += 4, j += 3) {
		rgb[j] = rows[i];
		rgb[j + 1] = rows[i + 1];
		rgb[j + 2] = rows[i + 2];
	}
	return { width, height, rgb };
}

// One measure followed per pixel - luminance, or the red value - and the
// transitions it makes. `step` and `qualifies(darker, lighter)` say what a
// transition is; `strict` is whether the change must exceed the step rather
// than reach it.
class TransitionTracker {
	constructor(pixels, step, strict, qualifies) {
		this.step = step;
		this.strict = strict;
		this.qualifies = qualifies;
		// Before a pixel's first transition its direction is unknown, and
		// both its lowest and highest level so far are kept; after it, the
		// extreme it is moving towards.
		this.low = new Float64Array(pixels);
		this.high = new Float64Array(pixels);
		this.direction = new Int8Array(pixels);
		this.started = false;
	}

	// Returns the indices of the pixels that completed a rising and a falling
	// transition on this frame.
	update(levels) {
		const rises = [];
		const falls = [];
		const { low, high, direction, step, strict, qualifies } = this;
		if (!this.started) {
			low.set(levels);
			high.set(levels);
			this.started = true;
			return { rises, falls };
		}
		const enough = strict ? (d) => d > step : (d) => d >= step;
		for (let i = 0; i < levels.length; ++i) {
			const level = levels[i];
			if (direction[i] === 0) {
				if (level < low[i]) low[i] = level;
				if (level > high[i]) high[i] = level;
				if (enough(high[i] - low[i]) && qualifies(low[i], high[i])) {
					const rising = level === high[i];
					direction[i] = rising ? 1 : -1;
					low[i] = level;
					high[i] = level;
					(rising ? rises : falls).push(i);
				}
			} else if (direction[i] > 0) {
				// Rising: high is the peak since the last transition.
				if (level > high[i]) {
					high[i] = level;
				} else if (enough(high[i] - level) && qualifies(level, high[i])) {
					direction[i] = -1;
					low[i] = level;
					falls.push(i);
				}
			} else if (level < low[i]) {
				low[i] = level;
			} else if (enough(level - low[i]) && qualifies(low[i], level)) {
				direction[i] = 1;
				high[i] = level;
				rises.push(i);
			}
		}
		return { rises, falls };
	}
}

// Sums of a per-pixel indicator over every position of the field, from one
// summed-area table.
class FieldSums {
	constructor(width, height, field) {
		this.width = width;
		this.height = height;
		this.field = field;
		this.columns = width - field.width + 1;
		this.rows = height - field.height + 1;
		this.integral = new Float64Array((width + 1) * (height + 1));
	}

	// Calls visit(position, sum) for every position of the field, where
	// position runs over the field's top-left corners row by row.
	each(indicator, visit) {
		const { width, height, field, columns, rows, integral } = this;
		const w1 = width + 1;
		for (let y = 0; y < height; ++y) {
			let row = 0;
			for (let x = 0; x < width; ++x) {
				row += indicator(y * width + x);
				integral[(y + 1) * w1 + x + 1] = integral[y * w1 + x + 1] + row;
			}
		}
		for (let y = 0; y < rows; ++y) {
			for (let x = 0; x < columns; ++x) {
				const top = y * w1 + x;
				const bottom = (y + field.height) * w1 + x;
				visit(y * columns + x, integral[bottom + field.width] - integral[bottom] -
					integral[top + field.width] + integral[top]);
			}
		}
	}

	largest(indicator) {
		let best = 0;
		this.each(indicator, (position, sum) => {
			if (sum > best) best = sum;
		});
		return best;
	}
}

// The pixel bound: how many transitions each pixel made in the last second,
// and the largest count more than the area threshold of some field reached.
class PixelBound {
	constructor(width, height, field) {
		this.threshold = AREA_FRACTION * field.width * field.height;
		this.sums = new FieldSums(width, height, field);
		this.counts = new Uint16Array(width * height);
		// histogram[n] is how many pixels made exactly n transitions in the
		// window, so a count no field can reach is refused without a scan.
		this.histogram = [width * height];
		this.history = [];
	}

	push(changes) {
		const changed = changes.rises.concat(changes.falls);
		this.history.push(changed);
		for (const i of changed) this.bump(i, 1);
		if (this.history.length > FRAMES_PER_SECOND) {
			for (const i of this.history.shift()) this.bump(i, -1);
		}
	}

	bump(i, by) {
		const before = this.counts[i];
		const after = before + by;
		this.counts[i] = after;
		--this.histogram[before];
		this.histogram[after] = (this.histogram[after] ?? 0) + 1;
	}

	atLeast(n) {
		let total = 0;
		for (let c = n; c < this.histogram.length; ++c) total += this.histogram[c] ?? 0;
		return total;
	}

	// The window's count: the largest n with more than the threshold of some
	// field at n or more. Counts at or below `known` are not looked for.
	count(known) {
		const counts = this.counts;
		let n = known + 1;
		while (this.atLeast(n) > this.threshold &&
			this.sums.largest((i) => (counts[i] >= n ? 1 : 0)) > this.threshold) {
			known = n;
			++n;
		}
		return known;
	}

	singlePixelCount() {
		let n = this.histogram.length - 1;
		while (n > 0 && !this.histogram[n]) --n;
		return n;
	}
}

// The concurrent count: a field transition is more than the area threshold
// of the field's pixels completing a transition the same way within the last
// `edge` frames, and it counts only when it reverses the field's last one.
// A field where both ways pass the threshold on one frame has made neither:
// it cannot brighten and darken at once, and what it shows is light moving
// across it rather than the field changing.
class ConcurrentCount {
	constructor(width, height, field, edge) {
		this.edgeFrames = edge;
		const pixels = width * height;
		this.threshold = AREA_FRACTION * field.width * field.height;
		this.area = field.width * field.height;
		this.sums = new FieldSums(width, height, field);
		const positions = this.sums.columns * this.sums.rows;
		this.lastRise = new Int32Array(pixels).fill(-FRAMES_PER_SECOND);
		this.lastFall = new Int32Array(pixels).fill(-FRAMES_PER_SECOND);
		this.direction = new Int8Array(positions);
		this.events = new Int32Array(positions * EVENT_RING).fill(-FRAMES_PER_SECOND);
		this.next = new Uint8Array(positions);
		this.rising = new Uint8Array(positions);
		this.falling = new Uint8Array(positions);
		// The pixels that changed each way on the recent frames, so that a
		// frame where too few did is refused without a scan.
		this.recent = { rises: [], falls: [] };
		this.worst = { transitions: 0, frame: null };
		this.widest = { rises: 0, falls: 0, risesFrame: null, fallsFrame: null };
	}

	push(frame, changes) {
		for (const i of changes.rises) this.lastRise[i] = frame;
		for (const i of changes.falls) this.lastFall[i] = frame;
		const rising = this.edge(frame, changes.rises, this.recent.rises, this.lastRise, this.rising, 'rises');
		const falling = this.edge(frame, changes.falls, this.recent.falls, this.lastFall, this.falling, 'falls');
		if (!rising && !falling) return;
		for (let position = 0; position < this.direction.length; ++position) {
			const up = rising && this.rising[position] === 1;
			const down = falling && this.falling[position] === 1;
			if (up !== down) this.transition(position, up ? 1 : -1, frame);
		}
	}

	// Marks in `over` the positions where more than the threshold of the
	// field changed this way, and says whether any did.
	edge(frame, changed, recent, last, over, name) {
		recent.push(changed.length);
		if (recent.length > this.edgeFrames) recent.shift();
		const atMost = recent.reduce((total, n) => total + n, 0);
		const widest = this.widest[name];
		if (atMost <= this.threshold && atMost <= widest) return false;

		const since = frame - this.edgeFrames;
		let largest = 0;
		this.sums.each((i) => (last[i] > since ? 1 : 0), (position, sum) => {
			if (sum > largest) largest = sum;
			over[position] = sum > this.threshold ? 1 : 0;
		});
		if (largest > widest) {
			this.widest[name] = largest;
			this.widest[`${name}Frame`] = frame;
		}
		return largest > this.threshold;
	}

	transition(position, sign, frame) {
		if (this.direction[position] === sign) return;
		this.direction[position] = sign;
		const ring = position * EVENT_RING;
		this.events[ring + this.next[position]] = frame;
		this.next[position] = (this.next[position] + 1) % EVENT_RING;
		let inWindow = 0;
		for (let k = 0; k < EVENT_RING; ++k) {
			if (this.events[ring + k] > frame - FRAMES_PER_SECOND) ++inWindow;
		}
		if (inWindow > this.worst.transitions) {
			this.worst.transitions = inWindow;
			this.worst.frame = frame;
		}
	}
}

// One measure - luminance or the red value - followed through every frame
// and counted both ways.
class Measure {
	constructor(width, height, field, edge, tracker) {
		this.tracker = tracker;
		this.bound = new PixelBound(width, height, field);
		this.concurrent = new ConcurrentCount(width, height, field, edge);
		this.boundWorst = { transitions: 0, frame: null };
		this.singlePixel = { transitions: 0, frame: null };
	}

	// A window is named by its last frame: the changes into frames
	// frame-59 ... frame.
	push(frame, levels) {
		const changes = this.tracker.update(levels);
		this.concurrent.push(frame, changes);
		this.bound.push(changes);
		const transitions = this.bound.count(this.boundWorst.transitions);
		if (transitions > this.boundWorst.transitions) {
			this.boundWorst = { transitions, frame };
		}
		const single = this.bound.singlePixelCount();
		if (single > this.singlePixel.transitions) {
			this.singlePixel = { transitions: single, frame };
		}
	}

	result() {
		const concurrent = this.concurrent.worst;
		const area = this.concurrent.area;
		const widest = this.concurrent.widest;
		return {
			transitions: concurrent.transitions,
			flashes: concurrent.transitions / 2,
			frame: concurrent.frame,
			passes: concurrent.transitions <= 2 * MAX_FLASHES,
			atLeast: concurrent.transitions >= EVENT_RING,
			widestRise: { fraction: widest.rises / area, frame: widest.risesFrame },
			widestFall: { fraction: widest.falls / area, frame: widest.fallsFrame },
			bound: { ...this.boundWorst, flashes: this.boundWorst.transitions / 2 },
			singlePixel: this.singlePixel,
		};
	}
}

// Feeds frames one at a time and reports the worst one-second window for
// general and red flashes.
export class FlashAnalyser {
	// `first` is the number of the first frame fed, so that a report names
	// frames as the sequence numbers them; `edge` is CONCURRENT_FRAMES unless
	// a reading of "concurrently" other than the verdict's is being priced.
	constructor(width, height, field = defaultField(width, height),
		{ first = 0, edge = CONCURRENT_FRAMES } = {}) {
		if (field.width > width || field.height > height) {
			throw new Error(`A ${field.width}x${field.height} field does not fit a ${width}x${height} frame.`);
		}
		const pixels = width * height;
		this.width = width;
		this.height = height;
		this.field = field;
		this.first = first;
		this.edge = edge;
		this.frames = 0;
		this.luminance = new Float64Array(pixels);
		this.redness = new Float64Array(pixels);
		this.general = new Measure(width, height, field, edge, new TransitionTracker(
			pixels, GENERAL_STEP, false, (darker) => darker < GENERAL_DARKER_BELOW));
		this.red = new Measure(width, height, field, edge, new TransitionTracker(
			pixels, RED_STEP, true, () => true));
	}

	addFrame(rgb) {
		const { luminance, redness } = this;
		for (let i = 0, j = 0; i < luminance.length; ++i, j += 3) {
			const r = LINEAR[rgb[j]];
			const g = LINEAR[rgb[j + 1]];
			const b = LINEAR[rgb[j + 2]];
			luminance[i] = 0.2126 * r + 0.7152 * g + 0.0722 * b;
			const sum = r + g + b;
			redness[i] = sum > 0 && r / sum >= RED_SATURATION ? Math.max(0, r - g - b) * 320 : 0;
		}
		this.general.push(this.first + this.frames, luminance);
		this.red.push(this.first + this.frames, redness);
		++this.frames;
	}

	result() {
		const general = this.general.result();
		const red = this.red.result();
		return {
			frames: this.frames,
			width: this.width,
			height: this.height,
			field: this.field,
			general,
			red,
			passes: general.passes && red.passes,
		};
	}
}

function percent(fraction) {
	return `${(100 * fraction).toFixed(1)}%`;
}

function describe(name, measure) {
	const where = measure.frame === null ? '' : `, worst window ending at frame ${measure.frame}`;
	const bound = measure.bound.frame === null ? '' : `, window ending at frame ${measure.bound.frame}`;
	const widest = (edge) => (edge.frame === null ? 'none' : `${percent(edge.fraction)} at frame ${edge.frame}`);
	const single = measure.singlePixel.frame === null ? '' : `, window ending at frame ${measure.singlePixel.frame}`;
	return [
		`${name}: ${measure.atLeast ? 'at least ' : ''}${measure.transitions} concurrent transitions (${measure.flashes} flashes) in one second${where}; ` +
			`${measure.passes ? 'passes' : 'FAILS'}.`,
		`  Widest concurrent change in one field: rising ${widest(measure.widestRise)}, falling ${widest(measure.widestFall)}.`,
		`  Pixel bound: ${measure.bound.transitions} transitions (${measure.bound.flashes} flashes)${bound}.`,
		`  One pixel, ignoring area: ${measure.singlePixel.transitions} transitions${single}.`,
	].join('\n');
}

const USAGE = 'usage: node tools/flash_check/flash_check.mjs <frames-directory> ' +
	'[--frames FIRST-LAST] [--field WxH] [--edge N]';

function main(argv) {
	let field = null;
	let range = null;
	let edge = CONCURRENT_FRAMES;
	const rest = [];
	for (let i = 0; i < argv.length; ++i) {
		if (argv[i] === '--field') {
			const match = /^(\d+)x(\d+)$/.exec(argv[++i] ?? '');
			if (!match) throw new Error(USAGE);
			field = { width: Number(match[1]), height: Number(match[2]) };
		} else if (argv[i] === '--frames') {
			const match = /^(\d+)-(\d+)$/.exec(argv[++i] ?? '');
			if (!match || Number(match[1]) > Number(match[2])) throw new Error(USAGE);
			range = { first: Number(match[1]), last: Number(match[2]) };
		} else if (argv[i] === '--edge') {
			const match = /^[1-9]\d*$/.exec(argv[++i] ?? '');
			if (!match) throw new Error(USAGE);
			edge = Number(match[0]);
		} else {
			rest.push(argv[i]);
		}
	}
	if (rest.length !== 1) throw new Error(USAGE);

	const directory = rest[0];
	const names = readdirSync(directory).filter((name) => name.endsWith('.png')).sort();
	if (names.length === 0 || names.some((name, index) => name !== `${String(index).padStart(6, '0')}.png`)) {
		throw new Error('Expected a continuous sequence of PNGs starting at 000000.png.');
	}
	range ??= { first: 0, last: names.length - 1 };
	if (range.last >= names.length) {
		throw new Error(`There are ${names.length} frames, so ${range.last} is not one of them.`);
	}

	let analyser = null;
	for (let index = range.first; index <= range.last; ++index) {
		const frame = readPng(path.join(directory, names[index]));
		analyser ??= new FlashAnalyser(frame.width, frame.height,
			field ?? defaultField(frame.width, frame.height), { first: range.first, edge });
		if (frame.width !== analyser.width || frame.height !== analyser.height) {
			throw new Error(`${names[index]} is ${frame.width}x${frame.height}, not ${analyser.width}x${analyser.height}.`);
		}
		analyser.addFrame(frame.rgb);
	}

	const result = analyser.result();
	console.log(`Frames ${range.first} to ${range.last}, ${result.width}x${result.height} at ${FRAMES_PER_SECOND} fps ` +
		`(${(result.frames / FRAMES_PER_SECOND).toFixed(3)} s); field ${result.field.width}x${result.field.height}, ` +
		`area threshold more than ${AREA_FRACTION * result.field.width * result.field.height} pixels; ` +
		`a concurrent edge is ${edge} frame${edge === 1 ? '' : 's'}.`);
	console.log(describe('General flashes', result.general));
	console.log(describe('Red flashes', result.red));
	console.log(result.passes
		? `Passes WCAG 2.3.1: no window has more than ${MAX_FLASHES} concurrent flashes.`
		: `FAILS WCAG 2.3.1: a window has more than ${MAX_FLASHES} concurrent flashes.`);
	return result.passes ? 0 : 1;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		process.exitCode = main(process.argv.slice(2));
	} catch (error) {
		console.error(`flash_check refused: ${error.message}`);
		process.exitCode = 2;
	}
}
