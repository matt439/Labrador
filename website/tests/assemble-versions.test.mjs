import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync } from 'node:fs';
import path from 'node:path';
import { test } from 'node:test';
import { assembleVersions } from '../scripts/assemble-versions.mjs';
import { assetUrl, canonicalize } from '../scripts/published-versions.mjs';
import { verifySnapshotOutput } from '../scripts/snapshot-output.mjs';
import {
	DEVELOPMENT_PAGES, RELEASE_PAGES, developmentDist, releaseAsset, response, scriptedFetch, temporaryDirectory, write,
} from './fixtures/snapshot-site.mjs';

const DEVELOPMENT_COMMIT = 'c'.repeat(40);

function website(t, releases = [], errata = []) {
	const root = temporaryDirectory(t, 'labrador-assemble-');
	write(root, 'published-versions.json', canonicalize({ schemaVersion: 1, releases, errata }));
	developmentDist(path.join(root, 'dist'), { commit: DEVELOPMENT_COMMIT });
	return root;
}

function unreachable() {
	throw new Error('the network must not be used');
}

function options(root, extra = {}) {
	return { website: root, fetch: unreachable, cacheDir: undefined, env: {}, sleep: async () => {}, warn: () => {}, ...extra };
}

function searchHashes(root) {
	const folder = path.join(root, 'dist/pagefind');
	return readdirSync(folder).sort().map((name) => `${name} ${createHash('sha256').update(readFileSync(path.join(folder, name))).digest('hex')}`);
}

function versionsFolder(root) {
	return readdirSync(path.join(root, 'dist/versions')).sort();
}

function index(root) {
	return JSON.parse(readFileSync(path.join(root, 'dist/versions/index.json'), 'utf8'));
}

test('an empty manifest makes no network call and lists only development', async (t) => {
	const root = website(t);
	const before = searchHashes(root);
	const result = await assembleVersions(options(root));
	assert.deepEqual(index(root), {
		schemaVersion: 1,
		versions: [{ id: 'development', label: 'Development', path: '/', commit: DEVELOPMENT_COMMIT, pages: DEVELOPMENT_PAGES }],
		publishedOrder: [],
	});
	assert.deepEqual(result.index, index(root));
	assert.deepEqual(versionsFolder(root), ['index.html', 'index.json']);
	assert.deepEqual(searchHashes(root), before);
});

test('a release served by its GitHub download mounts, lists its pages and is verified again', async (t) => {
	const { archive, entry, source } = await releaseAsset(t);
	const root = website(t, [entry]);
	const before = searchHashes(root);
	const { fetch, calls } = scriptedFetch([() => response(archive)]);
	const verified = [];
	const verify = async (directory, record) => {
		verified.push(record);
		return verifySnapshotOutput(directory, record);
	};
	await assembleVersions(options(root, { fetch, verify }));
	assert.deepEqual(calls.map((call) => call.url), [assetUrl(entry)]);
	assert.equal(calls[0].options.redirect, 'follow');
	assert.deepEqual(verified, [source]);
	assert.deepEqual(versionsFolder(root), ['index.html', 'index.json', 'v0.3.0']);
	assert.deepEqual(readdirSync(path.join(root, 'dist/versions/v0.3.0')).sort(), ['404.html', 'docs', 'index.html', 'pagefind', 'snapshot-build.json']);
	assert.deepEqual(index(root).versions[1], {
		id: 'v0.3.0', label: 'v0.3.0', path: '/versions/v0.3.0/', commit: entry.commit, published: entry.published,
		latest: true, sha256: entry.sha256, pages: RELEASE_PAGES,
	});
	assert.deepEqual(index(root).publishedOrder, ['v0.3.0']);
	assert.deepEqual(searchHashes(root), before);
});

test('a cached asset is used without the network', async (t) => {
	const { archive, entry } = await releaseAsset(t);
	const root = website(t, [entry]);
	const cacheDir = temporaryDirectory(t, 'labrador-assemble-cache-');
	write(cacheDir, entry.asset, archive);
	await assembleVersions(options(root, { cacheDir }));
	assert.deepEqual(index(root).versions.map((version) => version.id), ['development', 'v0.3.0']);
	assert.ok(existsSync(path.join(root, 'dist/versions/v0.3.0/docs/guides/one/index.html')));
});

