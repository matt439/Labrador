// The flash check's own cases, on synthetic frames whose answer is known:
//   node --test tools/flash_check/flash_check.test.mjs
// Each frame is 30x30 and the field 10x10, so the area threshold is more than
// 25 pixels of some 10x10 square.

import assert from 'node:assert/strict';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { deflateSync } from 'node:zlib';

import { FlashAnalyser, defaultField, readPng } from './flash_check.mjs';

const SIZE = 30;
const FIELD = { width: 10, height: 10 };

// A frame of one colour, with an optional square of another at the origin.
function frame(background, square = null, side = 0) {
	const rgb = new Uint8Array(SIZE * SIZE * 3);
	for (let y = 0; y < SIZE; ++y) {
		for (let x = 0; x < SIZE; ++x) {
			const colour = square && x < side && y < side ? square : background;
			rgb.set(colour, (y * SIZE + x) * 3);
		}
	}
	return rgb;
}

// `count` frames that switch between two frames every `period` frames.
function analyse(a, b, period, count = 180) {
	const analyser = new FlashAnalyser(SIZE, SIZE, FIELD);
	for (let i = 0; i < count; ++i) {
		analyser.addFrame(Math.floor(i / period) % 2 === 0 ? a : b);
	}
	return analyser.result();
}

const BLACK = [0, 0, 0];
const WHITE = [255, 255, 255];

test('black and white every five frames is six flashes a second and fails', () => {
	const result = analyse(frame(BLACK), frame(WHITE), 5);
	assert.equal(result.general.transitions, 12);
	assert.equal(result.general.flashes, 6);
	assert.equal(result.passes, false);
});

test('every ten frames is six transitions, three flashes, and passes', () => {
	const result = analyse(frame(BLACK), frame(WHITE), 10);
	assert.equal(result.general.transitions, 6);
	assert.equal(result.general.passes, true);
});

test('a fast flash on a quarter of a field or less is under the area threshold', () => {
	// A 5x5 square is 25 pixels: exactly 25%, which is not more than it.
	const result = analyse(frame(BLACK), frame(BLACK, WHITE, 5), 2);
	assert.equal(result.general.transitions, 0);
	assert.equal(result.general.singlePixel.transitions, 30);
	assert.equal(result.passes, true);
});

test('a fast flash on more than a quarter of a field fails', () => {
	// A 6x6 square is 36 pixels, switching every four frames: fifteen
	// transitions in every sixty changes.
	const result = analyse(frame(BLACK), frame(BLACK, WHITE, 6), 4);
	assert.equal(result.general.transitions, 15);
	assert.equal(result.general.bound.transitions, 15);
	assert.equal(result.passes, false);
});

test('flashes that are not concurrent fail the pixel bound and not the count', () => {
	// Five sets of columns, each a fifth of every field, lit three frames in
	// turn: every pixel flashes four times a second, but never more than one
	// set rises or falls within any three frames.
	const sets = [];
	for (let lit = 0; lit < 5; ++lit) {
		const rgb = new Uint8Array(SIZE * SIZE * 3);
		for (let i = 0; i < SIZE * SIZE; ++i) {
			if ((i % SIZE) % 5 === lit) rgb.fill(255, i * 3, i * 3 + 3);
		}
		sets.push(rgb);
	}
	const analyser = new FlashAnalyser(SIZE, SIZE, FIELD);
	for (let i = 0; i < 180; ++i) analyser.addFrame(sets[Math.floor(i / 3) % 5]);
	const result = analyser.result();
	assert.equal(result.general.bound.transitions, 8);
	assert.equal(result.general.transitions, 0);
	assert.equal(result.general.widestRise.fraction, 0.2);
	assert.equal(result.passes, true);
});

test('light moving across a field is not the field flashing', () => {
	// A one-pixel checkerboard that swaps every frame: every field has half
	// its pixels rising and half falling on each frame, and within any three
	// frames every pixel has done both, so no field changes.
	const boards = [0, 1].map((phase) => {
		const rgb = new Uint8Array(SIZE * SIZE * 3);
		for (let y = 0; y < SIZE; ++y) {
			for (let x = 0; x < SIZE; ++x) {
				if ((x + y + phase) % 2 === 0) rgb.fill(255, (y * SIZE + x) * 3, (y * SIZE + x) * 3 + 3);
			}
		}
		return rgb;
	});
	const analyser = new FlashAnalyser(SIZE, SIZE, FIELD);
	for (let i = 0; i < 120; ++i) analyser.addFrame(boards[i % 2]);
	const result = analyser.result();
	assert.equal(result.general.transitions, 0);
	assert.equal(result.general.bound.transitions, 60);
	assert.equal(result.general.widestRise.fraction, 1);
	assert.equal(result.general.widestFall.fraction, 1);
});

