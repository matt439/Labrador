// CI's check of a change to published-versions.json. It needs full Git history
// and the network, so it is never part of npm test or of the host's build.
// Before the site is built: the manifest may only grow by appending, each
// appended release must be an immutable GitHub release whose assets are the
// pinned bytes of its tag's snapshot and the release's starter ZIP, which Get
// Started links for that release, and the live site's index must agree
// with this manifest wherever both list a release. After the build (--dist):
// each erratum this change appends must link to something that exists.
import { execFileSync } from 'node:child_process';
import { lstatSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';
import { parse } from 'parse5';
import { FETCH_RETRIES, fetchArchive, pageList, stageRelease } from './assemble-versions.mjs';
import { canonicalize, checkAppendOnly, readManifest, validateManifest } from './published-versions.mjs';
import { verifySnapshotOutput } from './snapshot-output.mjs';

const MANIFEST_PATH = 'website/published-versions.json';
const REPOSITORY_URL = 'https://github.com/matt439/Labrador/';
const RELEASES_API = 'https://api.github.com/repos/matt439/Labrador/releases/tags/';
export const LIVE_INDEX = 'https://labradorengine.com/versions/index.json';
const REQUEST_TIMEOUT_MS = 30_000;
const FULL_COMMIT = /^(?:[0-9a-f]{40}|[0-9a-f]{64})$/;
const EMPTY_MANIFEST = canonicalize({ schemaVersion: 1, releases: [], errata: [] });
const USAGE = 'Usage: npm run versions:verify -- --base <commit> [--dist <directory>]';

function describeError(error) {
	return error instanceof Error ? error.message : String(error);
}

/** Report a problem that does not fail the run; under GitHub Actions it is a warning annotation. */
export function annotateWarning(message) {
	if (process.env.GITHUB_ACTIONS === 'true') {
		process.stderr.write(`::warning::${message.replace(/%/g, '%25').replace(/\r/g, '%0D').replace(/\n/g, '%0A')}\n`);
	} else {
		process.stderr.write(`warning: ${message}\n`);
	}
}

function logLine(message) {
	process.stderr.write(`${message}\n`);
}

/** Run git in root with stdout returned exactly, isolated from the caller's repository routing. */
export function gitIn(root) {
	const environment = { ...process.env };
	for (const key of Object.keys(environment)) {
		if (/^GIT_/i.test(key)) delete environment[key];
	}
	environment.GIT_NO_REPLACE_OBJECTS = '1';
	environment.GIT_OPTIONAL_LOCKS = '0';
	return (args) => execFileSync('git', ['-C', root, ...args], {
		encoding: 'utf8', env: environment, stdio: ['ignore', 'pipe', 'pipe'],
	});
}

function revParse(commit, git) {
	let resolved;
	try {
		resolved = git(['rev-parse', '--verify', '--quiet', `${commit}^{commit}`]).trim();
	} catch {
		resolved = '';
	}
	return FULL_COMMIT.test(resolved) ? resolved : null;
}

/**
 * The commit to compare against. A push that creates a branch reports the zero
 * SHA as its previous commit, and a manual run has none, so both compare with
 * where the change left master. So does a named base this checkout lacks, such
 * as the old tip a force-push reports, which no fetched ref reaches any more.
 */
export function resolveBase(base, git, { log = logLine } = {}) {
	const given = base ?? '';
	if (given !== '' && !/^0+$/.test(given)) {
		const resolved = revParse(given, git);
		if (resolved) return resolved;
		log(`base: ${JSON.stringify(given)} is not a commit in this checkout, as after a force-push; comparing with git merge-base HEAD origin/master instead.`);
	}
	let commit;
	try {
		commit = git(['merge-base', 'HEAD', 'origin/master']).trim();
	} catch (error) {
		throw new Error(`base: expected git merge-base HEAD origin/master to name a commit, found it failed: ${describeError(error)}; check out full history.`);
	}
	const resolved = revParse(commit, git);
	if (!resolved) {
		throw new Error(`base: expected a commit present in this checkout, found ${JSON.stringify(commit)}; check out full history.`);
	}
	return resolved;
}

/** The manifest at a commit, or an empty manifest when that commit has none. */
export function manifestAt(commit, git) {
	const listed = git(['ls-tree', '--name-only', commit, '--', MANIFEST_PATH]).trim();
	const text = listed === '' ? EMPTY_MANIFEST : git(['show', `${commit}:${MANIFEST_PATH}`]);
	try {
		return validateManifest(text);
	} catch (error) {
		throw new Error(`base ${commit}: ${describeError(error)}`, { cause: error });
	}
}

async function request(fetch, url, options = {}) {
	try {
		return await fetch(url, { ...options, signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS) });
	} catch (error) {
		throw new Error(`expected a response from ${url}, found ${describeError(error)}`);
	}
}

