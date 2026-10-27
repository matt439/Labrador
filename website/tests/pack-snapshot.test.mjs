import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';
import { packSnapshot } from '../scripts/pack-snapshot.mjs';
import { canonicalize, validateManifest } from '../scripts/published-versions.mjs';
import { readArchive } from '../scripts/snapshot-archive.mjs';
import { verifySnapshotOutput } from '../scripts/snapshot-output.mjs';

const TAG = 'v0.3.0';
const SOURCE_DATE = '2026-10-20';
const ORIGIN = 'https://labradorengine.com';
const PREFIX = `/versions/${TAG}/`;
const MANIFEST = '{\n  "schemaVersion": 1,\n  "releases": [],\n  "errata": []\n}\n';
const COMMIT_ENV = {
	...process.env,
	GIT_AUTHOR_DATE: `${SOURCE_DATE}T12:00:00+00:00`,
	GIT_COMMITTER_DATE: `${SOURCE_DATE}T12:00:00+00:00`,
};
const NOW = () => new Date('2026-10-28T23:30:00Z');

function git(root, ...args) {
	return execFileSync('git', args, {
		cwd: root, encoding: 'utf8', env: COMMIT_ENV, stdio: ['ignore', 'pipe', 'pipe'],
	}).trim();
}

function write(root, file, contents) {
	const target = path.join(root, file);
	mkdirSync(path.dirname(target), { recursive: true });
	writeFileSync(target, contents);
}

function temporaryDirectory(t) {
	const root = mkdtempSync(path.join(tmpdir(), 'labrador-pack-snapshot-'));
	t.after(() => {
		const target = realpathSync(root);
		assert.equal(path.dirname(target), realpathSync(tmpdir()));
		assert.ok(path.basename(target).startsWith('labrador-pack-snapshot-'));
		rmSync(target, { recursive: true, force: true });
	});
	return root;
}

function page(route, commit) {
	return `<!doctype html><html><head><link rel="canonical" href="${ORIGIN}${PREFIX}${route}"></head>
	<body><p>Documentation for Labrador ${TAG} <a href="https://github.com/matt439/Labrador/commit/${commit}">Source commit</a></p></body></html>`;
}

// A tagged checkout with a candidate output, as npm run snapshot:build leaves it.
async function candidate(t, { record = true } = {}) {
	const root = temporaryDirectory(t);
	git(root, 'init', '--quiet');
	git(root, 'config', 'user.name', 'Snapshot fixture');
	git(root, 'config', 'user.email', 'snapshot@example.invalid');
	git(root, 'config', 'core.autocrlf', 'false');
	write(root, '.gitignore', 'website/node_modules/\nwebsite/dist/\nwebsite/release-assets/\n');
	write(root, 'vcpkg.json', JSON.stringify({ name: 'labrador', 'version-string': TAG.slice(1) }) + '\n');
	write(root, 'engine/core/example.h', '#pragma once\n');
	write(root, 'website/package-lock.json', '{"lockfileVersion":3}\n');
	write(root, 'website/published-versions.json', MANIFEST);
	git(root, 'add', '.');
	git(root, 'commit', '--quiet', '-m', 'Record pack fixture');
	git(root, 'tag', '-a', TAG, '-m', `Release ${TAG}`);
	const commit = git(root, 'rev-parse', 'HEAD');
	const output = path.join(root, 'website/dist/snapshot');
	write(output, 'index.html', page('', commit));
	write(output, 'docs/index.html', page('docs/', commit));
	write(output, '404.html', page('404/', commit));
	write(output, 'pagefind/pagefind-entry.json', JSON.stringify({ languages: { en: { page_count: 1 } } }));
	write(output, 'pagefind/result.json', JSON.stringify({ url: `${PREFIX}docs/`, content: 'Labrador documentation fixture' }));
	write(output, 'pagefind/pagefind.js', `
		let basePath;
		export async function options(value) { basePath = value.basePath; }
		export async function search() {
			const data = await (await fetch(basePath + 'result.json')).json();
			return { results: [{ data: async () => data }] };
		}
		export async function destroy() {}
	`);
	const source = { schemaVersion: 1, tag: TAG, commit, sourceDate: SOURCE_DATE, snapshotPath: PREFIX };
	const verification = await verifySnapshotOutput(output, source);
	if (record) write(output, 'snapshot-build.json', `${JSON.stringify({ ...source, verification }, null, 2)}\n`);
	return { root, output, commit, source, verification };
}

