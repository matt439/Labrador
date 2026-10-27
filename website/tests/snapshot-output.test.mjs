import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { verifySnapshotOutput } from '../scripts/snapshot-output.mjs';

const SOURCE = {
	schemaVersion: 1,
	tag: 'v0.3.0',
	commit: 'a'.repeat(40),
	sourceDate: '2026-10-27',
	snapshotPath: '/versions/v0.3.0/',
};
const ORIGIN = 'https://labradorengine.com';
const PREFIX = SOURCE.snapshotPath;

function write(root, file, contents) {
	const target = path.join(root, file);
	mkdirSync(path.dirname(target), { recursive: true });
	writeFileSync(target, contents);
}

function page(route = '', extra = '', label = `Release documentation (${SOURCE.tag})`, commit = SOURCE.commit) {
	return `<!doctype html><html><head><link rel="canonical" href="${ORIGIN}${PREFIX}${route}"></head>
	<body><p>${label} <a href="https://github.com/matt439/Labrador/commit/${commit}">Source commit</a></p>${extra}</body></html>`;
}

function fixture(t, { html = page(), searchUrl = `${PREFIX}docs/`, searchCount = 1 } = {}) {
	const root = mkdtempSync(path.join(tmpdir(), 'labrador-snapshot-output-'));
	t.after(() => {
		const target = realpathSync(root);
		assert.equal(path.dirname(target), realpathSync(tmpdir()));
		assert.ok(path.basename(target).startsWith('labrador-snapshot-output-'));
		rmSync(target, { recursive: true, force: true });
	});
	write(root, 'index.html', html);
	write(root, 'docs/index.html', page('docs/'));
	write(root, '_astro/example.css', 'body { color: black; }');
	write(root, 'media/poster.png', 'fixture image');
	write(root, 'pagefind/pagefind-entry.json', JSON.stringify({ languages: { en: { page_count: searchCount } } }));
	write(root, 'pagefind/result.json', JSON.stringify({ url: searchUrl, content: 'Labrador documentation fixture' }));
	// This module models the emitted runtime's public API and makes a real fetch
	// through the verifier's isolated output reader. Site builds additionally
	// exercise the actual emitted Pagefind/Wasm module against the full index.
	write(root, 'pagefind/pagefind.js', `
		let basePath;
		export async function options(value) { basePath = value.basePath; }
		export async function search() {
			const data = await (await fetch(basePath + 'result.json')).json();
			return { results: [{ data: async () => data }] };
		}
		export async function destroy() {}
	`);
	return root;
}

test('verifies prefixed pages, relative assets and search without modifying output', async (t) => {
	const html = page('', `<a href="${PREFIX}docs/?q=one&amp;page=two#introduction">Docs</a>
		<link rel="stylesheet" href="_astro/example.css">
		<img src="media/poster.png"><a href="#local">Local</a><a href="mailto:owner@example.invalid">Mail</a>
		<a href="https://example.invalid/docs/">External</a>
		<script>const example = '<a href="/docs/">not markup</a>';</script>`);
	const root = fixture(t, { html });
	const before = readFileSync(path.join(root, 'index.html'));
	const result = await verifySnapshotOutput(root, SOURCE);
	assert.equal(result.pages, 2);
	assert.equal(result.searchPages, 1);
	assert.ok(result.references >= 7);
	assert.deepEqual(readFileSync(path.join(root, 'index.html')), before);
});

test('rejects a development link, including HTML character-reference escaping', async (t) => {
	for (const href of ['/docs/', '&#47;docs/', `${ORIGIN}/docs/`, '//labradorengine.com/docs/']) {
		const root = fixture(t, { html: page('', `<a href="${href}">Docs</a>`) });
		await assert.rejects(verifySnapshotOutput(root, SOURCE), /escapes snapshot prefix/);
	}
});

test('a similar release prefix is still outside this snapshot', async (t) => {
	const root = fixture(t, { html: page('', '<a href="/versions/v0.3.01/docs/">Other version</a>') });
	await assert.rejects(verifySnapshotOutput(root, SOURCE), /escapes snapshot prefix/);
});

test('relative links cannot climb out of the release prefix', async (t) => {
	const root = fixture(t);
	write(root, 'docs/index.html', page('docs/', '<a href="../../docs/">Escapes</a>'));
	await assert.rejects(verifySnapshotOutput(root, SOURCE), /escapes snapshot prefix/);
});

test('missing linked pages and assets fail even when their query or fragment differs', async (t) => {
	for (const extra of [
		`<a href="${PREFIX}absent/?q=yes#anchor">Missing page</a>`,
		`<img src="${PREFIX}media/absent.png?v=1#crop">`,
		`<script src="${PREFIX}_astro/absent.js"></script>`,
	]) {
		const root = fixture(t, { html: page('', extra) });
		await assert.rejects(verifySnapshotOutput(root, SOURCE), /missing snapshot target/);
	}
});

test('poster and every srcset candidate must stay in the snapshot and exist', async (t) => {
	const valid = `<video poster="${PREFIX}media/poster.png"></video>
		<img srcset="${PREFIX}media/poster.png 1x, ${PREFIX}media/poster.png?size=2 2x">`;
	await verifySnapshotOutput(fixture(t, { html: page('', valid) }), SOURCE);
	for (const extra of [
		'<video poster="/media/poster.png"></video>',
		`<img srcset="${PREFIX}media/poster.png 1x, /media/poster.png 2x">`,
		`<img srcset="${PREFIX}media/poster.png 1x, ${PREFIX}media/absent.png 2x">`,
	]) {
		await assert.rejects(verifySnapshotOutput(fixture(t, { html: page('', extra) }), SOURCE), /escapes snapshot prefix|missing snapshot target/);
	}
});