test('releases are listed newest first while publishedOrder keeps the append order', async (t) => {
	const newer = await releaseAsset(t, { tag: 'v0.4.0', commit: 'b'.repeat(40) });
	const backport = await releaseAsset(t, { tag: 'v0.3.1', commit: 'd'.repeat(40) });
	const root = website(t, [newer.entry, backport.entry]);
	const cacheDir = temporaryDirectory(t, 'labrador-assemble-cache-');
	write(cacheDir, newer.entry.asset, newer.archive);
	write(cacheDir, backport.entry.asset, backport.archive);
	await assembleVersions(options(root, { cacheDir }));
	const { versions, publishedOrder } = index(root);
	assert.deepEqual(versions.map((version) => [version.id, version.latest]), [['development', undefined], ['v0.4.0', true], ['v0.3.1', false]]);
	assert.deepEqual(publishedOrder, ['v0.4.0', 'v0.3.1']);
});

function assertNoPartial(root) {
	assert.deepEqual(versionsFolder(root).filter((name) => name.startsWith('.')), []);
}

test('an asset whose bytes differ from the manifest is refused and never retried', async (t) => {
	const { archive, entry } = await releaseAsset(t);
	const root = website(t, [{ ...entry, sha256: 'f'.repeat(64) }]);
	const { fetch, calls } = scriptedFetch([() => response(archive)]);
	await assert.rejects(assembleVersions(options(root, { fetch })), /^Error: v0\.3\.0: read archive: labrador-docs-v0\.3\.0\.tar\.gz: expected SHA-256 f{64}, found [0-9a-f]{64}\.$/);
	assert.equal(calls.length, 1);
	assertNoPartial(root);
	assert.equal(existsSync(path.join(root, 'dist/versions/index.json')), false);
});

test('a build record naming another tag or commit is refused', async (t) => {
	for (const [record, expected] of [
		[(value) => ({ ...value, tag: 'v0.3.1', snapshotPath: '/versions/v0.3.1/' }), /v0\.3\.0: build record: expected snapshot-build\.json tag "v0\.3\.0", found "v0\.3\.1"/],
		[(value) => ({ ...value, commit: 'e'.repeat(40) }), /v0\.3\.0: build record: expected snapshot-build\.json commit "a{40}", found "e{40}"/],
		[(value) => ({ ...value, schemaVersion: 2 }), /build record: expected snapshot-build\.json schemaVersion 1, found 2/],
	]) {
		const { archive, entry } = await releaseAsset(t, { record });
		const root = website(t, [entry]);
		await assert.rejects(assembleVersions(options(root, { fetch: scriptedFetch([() => response(archive)]).fetch })), expected);
		assertNoPartial(root);
	}
});

test('a release that fails its second verification leaves no partial folder', async (t) => {
	const { archive, entry } = await releaseAsset(t);
	const root = website(t, [entry]);
	const verify = async (directory) => {
		assert.ok(path.basename(directory) === '.v0.3.0.partial' && existsSync(directory));
		throw new Error('expected a release label, found none');
	};
	await assert.rejects(assembleVersions(options(root, { fetch: scriptedFetch([() => response(archive)]).fetch, verify })),
		/^Error: v0\.3\.0: verify: expected a release label, found none$/);
	assertNoPartial(root);
	assert.deepEqual(versionsFolder(root), ['index.html']);
});

test('anything already in dist/versions/ besides the development index is a collision', async (t) => {
	const { archive, entry } = await releaseAsset(t);
	for (const extra of ['v0.3.0/index.html', 'stray.txt', '.v0.3.0.partial/index.html']) {
		const root = website(t, [entry]);
		write(path.join(root, 'dist/versions'), extra, 'earlier');
		await assert.rejects(assembleVersions(options(root, { fetch: scriptedFetch([() => response(archive)]).fetch })),
			/development: output: expected dist\/versions\/ to hold only the development index\.html, found/);
	}
});