/**
 * A request whose network errors, timeouts, 5xx and 429 answers are retried
 * with fetchArchive's backoff. Any other answer is returned at once, and so is
 * the last answer when every attempt was one that could be retried.
 */
async function requestRetried(fetch, url, options, sleep) {
	for (let attempt = 0; ; attempt += 1) {
		let response = null;
		try {
			response = await request(fetch, url, options);
		} catch (error) {
			if (attempt === FETCH_RETRIES) throw new Error(`${error.message} (after ${attempt + 1} attempts)`, { cause: error });
		}
		if (response) {
			const retryable = response.status === 429 || (response.status >= 500 && response.status <= 599);
			if (!retryable || attempt === FETCH_RETRIES) return { response, attempts: attempt + 1 };
		}
		await sleep(1000 * 2 ** attempt);
	}
}

function statusFound(status, attempts) {
	return attempts > 1 ? `${status} (after ${attempts} attempts)` : String(status);
}

async function readJson(response, url) {
	try {
		return await response.json();
	} catch (error) {
		throw new Error(`expected JSON from ${url}, found ${describeError(error)}`);
	}
}

/**
 * The GitHub release for entry must be published and immutable, and carry
 * exactly two assets: the pinned documentation asset and the release's
 * starter ZIP, which is uploaded but not pinned.
 */
export async function checkGitHubRelease(entry, { fetch = globalThis.fetch, token } = {}) {
	if (!token) throw new Error('expected GITHUB_TOKEN for the GitHub releases API, found none');
	const url = `${RELEASES_API}${encodeURIComponent(entry.tag)}`;
	const response = await request(fetch, url, {
		headers: {
			Accept: 'application/vnd.github+json',
			Authorization: `Bearer ${token}`,
			'User-Agent': 'labrador-versions-verify',
			'X-GitHub-Api-Version': '2022-11-28',
		},
	});
	if (response.status !== 200) throw new Error(`expected HTTP 200 from ${url}, found ${response.status}`);
	const release = await readJson(response, url);
	for (const [key, expected] of [['tag_name', entry.tag], ['draft', false], ['prerelease', false], ['immutable', true]]) {
		if (release?.[key] !== expected) {
			throw new Error(`expected the release's ${key} to be ${JSON.stringify(expected)}, found ${JSON.stringify(release?.[key])}`);
		}
	}
	const assets = Array.isArray(release.assets) ? release.assets : [];
	const names = assets.map((asset) => asset?.name);
	const starter = starterAsset(entry.tag);
	if (assets.length !== 2 || !names.includes(entry.asset) || !names.includes(starter)) {
		throw new Error(`expected exactly two assets, ${entry.asset} and ${starter}, found ${assets.length}: ${names.join(', ')}`);
	}
	const asset = assets.find((candidate) => candidate.name === entry.asset);
	for (const [key, expected] of [['state', 'uploaded'], ['size', entry.bytes], ['digest', `sha256:${entry.sha256}`]]) {
		if (asset[key] !== expected) {
			throw new Error(`expected the asset's ${key} to be ${JSON.stringify(expected)}, found ${JSON.stringify(asset[key])}`);
		}
	}
	const zip = assets.find((candidate) => candidate.name === starter);
	if (zip.state !== 'uploaded') {
		throw new Error(`expected the starter ZIP's state to be "uploaded", found ${JSON.stringify(zip.state)}`);
	}
}

/**
 * The starter ZIP Get Started sends readers to for a listed release: the name
 * tools/consumer_check/starter_zip.py writes and src/lib/starter.ts links.
 */
export function starterAsset(tag) {
	return `labrador-starter-${tag}.zip`;
}

/** The commit origin's annotated tag peels to must be the entry's commit. */
export function checkTag(entry, git) {
	const ref = `refs/tags/${entry.tag}^{}`;
	let output;
	try {
		output = git(['ls-remote', '--tags', 'origin', ref]);
	} catch (error) {
		throw new Error(`expected git ls-remote origin ${ref} to answer, found it failed: ${describeError(error)}`);
	}
	const lines = output.split('\n').filter(Boolean);
	const found = lines.map((line) => line.split('\t')).filter(([, name]) => name === ref).map(([commit]) => commit);
	if (found.length !== 1 || found[0] !== entry.commit) {
		throw new Error(`expected origin's annotated tag ${entry.tag} to peel to ${entry.commit}, found ${found.length ? found.join(', ') : 'no peeled annotated tag'}`);
	}
}

/**
 * The live index and head must agree at every position both list: the same
 * tag, commit and asset hash in publication order. Either may be longer, since
 * the live site lags a change that appends and a branch may lag master. The
 * index may be absent only before anything is published.
 */
