import assert from 'node:assert/strict';
import { copyFileSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test } from 'node:test';
import { CAPTURE_PATH, verifyPreviewImageOutput, verifyReadmeImage } from '../scripts/check-preview-image.mjs';

const REPOSITORY = fileURLToPath(new URL('../../', import.meta.url));
const ALT = 'LineSweeper: a falling square above coloured blocks with score, hold and next-piece slots.';

function fixture(t, base = '/') {
	const root = mkdtempSync(path.join(os.tmpdir(), 'labrador-preview-image-'));
	t.after(() => {
		assert.equal(path.dirname(path.resolve(root)), path.resolve(os.tmpdir()));
		rmSync(root, { recursive: true, force: true });
	});
	const source = path.join(root, CAPTURE_PATH);
	mkdirSync(path.dirname(source), { recursive: true });
	copyFileSync(path.join(REPOSITORY, CAPTURE_PATH), source);
	writeFileSync(path.join(root, 'README.md'), `<img src="${CAPTURE_PATH}" width="1280" height="720" alt="${ALT}">`);
	const dist = path.join(root, 'dist');
	mkdirSync(path.join(dist, '_astro'), { recursive: true });
	copyFileSync(source, path.join(dist, '_astro/frame.png'));
	const image = `https://labradorengine.com${base}_astro/frame.png`;
	const metadata = {
		'og:image': image, 'og:image:type': 'image/png',
		'og:image:width': '1280', 'og:image:height': '720', 'og:image:alt': ALT,
		'twitter:image': image, 'twitter:image:alt': ALT, 'twitter:card': 'summary_large_image',
	};
	function html(changes = {}, extra = '') {
		return '<!doctype html><html><head>' + Object.entries({ ...metadata, ...changes })
			.filter(([, value]) => value !== undefined)
			.map(([name, value]) => `<meta ${name.startsWith('og:') ? 'property' : 'name'}="${name}" content="${value}">`)
			.join('') + extra + '</head><body>Fixture</body></html>';
	}
	function page(changes = {}, extra = '') { writeFileSync(path.join(dist, 'index.html'), html(changes, extra)); }
	page();
	return { root, dist, html, page, verify: () => verifyPreviewImageOutput(dist, { root, base }) };
}

test('the repository README selects the existing full-size PNG with alternative text', () => {
	const image = verifyReadmeImage();
	assert.equal(image.width, 1280);
	assert.equal(image.height, 720);
	assert.deepEqual(image.bytes, readFileSync(path.join(REPOSITORY, CAPTURE_PATH)));
});

test('built development and release metadata resolve to the original capture bytes', (t) => {
	for (const base of ['/', '/versions/v0.3.0/']) {
		const f = fixture(t, base);
		mkdirSync(path.join(f.dist, 'docs'));
		writeFileSync(path.join(f.dist, 'docs/index.html'), f.html());
		assert.deepEqual(f.verify(), { pages: 2, width: 1280, height: 720 });
	}
});

test('a missing, duplicated or dimension-mismatched README image fails', (t) => {
	for (const content of [
		`<img src="missing.png" width="1280" height="720" alt="${ALT}">`,
		`<img src="${CAPTURE_PATH}" width="640" height="360" alt="${ALT}">`,
		`<img src="${CAPTURE_PATH}" width="1280" height="720" alt="Game">`,
		`<img src="${CAPTURE_PATH}"><img src="${CAPTURE_PATH}">`,
	]) {
		const f = fixture(t);
		writeFileSync(path.join(f.root, 'README.md'), content);
		assert.throws(f.verify, /README:/);
	}
});

test('missing or duplicate image/card metadata fails instead of choosing one', (t) => {
	for (const name of ['og:image', 'og:image:alt', 'twitter:image', 'twitter:card']) {
		const f = fixture(t);
		f.page({ [name]: undefined });
		assert.throws(f.verify, /expected exactly one/);
		f.page({}, `<meta name="${name}" content="duplicate">`);
		assert.throws(f.verify, /expected exactly one/);
	}
});

test('image URLs must be absolute, local to this build and inside the release base', (t) => {
	for (const url of [
		'/_astro/frame.png', 'https://other.example/_astro/frame.png',
		'https://labradorengine.com/_astro/frame.png',
		'https://labradorengine.com/versions/v0.3.00/_astro/frame.png',
		'https://labradorengine.com/versions/v0.3.0/_astro/frame.png?crop=1',
		'https://labradorengine.com/versions/v0.3.0/_astro/frame.png#crop',
	]) {
		const f = fixture(t, '/versions/v0.3.0/');
		f.page({ 'og:image': url, 'twitter:image': url });
		assert.throws(f.verify, /Invalid URL|preview image must stay within/);
	}
});

test('missing assets and derived image bytes fail', (t) => {
	const f = fixture(t);
	const image = path.join(f.dist, '_astro/frame.png');
	writeFileSync(image, Buffer.concat([readFileSync(image), Buffer.from('changed')]));
	assert.throws(f.verify, /original capture bytes/);
	rmSync(image);
	assert.throws(f.verify, /ENOENT/);
});

test('dimensions, type, alt text and Twitter image must describe the selected capture', (t) => {
	for (const changes of [
		{ 'og:image:width': '640' }, { 'og:image:height': '360' },
		{ 'og:image:type': 'image/jpeg' }, { 'og:image:alt': 'Game' },
		{ 'twitter:image': 'https://labradorengine.com/_astro/other.png' },
		{ 'twitter:image:alt': 'Different frame' }, { 'twitter:card': 'summary' },
	]) {
		const f = fixture(t);
		f.page(changes);
		assert.throws(f.verify, /preview metadata must describe/);
	}
});

test('already-published snapshots retain their original metadata', (t) => {
	const f = fixture(t);
	mkdirSync(path.join(f.dist, 'versions/v0.3.0'), { recursive: true });
	writeFileSync(path.join(f.dist, 'versions/v0.3.0/index.html'), '<html><head></head><body>Old snapshot</body></html>');
	assert.equal(f.verify().pages, 1);
});

test('the development versions index keeps the same metadata checks', (t) => {
	const f = fixture(t);
	mkdirSync(path.join(f.dist, 'versions'));
	writeFileSync(path.join(f.dist, 'versions/index.html'), '<html><head></head><body>Release list</body></html>');
	assert.throws(f.verify, /expected exactly one og:image/);
	writeFileSync(path.join(f.dist, 'versions/index.html'), f.html());
	assert.equal(f.verify().pages, 2);
});

test('empty output cannot count as preview-image qualification', (t) => {
	const f = fixture(t);
	rmSync(path.join(f.dist, 'index.html'));
	assert.throws(f.verify, /expected built HTML pages/);
});