test('a change between two levels above 0.80 is not a transition', () => {
	// sRGB 235 is relative luminance 0.83.
	const result = analyse(frame([235, 235, 235]), frame(WHITE), 2);
	assert.equal(result.general.singlePixel.transitions, 0);
	assert.equal(result.passes, true);
});

test('a change of less than 0.1 is not a transition', () => {
	// sRGB 80 is 0.080; black to it is under the step.
	const result = analyse(frame(BLACK), frame([80, 80, 80]), 2);
	assert.equal(result.general.singlePixel.transitions, 0);
});

test('a ramp is followed to its extreme and counts once each way', () => {
	const analyser = new FlashAnalyser(SIZE, SIZE, FIELD);
	// Up from black in steps too small to be a transition alone, then down.
	const levels = [0, 20, 40, 60, 80, 100, 120, 100, 80, 60, 40, 20, 0];
	for (const level of levels) analyser.addFrame(frame([level, level, level]));
	const result = analyser.result();
	assert.equal(result.general.transitions, 2);
});

test('a dark saturated red flashing fast is a red failure and not a general one', () => {
	// sRGB 150 red is linear 0.30: luminance 0.065, under the general step,
	// and (R - G - B) x 320 = 97, over the red one.
	const result = analyse(frame(BLACK), frame([150, 0, 0]), 5);
	assert.equal(result.general.singlePixel.transitions, 0);
	assert.equal(result.red.transitions, 12);
	assert.equal(result.red.passes, false);
	assert.equal(result.passes, false);
});

test('a red that is not saturated is not a red flash', () => {
	// Linear R 0.30 against G 0.10: R / (R + G + B) is 0.75.
	const result = analyse(frame(BLACK), frame([150, 89, 0]), 5);
	assert.equal(result.red.singlePixel.transitions, 0);
});

test('the default field is a third of the frame each way', () => {
	assert.deepEqual(defaultField(1024, 768), { width: 341, height: 256 });
	assert.deepEqual(defaultField(1280, 720), { width: 427, height: 240 });
});

test('the PNG reader undoes every filter', () => {
	// A 2x5 RGB image, its row y written with PNG filter type y.
	const width = 2;
	const pixels = [
		[10, 20, 30, 40, 50, 60],
		[11, 21, 31, 41, 51, 61],
		[90, 80, 70, 60, 50, 40],
		[5, 250, 5, 250, 5, 250],
		[128, 64, 32, 16, 8, 4],
	];
	const raw = [];
	for (let y = 0; y < pixels.length; ++y) {
		const row = pixels[y];
		const above = y > 0 ? pixels[y - 1] : new Array(row.length).fill(0);
		raw.push(y);
		for (let x = 0; x < row.length; ++x) {
			const a = x >= 3 ? row[x - 3] : 0;
			const b = above[x];
			const c = x >= 3 ? above[x - 3] : 0;
			const p = a + b - c;
			const paeth = Math.abs(p - a) <= Math.abs(p - b) && Math.abs(p - a) <= Math.abs(p - c)
				? a : Math.abs(p - b) <= Math.abs(p - c) ? b : c;
			const predictor = [0, a, b, (a + b) >> 1, paeth][y];
			raw.push((row[x] - predictor) & 0xff);
		}
	}
	const chunk = (type, body) => {
		const header = Buffer.alloc(8);
		header.writeUInt32BE(body.length, 0);
		header.write(type, 4, 'latin1');
		return Buffer.concat([header, body, Buffer.alloc(4)]);
	};
	const ihdr = Buffer.alloc(13);
	ihdr.writeUInt32BE(width, 0);
	ihdr.writeUInt32BE(pixels.length, 4);
	ihdr[8] = 8;
	ihdr[9] = 2;
	const png = Buffer.concat([
		Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
		chunk('IHDR', ihdr),
		chunk('IDAT', deflateSync(Buffer.from(raw))),
		chunk('IEND', Buffer.alloc(0)),
	]);
	const directory = mkdtempSync(path.join(tmpdir(), 'flash-check-'));
	try {
		const file = path.join(directory, 'filters.png');
		writeFileSync(file, png);
		const decoded = readPng(file);
		assert.equal(decoded.width, width);
		assert.equal(decoded.height, pixels.length);
		assert.deepEqual([...decoded.rgb], pixels.flat());
	} finally {
		rmSync(directory, { recursive: true, force: true });
	}
});