export async function checkLiveIndex(baseManifest, head, { fetch = globalThis.fetch, sleep = delay } = {}) {
	const { response, attempts } = await requestRetried(fetch, LIVE_INDEX, { headers: { 'Cache-Control': 'no-cache' } }, sleep);
	if (response.status === 404) {
		if (baseManifest.releases.length) {
			throw new Error(`expected ${LIVE_INDEX} while the base manifest lists ${baseManifest.releases.length} release(s), found 404`);
		}
		return { live: 0 };
	}
	if (response.status !== 200) throw new Error(`expected HTTP 200 or 404 from ${LIVE_INDEX}, found ${statusFound(response.status, attempts)}`);
	const index = await readJson(response, LIVE_INDEX);
	if (index?.schemaVersion !== 1 || !Array.isArray(index.versions) || !Array.isArray(index.publishedOrder)) {
		throw new Error(`expected a schema 1 index with versions and publishedOrder, found ${JSON.stringify(index?.schemaVersion)}`);
	}
	const order = index.publishedOrder;
	const listed = index.versions.filter((version) => version?.id !== 'development').map((version) => version?.id);
	if (listed.length !== order.length || listed.some((id) => !order.includes(id))) {
		throw new Error(`expected the live versions ${JSON.stringify(listed)} to match its publishedOrder ${JSON.stringify(order)}`);
	}
	order.slice(0, head.releases.length).forEach((tag, position) => {
		const entry = head.releases[position];
		const version = index.versions.find((item) => item?.id === tag);
		if (tag !== entry.tag || version?.commit !== entry.commit || version?.sha256 !== entry.sha256) {
			throw new Error(`expected live publishedOrder[${position}] to be ${entry.tag} at ${entry.commit} with SHA-256 ${entry.sha256}, found ${JSON.stringify(tag)} at ${JSON.stringify(version?.commit)} with SHA-256 ${JSON.stringify(version?.sha256)}`);
		}
	});
	return { live: order.length };
}

/** Every check CI makes on the manifest in website/ against the base commit, before the build. */
export async function verifyPublishedVersions({
	website,
	base,
	git = gitIn(path.resolve(website, '..')),
	fetch = globalThis.fetch,
	token = process.env.GITHUB_TOKEN,
	verify = verifySnapshotOutput,
	sleep = delay,
	log = logLine,
	warn = annotateWarning,
} = {}) {
	const baseCommit = resolveBase(base, git, { log });
	const previous = manifestAt(baseCommit, git);
	const head = readManifest(website);
	checkAppendOnly(previous, head);
	const appended = head.releases.slice(previous.releases.length);
	for (const entry of appended) {
		const run = async (name, action) => {
			try {
				return await action();
			} catch (error) {
				throw new Error(`${entry.tag}: ${name}: ${describeError(error)}`, { cause: error });
			}
		};
		await run('GitHub release', () => checkGitHubRelease(entry, { fetch, token }));
		await run('tag', () => checkTag(entry, git));
		const bytes = await run('download', () => fetchArchive(entry, { fetch, sleep }));
		const scratch = mkdtempSync(path.join(tmpdir(), 'labrador-versions-verify-'));
		try {
			await stageRelease(entry, bytes, scratch, { verify });
		} finally {
			rmSync(scratch, { recursive: true, force: true });
		}
	}
	// Publication depends on the live site only when this change appends a
	// release. Otherwise a disagreement, a rollback or an outage of the live
	// site is reported without failing, so that it cannot block the change
	// that repairs it.
	let live = null;
	try {
		({ live } = await checkLiveIndex(previous, head, { fetch, sleep }));
	} catch (error) {
		if (appended.length) throw new Error(`live site: ${describeError(error)}`, { cause: error });
		warn(`live site: ${describeError(error)}; not failing, because this change appends no release.`);
	}
	return { base: baseCommit, appended: appended.map((entry) => entry.tag), live };
}

function isFile(filename) {
	try {
		return lstatSync(filename).isFile();
	} catch (error) {
		if (error.code === 'ENOENT' || error.code === 'ENOTDIR') return false;
		throw error;
	}
}

// The file a page's route was built from, the inverse of pagePath.
function pageFile(dist, route) {
	if (route === '') return 'index.html';
	return isFile(path.join(dist, ...`${route}index.html`.split('/'))) ? `${route}index.html` : `${route.slice(0, -1)}.html`;
}

function elementIds(html) {
	const ids = new Set();
	(function visit(node) {
		for (const attribute of node.attrs ?? []) {
			if (attribute.name === 'id') ids.add(attribute.value);
		}
		for (const child of node.childNodes ?? []) visit(child);
		if (node.content) visit(node.content);
	})(parse(html));
	return ids;
}

/**
 * A site path must name a page or file the development build emitted outside
 * versions/, and its fragment, if any, an id in that page.
 */