test('packs the verified candidate and returns the canonical entry for the written asset', async (t) => {
	const { root, commit } = await candidate(t);
	const { entry, file, text } = await packSnapshot(root, TAG, { now: NOW });
	assert.equal(file, path.join(root, 'website/release-assets/labrador-docs-v0.3.0.tar.gz'));
	const bytes = readFileSync(file);
	assert.equal(entry.bytes, bytes.length);
	assert.equal(entry.sha256, createHash('sha256').update(bytes).digest('hex'));
	assert.deepEqual(Object.keys(entry), ['tag', 'commit', 'sourceDate', 'path', 'asset', 'bytes', 'sha256', 'published']);
	assert.deepEqual({ ...entry, bytes: 0, sha256: '' }, {
		tag: TAG, commit, sourceDate: SOURCE_DATE, path: PREFIX, asset: 'labrador-docs-v0.3.0.tar.gz',
		bytes: 0, sha256: '', published: '2026-10-28',
	});
	assert.equal(text, canonicalize(entry));
	assert.deepEqual(JSON.parse(text), entry);
	// The printed entry is exactly what the owner appends to the manifest.
	const manifest = validateManifest(canonicalize({ schemaVersion: 1, releases: [JSON.parse(text)], errata: [] }));
	assert.deepEqual(manifest.releases[0], entry);
	const files = readArchive(bytes, entry);
	assert.deepEqual(files.get('docs/index.html'), readFileSync(path.join(root, 'website/dist/snapshot/docs/index.html')));
	assert.ok(files.has('snapshot-build.json'));
	assert.equal(readFileSync(path.join(root, 'website/published-versions.json'), 'utf8'), MANIFEST, 'the manifest is not edited');
	assert.equal(git(root, 'status', '--porcelain'), '');
});

test('a candidate without its build record is refused', async (t) => {
	const { root } = await candidate(t, { record: false });
	await assert.rejects(packSnapshot(root, TAG, { now: NOW }), /expected the record written by npm run snapshot:build, found ENOENT/);
	assert.equal(existsSync(path.join(root, 'website/release-assets')), false);
});

test('a build record for other source, or without verification, is refused', async (t) => {
	for (const [change, expected] of [
		[(record) => ({ ...record, commit: 'f'.repeat(40) }), /expected the source record/],
		[(record) => ({ ...record, tag: 'v0.4.0', snapshotPath: '/versions/v0.4.0/' }), /expected the source record/],
		[(record) => ({ ...record, extra: true }), /expected the source record/],
		[({ verification, ...record }) => record, /expected a verification result, found undefined/],
	]) {
		const { root, output, source, verification } = await candidate(t);
		write(output, 'snapshot-build.json', JSON.stringify(change({ ...source, verification })));
		await assert.rejects(packSnapshot(root, TAG, { now: NOW }), expected);
		assert.equal(existsSync(path.join(root, 'website/release-assets')), false);
	}
});

test('output changed after its build is refused', async (t) => {
	const { root, output, commit } = await candidate(t);
	write(output, 'docs/extra/index.html', page('docs/extra/', commit));
	await assert.rejects(packSnapshot(root, TAG, { now: NOW }), /the output changed after it was built/);
});

test('an existing asset file is never replaced', async (t) => {
	const { root } = await candidate(t);
	const file = path.join(root, 'website/release-assets/labrador-docs-v0.3.0.tar.gz');
	write(root, 'website/release-assets/labrador-docs-v0.3.0.tar.gz', 'earlier asset');
	await assert.rejects(packSnapshot(root, TAG, { now: NOW }), /expected no existing release asset, found EEXIST; an attached asset is never replaced/);
	assert.equal(readFileSync(file, 'utf8'), 'earlier asset');
});

test('dirty or untagged source is refused before anything is packed', async (t) => {
	const { root } = await candidate(t);
	write(root, 'engine/core/example.h', '#pragma once\n// Local change\n');
	await assert.rejects(packSnapshot(root, TAG, { now: NOW }), /dirty/);
	await assert.rejects(packSnapshot(root, 'v0.4.0', { now: NOW }), Error);
	assert.equal(existsSync(path.join(root, 'website/release-assets')), false);
});

test('the CLI rejects missing and surplus arguments without printing an entry', () => {
	const script = fileURLToPath(new URL('../scripts/pack-snapshot.mjs', import.meta.url));
	for (const args of [[], [TAG, 'unexpected']]) {
		const result = spawnSync(process.execPath, [script, ...args], { encoding: 'utf8' });
		assert.equal(result.status, 1);
		assert.equal(result.stdout, '');
		assert.match(result.stderr, /^Usage: npm run snapshot:pack -- vMAJOR\.MINOR\.PATCH/);
	}
});
