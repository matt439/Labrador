import assert from 'node:assert/strict';
import { mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';
import {
	ARCHIVE_MAX_BYTES, assetUrl, canonicalize, checkAppendOnly, compareTags, readManifest, validateManifest,
} from '../scripts/published-versions.mjs';

const EMPTY = '{\n  "schemaVersion": 1,\n  "releases": [],\n  "errata": []\n}\n';

function release(tag, overrides = {}) {
	return {
		tag,
		commit: 'a'.repeat(40),
		sourceDate: '2026-10-27',
		path: `/versions/${tag}/`,
		asset: `labrador-docs-${tag}.tar.gz`,
		bytes: 1234567,
		sha256: 'b'.repeat(64),
		published: '2026-10-28',
		...overrides,
	};
}

function erratum(tag, overrides = {}) {
	return { tag, date: '2026-11-02', title: 'Correct the pad deadzone default', href: '/docs/errata/pad-deadzone/', ...overrides };
}

function manifest(releases = [], errata = []) {
	return canonicalize({ schemaVersion: 1, releases, errata });
}

function refused(text, pattern) {
	assert.throws(() => validateManifest(text), pattern);
}

test('an empty manifest is valid, and the committed manifest is that exact text', () => {
	assert.deepEqual(validateManifest(EMPTY), { schemaVersion: 1, releases: [], errata: [] });
	const website = fileURLToPath(new URL('../', import.meta.url));
	assert.equal(readFileSync(path.join(website, 'published-versions.json'), 'utf8'), EMPTY);
	assert.deepEqual(readManifest(website), { schemaVersion: 1, releases: [], errata: [] });
});

test('a full manifest validates to a frozen copy in canonical order', () => {
	const text = manifest([release('v0.3.0'), release('v0.4.0', { commit: 'c'.repeat(64) })], [erratum('v0.3.0')]);
	const value = validateManifest(text);
	assert.equal(value.releases.length, 2);
	assert.equal(value.errata[0].tag, 'v0.3.0');
	assert.ok(Object.isFrozen(value) && Object.isFrozen(value.releases[0]));
	assert.deepEqual(Object.keys(value.releases[0]),
		['tag', 'commit', 'sourceDate', 'path', 'asset', 'bytes', 'sha256', 'published']);
	assert.equal(canonicalize(value), text);
});

test('canonicalize orders keys and ends with one newline', () => {
	assert.equal(canonicalize({ errata: [], releases: [], schemaVersion: 1 }), EMPTY);
	assert.equal(canonicalize({ href: '/x/', tag: 'v0.3.0', title: 't', date: '2026-01-01' }),
		'{\n  "tag": "v0.3.0",\n  "date": "2026-01-01",\n  "title": "t",\n  "href": "/x/"\n}\n');
});

test('non-canonical bytes are refused', () => {
	refused(EMPTY.replace(/\n/g, '\r\n'), /LF line endings/);
	refused(`﻿${EMPTY}`, /byte-order mark/);
	refused('{\n  "releases": [],\n  "schemaVersion": 1,\n  "errata": []\n}\n', /canonical text.*line 2/);
	refused(EMPTY.trimEnd(), /canonical text/);
	refused(`${EMPTY}\n`, /canonical text/);
	refused('{"schemaVersion":1,"releases":[],"errata":[]}\n', /canonical text/);
	refused('{\n  "schemaVersion": 1,\n  "schemaVersion": 1,\n  "releases": [],\n  "errata": []\n}\n', /canonical text/);
	refused(manifest([release('v0.3.0')]).replace('"tag": "v0.3.0",', '"tag": "v0.3.0",\n      "tag": "v0.3.0",'), /canonical text/);
	refused('{ not json', /expected JSON/);
});

test('extra, missing and mistyped keys are refused', () => {
	refused(canonicalize({ schemaVersion: 1, releases: [], errata: [], latest: 'v0.3.0' }), /only the keys.*found latest/);
	refused(manifest([{ ...release('v0.3.0'), url: 'https://example.com/a.tar.gz' }]), /releases\[0\]: expected only the keys.*found url/);
	refused(manifest([release('v0.3.0')], [{ ...erratum('v0.3.0'), severity: 'high' }]), /errata\[0\]: expected only the keys/);
	const { sha256: _sha256, ...withoutHash } = release('v0.3.0');
	refused(manifest([withoutHash]), /missing sha256/);
	refused(canonicalize({ schemaVersion: 1, releases: [] }), /missing errata/);
	refused(canonicalize({ schemaVersion: 2, releases: [], errata: [] }), /schemaVersion: expected 1, found 2/);
	refused(canonicalize({ schemaVersion: 1, releases: {}, errata: [] }), /releases: expected an array/);
	refused(canonicalize([]), /Manifest: expected an object/);
});

test('fields are checked against their formats', () => {
	refused(manifest([release('v0.3.0', { commit: 'A'.repeat(40) })]), /commit: expected a full lowercase commit/);
	refused(manifest([release('v0.3.0', { commit: 'a'.repeat(12) })]), /commit/);
	refused(manifest([release('v0.3.0', { sha256: 'b'.repeat(63) })]), /sha256: expected 64 lowercase hex/);
	refused(manifest([release('v0.3.0', { sourceDate: '2026-02-30' })]), /sourceDate: expected a calendar date/);
	refused(manifest([release('v0.3.0', { published: '28/10/2026' })]), /published: expected a calendar date/);
	refused(manifest([release('v0.3.0', { bytes: 0 })]), /bytes: expected an integer/);
	refused(manifest([release('v0.3.0', { bytes: 1.5 })]), /bytes: expected an integer/);
	refused(manifest([release('v0.3.0', { bytes: '12' })]), /bytes: expected an integer/);
	refused(manifest([release('v0.3.0', { bytes: ARCHIVE_MAX_BYTES + 1 })]), /bytes: expected an integer from 1 to 268435456/);
	assert.equal(validateManifest(manifest([release('v0.3.0', { bytes: ARCHIVE_MAX_BYTES })])).releases[0].bytes, ARCHIVE_MAX_BYTES);
});

test('a path or asset not derived from the tag is refused', () => {
	refused(manifest([release('v0.3.0', { path: '/versions/v0.4.0/' })]), /path: expected "\/versions\/v0.3.0\/", found "\/versions\/v0.4.0\/"/);
	refused(manifest([release('v0.3.0', { path: '/versions/v0.3.0' })]), /path: expected/);
	refused(manifest([release('v0.3.0', { asset: 'labrador-docs-v0.3.0.zip' })]), /asset: expected "labrador-docs-v0.3.0.tar.gz"/);
	refused(manifest([release('v0.3.0', { asset: '../v0.4.0/labrador-docs-v0.4.0.tar.gz' })]), /asset: expected/);
});

test('a tag below v0.3.0, or one that is not stable, is refused', () => {
	refused(manifest([release('v0.2.9')]), /releases\[0\]\.tag: Documentation snapshots start at v0\.3\.0/);
	refused(manifest([release('v0.0.1')]), /start at v0\.3\.0/);
	for (const tag of ['v0.3.0-rc.1', 'v0.3', '0.3.0', 'v00.3.0', 'v0.3.0+build', 'V0.3.0', ' v0.3.0']) {
		refused(manifest([release(tag)]), /stable release tag/);
	}
	refused(manifest([release(3)]), /stable release tag/);
	assert.equal(validateManifest(manifest([release('v1.0.0'), release('v0.10.0')])).releases.length, 2);
});

test('duplicate tags are refused', () => {
	refused(manifest([release('v0.3.0'), release('v0.4.0'), release('v0.3.0', { sha256: 'c'.repeat(64) })]),
		/releases\[2\]\.tag: expected a tag not already listed, found v0\.3\.0 again after releases\[0\]/);
});

test('errata must name a listed release and link inside the development site or the repository', () => {
	const releases = [release('v0.3.0')];
	refused(manifest(releases, [erratum('v0.4.0')]), /errata\[0\]\.tag: expected a listed release, found "v0\.4\.0"/);
	refused(manifest([], [erratum('v0.3.0')]), /expected a listed release/);
	for (const href of [
		'/versions/v0.3.0/docs/', '/versions/', '/versions', '/versions#x',
		'https://example.com/errata/', '//example.com/errata/', 'https://github.com/someone/Labrador/issues/1',
		'https://github.com/matt439/Labrador.evil/x', 'https://github.com/matt439/Labrador/../Other/x',
		'https://user@github.com/matt439/Labrador/issues/1', 'http://github.com/matt439/Labrador/issues/1',
		'docs/errata/', '/docs/../versions/v0.3.0/', '/docs//errata/', '/docs/errata/?x=1', 'javascript:alert(1)',
		'/docs\\errata/', '',
	]) {
		refused(manifest(releases, [erratum('v0.3.0', { href })]), /href: expected a site path outside \/versions\//);
	}
	for (const href of ['/docs/errata/pad-deadzone/', '/docs/errata/#v0-3-0', '/', 'https://github.com/matt439/Labrador/issues/42']) {
		assert.equal(validateManifest(manifest(releases, [erratum('v0.3.0', { href })])).errata[0].href, href);
	}
	refused(manifest(releases, [erratum('v0.3.0', { title: '' })]), /title: expected a non-empty single-line title/);
	refused(manifest(releases, [erratum('v0.3.0', { title: 'two\nlines' })]), /title/);
	refused(manifest(releases, [erratum('v0.3.0', { date: '2026-13-01' })]), /date: expected a calendar date/);
});

test('assetUrl derives the GitHub release download from the tag', () => {
	assert.equal(assetUrl(release('v0.3.0')),
		'https://github.com/matt439/Labrador/releases/download/v0.3.0/labrador-docs-v0.3.0.tar.gz');
});

test('compareTags orders by numeric semantic version', () => {
	const sorted = ['v0.10.0', 'v0.3.1', 'v1.0.0', 'v0.3.0', 'v0.4.0', 'v0.0.9'].sort(compareTags);
	assert.deepEqual(sorted, ['v0.0.9', 'v0.3.0', 'v0.3.1', 'v0.4.0', 'v0.10.0', 'v1.0.0']);
	assert.equal(compareTags('v0.3.0', 'v0.3.0'), 0);
	assert.equal(compareTags('v12345678901234567890.0.0', 'v12345678901234567889.9.9'), 1);
	assert.throws(() => compareTags('v0.3.0-rc.1', 'v0.3.0'), /stable release tag/);
});

test('readManifest validates the file and names it on read failure', (t) => {
	const website = mkdtempSync(path.join(tmpdir(), 'labrador-published-versions-'));
	t.after(() => rmSync(realpathSync(website), { recursive: true, force: true }));
	assert.throws(() => readManifest(website), /published-versions\.json: expected a readable UTF-8 manifest, found ENOENT/);
	writeFileSync(path.join(website, 'published-versions.json'), Buffer.from([0x7b, 0xff, 0x7d, 0x0a]));
	assert.throws(() => readManifest(website), /expected a readable UTF-8 manifest/);
	writeFileSync(path.join(website, 'published-versions.json'), `﻿${EMPTY}`);
	assert.throws(() => readManifest(website), /byte-order mark/);
	writeFileSync(path.join(website, 'published-versions.json'), EMPTY.replace(/\n/g, '\r\n'));
	assert.throws(() => readManifest(website), /LF line endings/);
	const text = manifest([release('v0.3.0')]);
	writeFileSync(path.join(website, 'published-versions.json'), text);
	assert.equal(readManifest(website).releases[0].tag, 'v0.3.0');
});

const BASE = validateManifest(manifest([release('v0.3.0'), release('v0.4.0')], [erratum('v0.3.0')]));

function next(releases, errata = BASE.errata) {
	return validateManifest(manifest(releases, errata));
}

test('checkAppendOnly accepts appended releases, backports and errata', () => {
	assert.doesNotThrow(() => checkAppendOnly(validateManifest(EMPTY), BASE));
	assert.doesNotThrow(() => checkAppendOnly(BASE, BASE));
	assert.doesNotThrow(() => checkAppendOnly(BASE, next([...BASE.releases, release('v0.5.0')])));
	assert.doesNotThrow(() => checkAppendOnly(BASE, next([...BASE.releases, release('v0.3.1')])));
	assert.doesNotThrow(() => checkAppendOnly(BASE, next(BASE.releases, [...BASE.errata, erratum('v0.4.0'), erratum('v0.3.0')])));
});

test('checkAppendOnly refuses removal, reordering, edits and insertion', () => {
	assert.throws(() => checkAppendOnly(BASE, next([BASE.releases[0]])),
		/releases: expected at least the 2 published item\(s\), found 1/);
	assert.throws(() => checkAppendOnly(BASE, next([BASE.releases[1], BASE.releases[0]])),
		/releases\[0\]: expected the published item unchanged/);
	assert.throws(() => checkAppendOnly(BASE, next([BASE.releases[0], release('v0.4.0', { sha256: 'c'.repeat(64) })])),
		/releases\[1\]: expected the published item unchanged .*"sha256":"b{64}".*found .*"sha256":"c{64}"/);
	assert.throws(() => checkAppendOnly(BASE, next([BASE.releases[0], release('v0.4.0', { published: '2026-10-29' })])),
		/releases\[1\]: expected the published item unchanged/);
	assert.throws(() => checkAppendOnly(BASE, next([BASE.releases[0], release('v0.3.1'), BASE.releases[1]])),
		/releases\[2\]\.tag: expected a tag never published before, found v0\.4\.0, published at releases\[1\]/);
	assert.throws(() => checkAppendOnly(BASE, next(BASE.releases, [])),
		/errata: expected at least the 1 published item/);
});

test('checkAppendOnly refuses a removed tag added again', () => {
	// Removal and re-addition in one change: the tag reappears past the old end.
	assert.throws(() => checkAppendOnly(BASE, next([BASE.releases[1], release('v0.5.0'), BASE.releases[0]])),
		/releases\[2\]\.tag: expected a tag never published before, found v0\.3\.0, published at releases\[0\]/);
	// Unvalidated input still cannot republish a tag at the end.
	const repeated = { schemaVersion: 1, releases: [...BASE.releases, BASE.releases[0]], errata: BASE.errata };
	assert.throws(() => checkAppendOnly(BASE, repeated), /found v0\.3\.0, published at releases\[0\]/);
});

test('checkAppendOnly refuses edited, removed or inserted errata', () => {
	assert.throws(() => checkAppendOnly(BASE, next(BASE.releases, [erratum('v0.3.0', { title: 'Reworded' })])),
		/errata\[0\]: expected the published item unchanged/);
	assert.throws(() => checkAppendOnly(BASE, next(BASE.releases, [erratum('v0.3.0', { href: '/docs/errata/other/' })])),
		/errata\[0\]: expected the published item unchanged/);
	assert.throws(() => checkAppendOnly(BASE, next(BASE.releases, [erratum('v0.4.0'), ...BASE.errata])),
		/errata\[0\]: expected the published item unchanged/);
});

test('checkAppendOnly refuses a schema change', () => {
	assert.throws(() => checkAppendOnly(BASE, { ...BASE, schemaVersion: 2 }), /schemaVersion: expected 1, found 2/);
});