export function checkSiteLink(href, dist, pages = new Set(pageList(dist, { skip: ['versions'] }))) {
	const [target, fragment] = href.split('#');
	const name = target.slice(1);
	if (name === 'versions' || name.startsWith('versions/')) {
		throw new Error(`expected a path outside /versions/, found ${target}`);
	}
	const folder = name === '' || name.endsWith('/');
	let page;
	if (folder && pages.has(name)) page = pageFile(dist, name);
	else if (!folder && isFile(path.join(dist, ...name.split('/')))) page = name.endsWith('.html') ? name : null;
	else if (!folder && pages.has(`${name}/`)) page = pageFile(dist, `${name}/`);
	else throw new Error(`expected a page or file the build emitted at ${target}, found none in ${dist}`);
	if (fragment === undefined) return;
	if (page === null) throw new Error(`expected an HTML page to hold #${fragment}, found the file ${target}`);
	if (!elementIds(readFileSync(path.join(dist, ...page.split('/')), 'utf8')).has(fragment)) {
		throw new Error(`expected an element with id ${JSON.stringify(fragment)} in ${page}, found none`);
	}
}

/** A repository URL must answer HTTP 200, to HEAD or, where HEAD is not allowed, to GET. */
export async function checkRepositoryLink(href, { fetch = globalThis.fetch, sleep = delay } = {}) {
	const headers = { 'User-Agent': 'labrador-versions-verify' };
	let { response, attempts } = await requestRetried(fetch, href, { method: 'HEAD', redirect: 'follow', headers }, sleep);
	if (response.status === 405 || response.status === 501) {
		({ response, attempts } = await requestRetried(fetch, href, { method: 'GET', redirect: 'follow', headers }, sleep));
	}
	if (response.status !== 200) throw new Error(`expected HTTP 200 from ${href}, found ${statusFound(response.status, attempts)}`);
}

/**
 * The check after the build: each erratum appended since the base links to
 * something that exists. Earlier errata are never checked again, because their
 * links cannot change and a later rename must not block every build.
 */
export async function verifyAppendedErrata({
	website,
	base,
	dist,
	git = gitIn(path.resolve(website, '..')),
	fetch = globalThis.fetch,
	sleep = delay,
	log = logLine,
} = {}) {
	const baseCommit = resolveBase(base, git, { log });
	const previous = manifestAt(baseCommit, git);
	const head = readManifest(website);
	checkAppendOnly(previous, head);
	const appended = head.errata.slice(previous.errata.length);
	const directory = path.resolve(website, dist);
	const pages = appended.length ? new Set(pageList(directory, { skip: ['versions'] })) : new Set();
	for (const [offset, erratum] of appended.entries()) {
		try {
			if (erratum.href.startsWith(REPOSITORY_URL)) await checkRepositoryLink(erratum.href, { fetch, sleep });
			else checkSiteLink(erratum.href, directory, pages);
		} catch (error) {
			throw new Error(`errata[${previous.errata.length + offset}].href ${erratum.href}: ${describeError(error)}`, { cause: error });
		}
	}
	return { base: baseCommit, errata: appended.length };
}

/**
 * The command line: --base, whose value may be empty or dropped by the shell
 * when empty (a manual run has no previous commit, and resolveBase then uses
 * the merge base), and --dist, which selects the check after the build.
 */
export function parseArguments(args) {
	const options = {};
	for (let index = 0; index < args.length; index += 1) {
		const flag = args[index];
		const value = args[index + 1];
		const hasValue = value !== undefined && !value.startsWith('--');
		if (flag === '--base' && !('base' in options)) {
			options.base = hasValue ? value : '';
			if (hasValue) index += 1;
		} else if (flag === '--dist' && !('dist' in options) && hasValue && value !== '') {
			options.dist = value;
			index += 1;
		} else {
			throw new Error(USAGE);
		}
	}
	if (!('base' in options)) throw new Error(USAGE);
	return options;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		const { base, dist } = parseArguments(process.argv.slice(2));
		const website = fileURLToPath(new URL('../', import.meta.url));
		if (dist !== undefined) {
			const result = await verifyAppendedErrata({ website, base, dist });
			process.stdout.write(`Appended errata verified against ${result.base} in ${dist}: ${result.errata ? `${result.errata} erratum link(s) checked` : 'nothing appended'}.\n`);
		} else {
			const result = await verifyPublishedVersions({ website, base });
			const live = result.live === null ? 'the live site could not be confirmed (see the warning above)' : `the live site lists ${result.live} release(s)`;
			process.stdout.write(`Published versions verified against ${result.base}: ${result.appended.length ? `appended ${result.appended.join(', ')}` : 'nothing appended'}; ${live}.\n`);
		}
	} catch (error) {
		process.stderr.write(`${describeError(error)}\n`);
		process.exitCode = 1;
	}
}
