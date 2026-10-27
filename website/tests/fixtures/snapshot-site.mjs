// Small built sites for the assembly and publication tests: a release
// snapshot that passes verifySnapshotOutput, packed as its release asset, and a
// development dist/ shaped like the output of astro build.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdirSync, mkdtempSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { packTree } from '../../scripts/snapshot-archive.mjs';
import { verifySnapshotOutput } from '../../scripts/snapshot-output.mjs';

const ORIGIN = 'https://labradorengine.com';
export const RELEASE_PAGES = ['', 'docs/', 'docs/guides/one/'];
export const DEVELOPMENT_PAGES = ['', 'docs/', 'docs/guides/one/', 'docs/guides/two/'];

export function temporaryDirectory(t, prefix) {
	const root = mkdtempSync(path.join(tmpdir(), prefix));
	t.after(() => {
		const target = realpathSync(root);
		assert.equal(path.dirname(target), realpathSync(tmpdir()));
		assert.ok(path.basename(target).startsWith(prefix));
		rmSync(target, { recursive: true, force: true });
	});
	return root;
}

export function write(root, file, contents) {
	const target = path.join(root, file);
	mkdirSync(path.dirname(target), { recursive: true });
	writeFileSync(target, contents);
}

function file(route) {
	return route === '' ? 'index.html' : `${route}index.html`;
}

function releasePage(prefix, route, tag, commit) {
	return `<!doctype html><html><head><link rel="canonical" href="${ORIGIN}${prefix}${route}"></head>
	<body><p>Documentation for Labrador ${tag} <a href="https://github.com/matt439/Labrador/commit/${commit}">Source commit</a></p>
	<a href="/versions/" data-version-index>All documentation versions</a></body></html>`;
}

const PAGEFIND = `
	let basePath;
	export async function options(value) { basePath = value.basePath; }
	export async function search() {
		const data = await (await fetch(basePath + 'result.json')).json();
		return { results: [{ data: async () => data }] };
	}
	export async function destroy() {}
`;

/**
 * Build, verify and pack a release snapshot. record changes the
 * snapshot-build.json written into it; the entry always describes source.
 */
export async function releaseAsset(t, {
	tag = 'v0.3.0', commit = 'a'.repeat(40), sourceDate = '2026-10-20', published = '2026-10-21',
	record = (value) => value,
} = {}) {
	const directory = temporaryDirectory(t, 'labrador-release-fixture-');
	const prefix = `/versions/${tag}/`;
	for (const route of RELEASE_PAGES) write(directory, file(route), releasePage(prefix, route, tag, commit));
	write(directory, '404.html', releasePage(prefix, '404/', tag, commit));
	write(directory, 'pagefind/pagefind-entry.json', JSON.stringify({ languages: { en: { page_count: 1 } } }));
	write(directory, 'pagefind/result.json', JSON.stringify({ url: `${prefix}docs/`, content: 'Labrador documentation fixture' }));
	write(directory, 'pagefind/pagefind.js', PAGEFIND);
	const source = { schemaVersion: 1, tag, commit, sourceDate, snapshotPath: prefix };
	const verification = await verifySnapshotOutput(directory, source);
	write(directory, 'snapshot-build.json', `${JSON.stringify(record({ ...source, verification }), null, 2)}\n`);
	const archive = packTree(directory);
	const entry = {
		tag, commit, sourceDate, path: prefix, asset: `labrador-docs-${tag}.tar.gz`,
		bytes: archive.length, sha256: createHash('sha256').update(archive).digest('hex'), published,
	};
	return { archive, entry, source };
}

/** A development dist/ as astro build leaves it, with its own search index. */
export function developmentDist(dist, { commit = 'c'.repeat(40) } = {}) {
	const revision = `<p class="site-revision"><span class="revision">Development documentation (unreleased).
		Built from Labrador <a href="https://github.com/matt439/Labrador/commit/${commit}"><code>${commit.slice(0, 7)}</code></a> of 2026-10-27.</span></p>`;
	for (const route of DEVELOPMENT_PAGES) {
		write(dist, file(route), `<!doctype html><html><body><p>Development</p>${revision}</body></html>`);
	}
	write(dist, '404.html', `<!doctype html><html><body>${revision}</body></html>`);
	write(dist, 'versions/index.html', `<!doctype html><html><body>${revision}</body></html>`);
	write(dist, 'pagefind/pagefind.js', 'export {};');
	write(dist, 'pagefind/pagefind-entry.json', JSON.stringify({ languages: { en: { page_count: 4 } } }));
	write(dist, '_astro/site.css', 'body { color: black; }');
	return dist;
}

/** A fetch stub that answers each call with the next response factory. */
export function scriptedFetch(responses) {
	const calls = [];
	const fetch = async (url, options) => {
		calls.push({ url: String(url), options });
		const next = responses[Math.min(calls.length - 1, responses.length - 1)];
		return next(url, options);
	};
	return { fetch, calls };
}

/** A response like the one fetch gives for a download that ended at url. */
export function response(body, { status = 200, url = 'https://objects.githubusercontent.com/asset', headers = {} } = {}) {
	const result = new Response(body, { status, headers });
	Object.defineProperty(result, 'url', { value: url });
	return result;
}
