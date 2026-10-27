// Mount every published release snapshot into the development build's dist/
// and write /versions/index.json, the one file every version's selector reads.
// Each asset is downloaded from its GitHub release, checked against the
// manifest and verified again before it is renamed into place. Any failure
// exits non-zero so the host does not deploy and the previous site stays live.
import { createHash } from 'node:crypto';
import { lstatSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';
import { parse } from 'parse5';
import { assetUrl, compareTags, readManifest } from './published-versions.mjs';
import { extractArchive, readArchive } from './snapshot-archive.mjs';
import { pagePath, verifySnapshotOutput } from './snapshot-output.mjs';

// The host's limit on files in one deployment.
export const ASSET_FILE_LIMIT = 100_000;
export const FETCH_TIMEOUT_MS = 120_000;
export const FETCH_RETRIES = 3;

const COMMIT_LINK = /^https:\/\/github\.com\/matt439\/Labrador\/commit\/([0-9a-f]{40}|[0-9a-f]{64})$/;

// Only a failure that a second attempt could change is retried.
class TransientError extends Error {}

function describeError(error) {
	return error instanceof Error ? error.message : String(error);
}

async function step(tag, name, action) {
	try {
		return await action();
	} catch (error) {
		throw new Error(`${tag}: ${name}: ${describeError(error)}`, { cause: error });
	}
}

function kind(filename) {
	try {
		const stat = lstatSync(filename);
		if (stat.isSymbolicLink()) return 'a symbolic link';
		if (stat.isDirectory()) return 'a directory';
		return stat.isFile() ? 'a file' : 'a special entry';
	} catch (error) {
		if (error.code === 'ENOENT') return 'nothing';
		throw error;
	}
}

function walk(directory, visit, relative = '') {
	for (const entry of readdirSync(path.join(directory, relative), { withFileTypes: true })) {
		const name = relative ? `${relative}/${entry.name}` : entry.name;
		if (entry.isSymbolicLink()) throw new Error(`expected regular files, found a symbolic link at ${name}`);
		if (entry.isDirectory()) walk(directory, visit, name);
		else if (entry.isFile()) visit(name);
		else throw new Error(`expected regular files, found a special entry at ${name}`);
	}
}

function byteCompare(a, b) {
	return Buffer.compare(Buffer.from(a, 'utf8'), Buffer.from(b, 'utf8'));
}

/**
 * The routes of a built site's pages, in byte order, as the selector matches
 * them: the same mapping verifySnapshotOutput uses, without the 404 page and
 * without any folder named in skip.
 */
export function pageList(directory, { skip = [] } = {}) {
	const pages = [];
	walk(directory, (name) => {
		if (!name.endsWith('.html') || name === '404.html') return;
		if (skip.some((folder) => name.startsWith(`${folder}/`))) return;
		pages.push(pagePath(name));
	});
	return pages.sort(byteCompare);
}

function treeHashes(directory) {
	const hashes = [];
	if (kind(directory) === 'nothing') return hashes;
	walk(directory, (name) => {
		hashes.push(`${name}\0${createHash('sha256').update(readFileSync(path.join(directory, name))).digest('hex')}`);
	});
	return hashes.sort(byteCompare);
}

function countFiles(directory) {
	let count = 0;
	walk(directory, () => { count += 1; });
	return count;
}

// The development build names its commit in the revision line of every page.
// Reading it from the output keeps the index true to what was actually built.
function developmentCommit(dist) {
	const document = parse(readFileSync(path.join(dist, 'index.html'), 'utf8'));
	const links = [];
	(function visit(node, inRevision) {
		const attrs = Object.fromEntries((node.attrs ?? []).map((attribute) => [attribute.name, attribute.value]));
		const revision = inRevision || (attrs.class ?? '').split(/\s+/).includes('revision');
		if (revision && node.tagName === 'a' && attrs.href) links.push(attrs.href);
		for (const child of node.childNodes ?? []) visit(child, revision);
		if (node.content) visit(node.content, revision);
	})(document, false);
	const match = links.length === 1 ? COMMIT_LINK.exec(links[0]) : null;
	// A host may supply a commit that is not a full hash; the field is optional.
	return match ? match[1] : undefined;
}

async function download(url, bytes, fetch) {
	const controller = new AbortController();
	const signal = AbortSignal.any([controller.signal, AbortSignal.timeout(FETCH_TIMEOUT_MS)]);
	let response;
	try {
		response = await fetch(url, { redirect: 'follow', signal });
	} catch (error) {
		throw new TransientError(`expected a response from ${url}, found ${describeError(error)}`);
	}
	if (response.status >= 500 && response.status <= 599) {
		throw new TransientError(`expected HTTP 200 from ${url}, found ${response.status}`);
	}
	if (response.status !== 200) throw new Error(`expected HTTP 200 from ${url}, found ${response.status}`);
	let final;
	try { final = new URL(response.url); } catch { final = null; }
	if (final?.protocol !== 'https:') {
		throw new Error(`expected the download to end at an https URL, found ${JSON.stringify(response.url)}`);
	}
	if (!response.body) throw new Error(`expected a response body from ${url}, found none`);
	const chunks = [];
	let length = 0;
	try {
		for await (const chunk of response.body) {
			length += chunk.length;
			if (length > bytes) {
				controller.abort();
				throw new Error(`expected ${bytes} bytes from ${url}, found more; the download was aborted`);
			}
			chunks.push(chunk);
		}
	} catch (error) {
		if (length > bytes) throw error;
		throw new TransientError(`expected ${bytes} bytes from ${url}, found the transfer failed after ${length}: ${describeError(error)}`);
	}
	return Buffer.concat(chunks, length);
}

/**
 * The attached asset's bytes, from cacheDir/<asset> when that file exists and
 * otherwise from its GitHub release download. Network errors and 5xx answers
 * are retried; the caller's hash check is never a reason to retry.
 */
export async function fetchArchive(entry, { fetch = globalThis.fetch, cacheDir, sleep = delay } = {}) {
	if (cacheDir) {
		const cached = path.join(cacheDir, entry.asset);
		const found = kind(cached);
		if (found === 'a file') return readFileSync(cached);
		if (found !== 'nothing') throw new Error(`expected ${cached} to be a cached asset file, found ${found}`);
	}
	const url = assetUrl(entry);
	for (let attempt = 0; ; attempt += 1) {
		try {
			return await download(url, entry.bytes, fetch);
		} catch (error) {
			if (!(error instanceof TransientError) || attempt === FETCH_RETRIES) {
				throw new Error(attempt ? `${error.message} (after ${attempt + 1} attempts)` : error.message, { cause: error });
			}
			await sleep(1000 * 2 ** attempt);
		}
	}
}

function sourceRecord(entry) {
	return { schemaVersion: 1, tag: entry.tag, commit: entry.commit, sourceDate: entry.sourceDate, snapshotPath: entry.path };
}

function checkBuildRecord(files, entry) {
	let record;
	try {
		record = JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(files.get('snapshot-build.json')));
	} catch (error) {
		throw new Error(`expected snapshot-build.json to be UTF-8 JSON, found ${describeError(error)}`);
	}
	const expected = sourceRecord(entry);
	for (const key of Object.keys(expected)) {
		if (record?.[key] !== expected[key]) {
			throw new Error(`expected snapshot-build.json ${key} ${JSON.stringify(expected[key])}, found ${JSON.stringify(record?.[key])}`);
		}
	}
}