test('a destination that appears while a release is staged is not replaced', async (t) => {
	const { archive, entry } = await releaseAsset(t);
	const root = website(t, [entry]);
	const verify = async (directory, record) => {
		mkdirSync(path.join(root, 'dist/versions/v0.3.0'));
		return verifySnapshotOutput(directory, record);
	};
	await assert.rejects(assembleVersions(options(root, { fetch: scriptedFetch([() => response(archive)]).fetch, verify })),
		/v0\.3\.0: mount: expected no dist\/versions\/v0\.3\.0, found a directory/);
	assertNoPartial(root);
});

test('a leftover release candidate, a missing development page or a release build is refused', async (t) => {
	let root = website(t);
	write(path.join(root, 'dist'), 'snapshot/index.html', 'candidate');
	await assert.rejects(assembleVersions(options(root)), /expected no dist\/snapshot\/ candidate, found a directory/);
	root = website(t);
	for (const file of ['docs/index.html', 'versions/index.html']) {
		const other = website(t);
		rmSync(path.join(other, 'dist', file));
		await assert.rejects(assembleVersions(options(other)), new RegExp(`expected dist/${file} from astro build, found nothing`));
	}
	await assert.rejects(assembleVersions(options(root, { env: { LABRADOR_DOCS_TAG: 'v0.3.0' } })),
		/expected a development build without LABRADOR_DOCS_TAG, found "v0\.3\.0"/);
	assert.deepEqual(versionsFolder(root), ['index.html']);
});

test('an invalid manifest is refused before anything is fetched', async (t) => {
	const root = website(t);
	write(root, 'published-versions.json', '{"schemaVersion":1,"releases":[],"errata":[]}\n');
	await assert.rejects(assembleVersions(options(root)), /^Error: manifest: read: Manifest: expected canonical text/);
});

test('development search changed during assembly fails the before and after comparison', async (t) => {
	const { archive, entry } = await releaseAsset(t);
	const root = website(t, [entry]);
	const verify = async (directory, record) => {
		write(path.join(root, 'dist'), 'pagefind/pagefind-entry.json', '{}');
		return verifySnapshotOutput(directory, record);
	};
	await assert.rejects(assembleVersions(options(root, { fetch: scriptedFetch([() => response(archive)]).fetch, verify })),
		/assembly: invariants: expected dist\/pagefind\/ unchanged by assembly, found pagefind-entry\.json changed/);
});

test('network errors and 5xx answers are retried; other failures are not', async (t) => {
	const { archive, entry } = await releaseAsset(t);
	const networkError = () => { throw new TypeError('fetch failed'); };
	let root = website(t, [entry]);
	let script = scriptedFetch([networkError, () => response('busy', { status: 503 }), () => response(archive)]);
	const sleeps = [];
	await assembleVersions(options(root, { fetch: script.fetch, sleep: async (ms) => { sleeps.push(ms); } }));
	assert.equal(script.calls.length, 3);
	assert.deepEqual(sleeps, [1000, 2000]);

	root = website(t, [entry]);
	script = scriptedFetch([() => response('busy', { status: 502 })]);
	await assert.rejects(assembleVersions(options(root, { fetch: script.fetch })), /v0\.3\.0: download: expected HTTP 200 from .*, found 502 \(after 4 attempts\)/);
	assert.equal(script.calls.length, 4);

	for (const [answer, expected] of [
		[() => response('gone', { status: 404 }), /download: expected HTTP 200 from .*, found 404$/],
		[() => response(archive, { url: 'http://objects.githubusercontent.com/asset' }), /download: expected the download to end at an https URL, found "http:/],
		[() => response(Buffer.concat([archive, Buffer.alloc(1)])), /download: expected \d+ bytes from .*, found more; the download was aborted$/],
	]) {
		root = website(t, [entry]);
		script = scriptedFetch([answer]);
		await assert.rejects(assembleVersions(options(root, { fetch: script.fetch })), expected);
		assert.equal(script.calls.length, 1);
		assertNoPartial(root);
	}
});