test('every page needs a visible release label and the exact full commit link', async (t) => {
	for (const html of [
		page('', '', 'Development documentation (unreleased)'),
		page('', `<script>const label = '${SOURCE.tag}'</script>`, 'Development'),
		page('', '', `Release documentation (${SOURCE.tag})`, SOURCE.commit.slice(0, 7)),
		page('', '', `Release documentation (${SOURCE.tag})`, 'b'.repeat(40)),
	]) {
		await assert.rejects(verifySnapshotOutput(fixture(t, { html }), SOURCE), /missing visible release label|missing full release source-commit link/);
	}
});

test('source links and archived edit links cannot follow a moving branch', async (t) => {
	for (const route of ['edit/master/website/page.mdx', 'blob/master/engine/core/object.h', 'tree/main/samples/', `blob/${SOURCE.commit.slice(0, 7)}/engine/core/object.h`]) {
		const root = fixture(t, { html: page('', `<a href="https://github.com/matt439/Labrador/${route}">Source</a>`) });
		await assert.rejects(verifySnapshotOutput(root, SOURCE), /source link does not name the full release commit/);
	}
	await verifySnapshotOutput(fixture(t, { html: page('', `<a href="https://github.com/matt439/Labrador/blob/${SOURCE.commit}/engine/core/object.h#L3">Source</a>`) }), SOURCE);
});

test('historical release examples require an explicit marker and a stable tag', async (t) => {
	const historical = 'https://github.com/matt439/Labrador/blob/v0.2.0/CHANGES.md';
	await verifySnapshotOutput(fixture(t, { html: page('', `<a href="${historical}" data-release-example>Earlier release</a>`) }), SOURCE);
	await assert.rejects(verifySnapshotOutput(fixture(t, { html: page('', `<a href="${historical}">Source</a>`) }), SOURCE), /source link/);
	for (const route of ['blob/master/CHANGES.md', 'edit/v0.2.0/CHANGES.md', 'blob/v0.3.0-rc.1/CHANGES.md']) {
		const html = page('', `<a href="https://github.com/matt439/Labrador/${route}" data-release-example>Example</a>`);
		await assert.rejects(verifySnapshotOutput(fixture(t, { html }), SOURCE), /source link/);
	}
});

test('GitHub repository casing cannot bypass source revision checks', async (t) => {
	for (const route of ['blob/master/engine/core/object.h', `EDIT/${SOURCE.commit}/website/page.mdx`]) {
		const html = page('', `<a href="https://github.com/Matt439/labrador/${route}">Source</a>`);
		await assert.rejects(verifySnapshotOutput(fixture(t, { html }), SOURCE), /source link/);
	}
});

test('standalone 404 output has its own canonical page and release label', async (t) => {
	const root = fixture(t);
	write(root, '404.html', page('404/'));
	assert.equal((await verifySnapshotOutput(root, SOURCE)).pages, 3);
	write(root, '404.html', page('docs/'));
	await assert.rejects(verifySnapshotOutput(root, SOURCE), /canonical URL does not identify/);
});

test('canonical URLs must be absolute and within this snapshot', async (t) => {
	for (const href of ['/versions/v0.3.0/', 'https://example.invalid/versions/v0.3.0/', `${ORIGIN}/`, `${ORIGIN}${PREFIX}missing/`]) {
		const html = page().replace(`${ORIGIN}${PREFIX}`, href);
		await assert.rejects(verifySnapshotOutput(fixture(t, { html }), SOURCE), /canonical URL must be absolute|canonical URL does not identify|outside the snapshot origin|escapes snapshot prefix|missing snapshot target/);
	}
});

test('a base element cannot override the checked page URL', async (t) => {
	const root = fixture(t, { html: page().replace('<head>', '<head><base href="/">') });
	await assert.rejects(verifySnapshotOutput(root, SOURCE), /base elements/);
});

test('search results stay within the same snapshot and name existing pages', async (t) => {
	for (const searchUrl of ['/docs/', `${PREFIX}missing/`, 'https://example.invalid/docs/', '/versions/v0.3.01/docs/']) {
		await assert.rejects(verifySnapshotOutput(fixture(t, { searchUrl }), SOURCE), /escapes snapshot prefix|missing snapshot target|outside the snapshot origin/);
	}
});

test('a missing search module or mismatched index page count fails qualification', async (t) => {
	const root = fixture(t);
	rmSync(path.join(root, 'pagefind/pagefind.js'));
	await assert.rejects(verifySnapshotOutput(root, SOURCE), /missing pagefind\/pagefind.js/);
	await assert.rejects(verifySnapshotOutput(fixture(t, { searchCount: 2 }), SOURCE), /entry declares 2/);
});

test('the search runtime cannot fetch development output or network assets', async (t) => {
	const root = fixture(t);
	write(root, 'pagefind/pagefind.js', `export async function options() {}
		export async function search() { await fetch('/pagefind/result.json'); }
		export async function destroy() {}`);
	await assert.rejects(verifySnapshotOutput(root, SOURCE), /Pagefind fetched outside its snapshot/);
});

test('an interrupted search runtime fails instead of leaving qualification pending', async (t) => {
	const root = fixture(t);
	write(root, 'pagefind/pagefind.js', 'export async function options() { process.exit(0); }');
	await assert.rejects(verifySnapshotOutput(root, SOURCE), /before reporting a result/);
});

test('an unverified source record cannot select arbitrary output routing', async (t) => {
	const root = fixture(t);
	await assert.rejects(verifySnapshotOutput(root, { ...SOURCE, snapshotPath: '/' }), /verified release-source record/);
});