/**
 * Check an asset's bytes against its manifest entry, extract it into
 * <parent>/.<tag>.partial/ and verify the extracted site again. Returns the
 * partial folder, which the caller renames into place; on failure no partial
 * folder is left behind.
 */
export async function stageRelease(entry, bytes, parent, { verify = verifySnapshotOutput } = {}) {
	const { tag } = entry;
	const files = await step(tag, 'read archive', () => readArchive(bytes, entry));
	await step(tag, 'build record', () => checkBuildRecord(files, entry));
	const partial = await step(tag, 'extract', () => extractArchive(files, parent, tag));
	try {
		await step(tag, 'verify', () => verify(partial, sourceRecord(entry)));
	} catch (error) {
		rmSync(partial, { recursive: true, force: true });
		throw error;
	}
	return partial;
}

function sameList(a, b) {
	return a.length === b.length && a.every((item, index) => item === b[index]);
}

/**
 * Mount the manifest's releases below dist/versions/ and write its index.
 * Nothing is fetched when the manifest lists no release.
 */
export async function assembleVersions({
	website,
	dist = path.join(website, 'dist'),
	fetch = globalThis.fetch,
	cacheDir = process.env.LABRADOR_SNAPSHOT_CACHE,
	verify = verifySnapshotOutput,
	env = process.env,
	sleep = delay,
	warn = (message) => process.stderr.write(`${message}\n`),
} = {}) {
	if (env.LABRADOR_DOCS_TAG !== undefined) {
		throw new Error(`assemble: expected a development build without LABRADOR_DOCS_TAG, found ${JSON.stringify(env.LABRADOR_DOCS_TAG)}; release snapshots are never assembled.`);
	}
	const manifest = await step('manifest', 'read', () => readManifest(website));
	const versions = path.join(dist, 'versions');
	await step('development', 'output', () => {
		for (const file of ['index.html', 'docs/index.html', 'versions/index.html']) {
			const found = kind(path.join(dist, file));
			if (found !== 'a file') throw new Error(`expected dist/${file} from astro build, found ${found}`);
		}
		const snapshot = kind(path.join(dist, 'snapshot'));
		if (snapshot !== 'nothing') throw new Error(`expected no dist/snapshot/ candidate, found ${snapshot}; rebuild without LABRADOR_DOCS_TAG`);
		const entries = readdirSync(versions).sort(byteCompare);
		if (!sameList(entries, ['index.html'])) {
			throw new Error(`expected dist/versions/ to hold only the development index.html, found ${entries.join(', ')}`);
		}
	});
	const searchBefore = await step('development', 'search', () => treeHashes(path.join(dist, 'pagefind')));
	const development = {
		id: 'development',
		label: 'Development',
		path: '/',
		commit: developmentCommit(dist),
		pages: await step('development', 'pages', () => pageList(dist, { skip: ['versions'] })),
	};
	if (development.commit === undefined) delete development.commit;

	const mounted = new Map();
	try {
		for (const entry of manifest.releases) {
			const bytes = await step(entry.tag, 'download', () => fetchArchive(entry, { fetch, cacheDir, sleep }));
			const partial = await stageRelease(entry, bytes, versions, { verify });
			const destination = path.join(versions, entry.tag);
			await step(entry.tag, 'mount', () => {
				const found = kind(destination);
				if (found !== 'nothing') throw new Error(`expected no dist/versions/${entry.tag}, found ${found}`);
				renameSync(partial, destination);
			});
			mounted.set(entry.tag, await step(entry.tag, 'pages', () => pageList(destination)));
		}
	} catch (error) {
		for (const name of readdirSync(versions)) {
			if (/^\..+\.partial$/.test(name)) rmSync(path.join(versions, name), { recursive: true, force: true });
		}
		throw error;
	}

	const releases = [...manifest.releases].sort((a, b) => compareTags(b.tag, a.tag));
	const index = {
		schemaVersion: 1,
		versions: [development, ...releases.map((entry, position) => ({
			id: entry.tag,
			label: entry.tag,
			path: entry.path,
			commit: entry.commit,
			published: entry.published,
			latest: position === 0,
			sha256: entry.sha256,
			pages: mounted.get(entry.tag),
		}))],
		publishedOrder: manifest.releases.map((entry) => entry.tag),
	};
	writeFileSync(path.join(versions, 'index.json'), `${JSON.stringify(index)}\n`, { flag: 'wx' });

	const files = await step('assembly', 'invariants', () => {
		const expected = [...manifest.releases.map((entry) => entry.tag), 'index.html', 'index.json'].sort(byteCompare);
		const found = readdirSync(versions).sort(byteCompare);
		if (!sameList(found, expected)) throw new Error(`expected dist/versions/ to hold ${expected.join(', ')}, found ${found.join(', ')}`);
		for (const entry of manifest.releases) {
			const type = kind(path.join(versions, entry.tag));
			if (type !== 'a directory') throw new Error(`expected dist/versions/${entry.tag} to be a directory, found ${type}`);
		}
		const searchAfter = treeHashes(path.join(dist, 'pagefind'));
		if (!sameList(searchBefore, searchAfter)) {
			const changed = [...searchBefore.filter((line) => !searchAfter.includes(line)), ...searchAfter.filter((line) => !searchBefore.includes(line))];
			throw new Error(`expected dist/pagefind/ unchanged by assembly, found ${[...new Set(changed.map((line) => line.split('\0')[0]))].join(', ')} changed`);
		}
		const count = countFiles(dist);
		if (count > ASSET_FILE_LIMIT) throw new Error(`expected at most ${ASSET_FILE_LIMIT} files in dist/, found ${count}`);
		if (count > ASSET_FILE_LIMIT * 0.8) warn(`Warning: dist/ holds ${count} files, more than 80% of the ${ASSET_FILE_LIMIT}-file deployment limit.`);
		return count;
	});
	return { index, files };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		if (process.argv.length !== 2) throw new Error('Usage: node scripts/assemble-versions.mjs');
		const website = fileURLToPath(new URL('../', import.meta.url));
		const { index, files } = await assembleVersions({ website });
		process.stdout.write(`Assembled ${index.versions.length - 1} release snapshot(s) into dist/versions/ (${files} files).\n`);
	} catch (error) {
		process.stderr.write(`${describeError(error)}\n`);
		process.exitCode = 1;
	}
}
