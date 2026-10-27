import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import path from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';
import { assetUrl, canonicalize } from '../scripts/published-versions.mjs';
import {
	LIVE_INDEX, annotateWarning, parseArguments, resolveBase, verifyAppendedErrata, verifyPublishedVersions,
} from '../scripts/verify-published-versions.mjs';
import { developmentDist, releaseAsset, response, temporaryDirectory, write } from './fixtures/snapshot-site.mjs';

const SOURCE_DATE = '2026-10-20';
const COMMIT_ENV = {
	...process.env,
	GIT_AUTHOR_DATE: `${SOURCE_DATE}T12:00:00+00:00`,
	GIT_COMMITTER_DATE: `${SOURCE_DATE}T12:00:00+00:00`,
};
const API = 'https://api.github.com/repos/matt439/Labrador/releases/tags/';

function git(root, ...args) {
	return execFileSync('git', args, {
		cwd: root, encoding: 'utf8', env: COMMIT_ENV, stdio: ['ignore', 'pipe', 'pipe'],
	}).trim();
}

function manifest(releases = [], errata = []) {
	return canonicalize({ schemaVersion: 1, releases, errata });
}

// A clone of a bare origin: master carries the base commit, and v0.3.0 is an
// annotated tag on it. Head's manifest is whatever the working tree holds.
function checkout(t, { baseManifest } = {}) {
	const origin = temporaryDirectory(t, 'labrador-verify-origin-');
	git(origin, 'init', '--quiet', '--bare', '--initial-branch=master');
	const root = temporaryDirectory(t, 'labrador-verify-checkout-');
	git(root, 'init', '--quiet', '--initial-branch=master');
	git(root, 'config', 'user.name', 'Versions fixture');
	git(root, 'config', 'user.email', 'versions@example.invalid');
	git(root, 'config', 'core.autocrlf', 'false');
	write(root, 'engine/core/example.h', '#pragma once\n');
	write(root, 'website/package.json', '{}\n');
	git(root, 'add', '.');
	git(root, 'commit', '--quiet', '-m', 'Release source');
	git(root, 'tag', '-a', 'v0.3.0', '-m', 'Release v0.3.0');
	const tagged = git(root, 'rev-parse', 'HEAD');
	if (baseManifest !== undefined) {
		write(root, 'website/published-versions.json', baseManifest);
		git(root, 'add', '.');
		git(root, 'commit', '--quiet', '-m', 'Publish');
	}
	const base = git(root, 'rev-parse', 'HEAD');
	git(root, 'remote', 'add', 'origin', origin);
	git(root, 'push', '--quiet', 'origin', 'master', '--tags');
	git(root, 'fetch', '--quiet', 'origin');
	return { root, website: path.join(root, 'website'), tagged, base };
}

function githubRelease(entry, change = (release) => release) {
	return change({
		tag_name: entry.tag, draft: false, prerelease: false, immutable: true,
		assets: [{ name: entry.asset, state: 'uploaded', size: entry.bytes, digest: `sha256:${entry.sha256}` }],
	});
}

// Answers by URL: the releases API, the asset download and the live index.
function site({ releases = {}, archives = {}, live = () => response('Not found', { status: 404, url: LIVE_INDEX }) } = {}) {
	const calls = [];
	const fetch = async (url, options) => {
		calls.push(String(url));
		if (url === LIVE_INDEX) return live();
		if (url.startsWith(API)) {
			const tag = decodeURIComponent(url.slice(API.length));
			assert.equal(options.headers.Authorization, 'Bearer fixture-token');
			return tag in releases ? response(JSON.stringify(releases[tag]), { url }) : response('{}', { status: 404, url });
		}
		if (url in archives) return response(archives[url]);
		return response('Not found', { status: 404, url });
	};
	return { fetch, calls };
}

function liveIndex(entries) {
	return () => response(JSON.stringify({
		schemaVersion: 1,
		versions: [{ id: 'development', path: '/', pages: [''] }, ...entries.map((entry) => ({
			id: entry.tag, path: entry.path, commit: entry.commit, sha256: entry.sha256, pages: [''],
		}))],
		publishedOrder: entries.map((entry) => entry.tag),
	}), { url: LIVE_INDEX });
}

function options(fixture, extra) {
	return { website: fixture.website, base: fixture.base, token: 'fixture-token', sleep: async () => {}, ...extra };
}

async function published(t, fixture) {
	const { archive, entry } = await releaseAsset(t, { commit: fixture.tagged, sourceDate: SOURCE_DATE });
	return { archive, entry, archives: { [assetUrl(entry)]: archive } };
}

test('a base without the manifest counts as empty, and an appended release is checked and downloaded', async (t) => {
	const fixture = checkout(t);
	const { entry, archives } = await published(t, fixture);
	write(fixture.website, 'published-versions.json', manifest([entry]));
	const { fetch, calls } = site({ releases: { 'v0.3.0': githubRelease(entry) }, archives });
	const verified = [];
	const result = await verifyPublishedVersions(options(fixture, {
		fetch, verify: async (_directory, record) => { verified.push(record.commit); return {}; },
	}));
	assert.deepEqual(result, { base: fixture.base, appended: ['v0.3.0'], live: 0 });
	assert.deepEqual(calls, [`${API}v0.3.0`, assetUrl(entry), LIVE_INDEX]);
	assert.deepEqual(verified, [fixture.tagged]);
});

test('the real verifier accepts the downloaded fixture snapshot', async (t) => {
	const fixture = checkout(t);
	const { entry, archives } = await published(t, fixture);
	write(fixture.website, 'published-versions.json', manifest([entry]));
	const { fetch } = site({ releases: { 'v0.3.0': githubRelease(entry) }, archives });
	assert.deepEqual((await verifyPublishedVersions(options(fixture, { fetch }))).appended, ['v0.3.0']);
});

test('a zero, empty or unknown base compares with where the branch left origin/master', async (t) => {
	const fixture = checkout(t, { baseManifest: manifest() });
	write(fixture.website, 'published-versions.json', manifest());
	for (const base of ['0'.repeat(40), '']) {
		const logged = [];
		const result = await verifyPublishedVersions(options(fixture, { base, fetch: site().fetch, log: (line) => logged.push(line) }));
		assert.equal(result.base, fixture.base);
		assert.deepEqual(logged, []);
	}
	const logged = [];
	const result = await verifyPublishedVersions(options(fixture, { base: 'f'.repeat(40), fetch: site().fetch, log: (line) => logged.push(line) }));
	assert.equal(result.base, fixture.base);
	assert.equal(logged.length, 1);
	assert.match(logged[0], /^base: "f{40}" is not a commit in this checkout, as after a force-push; comparing with git merge-base HEAD origin\/master instead\.$/);
});

test('resolveBase falls back to the merge base only when the named base does not resolve', () => {
	const known = 'a'.repeat(40);
	const mergeBase = 'b'.repeat(40);
	const fakeGit = ({ mergeBaseFails = false } = {}) => {
		const calls = [];
		const git = (args) => {
			calls.push(args.join(' '));
			if (args[0] === 'merge-base') {
				if (mergeBaseFails) throw new Error('no origin/master');
				return `${mergeBase}\n`;
			}
			if (args[0] === 'rev-parse') {
				const commit = args[3].replace(/\^\{commit\}$/, '');
				if (commit === known || commit === mergeBase) return `${commit}\n`;
				throw new Error('unknown revision');
			}
			throw new Error(`unexpected git ${args.join(' ')}`);
		};
		return { git, calls };
	};
	const present = fakeGit();
	assert.equal(resolveBase(known, present.git, { log: () => assert.fail('nothing to report') }), known);
	assert.deepEqual(present.calls, [`rev-parse --verify --quiet ${known}^{commit}`]);

	const forced = fakeGit();
	const logged = [];
	assert.equal(resolveBase('c'.repeat(40), forced.git, { log: (line) => logged.push(line) }), mergeBase);
	assert.deepEqual(forced.calls, [
		`rev-parse --verify --quiet ${'c'.repeat(40)}^{commit}`,
		'merge-base HEAD origin/master',
		`rev-parse --verify --quiet ${mergeBase}^{commit}`,
	]);
	assert.match(logged.join('\n'), /"c{40}" is not a commit in this checkout.*merge-base HEAD origin\/master instead/);

	assert.throws(() => resolveBase('c'.repeat(40), fakeGit({ mergeBaseFails: true }).git, { log: () => {} }),
		/base: expected git merge-base HEAD origin\/master to name a commit, found it failed: no origin\/master; check out full history/);
});

test('an edited or removed base entry fails', async (t) => {
	const probe = checkout(t);
	const { entry } = await published(t, probe);
	const fixture = checkout(t, { baseManifest: manifest([entry]) });
	write(fixture.website, 'published-versions.json', manifest([{ ...entry, published: '2026-10-22' }]));
	await assert.rejects(verifyPublishedVersions(options(fixture, { fetch: site().fetch })), /releases\[0\]: expected the published item unchanged/);
	write(fixture.website, 'published-versions.json', manifest());
	await assert.rejects(verifyPublishedVersions(options(fixture, { fetch: site().fetch })), /releases: expected at least the 1 published item/);
});

test('a GitHub release that is not exactly the pinned immutable asset is refused', async (t) => {
	const fixture = checkout(t);
	const { entry, archives } = await published(t, fixture);
	write(fixture.website, 'published-versions.json', manifest([entry]));
	const extra = { name: 'notes.txt', state: 'uploaded', size: 1, digest: `sha256:${'0'.repeat(64)}` };
	for (const [change, expected] of [
		[(release) => ({ ...release, draft: true }), /v0\.3\.0: GitHub release: expected the release's draft to be false, found true/],
		[(release) => ({ ...release, prerelease: true }), /expected the release's prerelease to be false, found true/],
		[(release) => ({ ...release, immutable: false }), /expected the release's immutable to be true, found false/],
		[({ immutable, ...release }) => release, /expected the release's immutable to be true, found undefined/],
		[(release) => ({ ...release, assets: [{ ...release.assets[0], digest: `sha256:${'f'.repeat(64)}` }] }), /expected the asset's digest to be "sha256:[0-9a-f]{64}", found "sha256:f{64}"/],
		[(release) => ({ ...release, assets: [{ ...release.assets[0], size: entry.bytes + 1 }] }), new RegExp(`expected the asset's size to be ${entry.bytes}, found ${entry.bytes + 1}`)],
		[(release) => ({ ...release, assets: [...release.assets, extra] }), /expected exactly one asset, labrador-docs-v0\.3\.0\.tar\.gz, found 2/],
		[(release) => ({ ...release, assets: [{ ...release.assets[0], name: 'other.tar.gz' }] }), /expected the asset's name to be "labrador-docs-v0\.3\.0\.tar\.gz", found "other\.tar\.gz"/],
	]) {
		const { fetch, calls } = site({ releases: { 'v0.3.0': githubRelease(entry, change) }, archives });
		await assert.rejects(verifyPublishedVersions(options(fixture, { fetch })), expected);
		assert.deepEqual(calls, [`${API}v0.3.0`], 'nothing is downloaded after a refused release');
	}
	const { fetch } = site({ archives });
	await assert.rejects(verifyPublishedVersions(options(fixture, { fetch })), /GitHub release: expected HTTP 200 from .*, found 404/);
	await assert.rejects(verifyPublishedVersions(options(fixture, { fetch, token: '' })), /expected GITHUB_TOKEN for the GitHub releases API, found none/);
});

test("an entry whose commit is not the one origin's tag peels to is refused", async (t) => {
	const fixture = checkout(t);
	const { archive, entry } = await releaseAsset(t, { commit: 'b'.repeat(40), sourceDate: SOURCE_DATE });
	write(fixture.website, 'published-versions.json', manifest([entry]));
	const { fetch, calls } = site({ releases: { 'v0.3.0': githubRelease(entry) }, archives: { [assetUrl(entry)]: archive } });
	await assert.rejects(verifyPublishedVersions(options(fixture, { fetch })),
		new RegExp(`v0\\.3\\.0: tag: expected origin's annotated tag v0\\.3\\.0 to peel to b{40}, found ${fixture.tagged}`));
	assert.deepEqual(calls, [`${API}v0.3.0`]);
});

test('an asset whose download differs from the pinned bytes is refused', async (t) => {
	const fixture = checkout(t);
	const { archive, entry } = await published(t, fixture);
	write(fixture.website, 'published-versions.json', manifest([entry]));
	const tampered = Buffer.from(archive);
	tampered[tampered.length - 1] ^= 1;
	const { fetch } = site({ releases: { 'v0.3.0': githubRelease(entry) }, archives: { [assetUrl(entry)]: tampered } });
	await assert.rejects(verifyPublishedVersions(options(fixture, { fetch })), /v0\.3\.0: read archive: .*expected SHA-256/);
});

// A base that lists v0.3.0, and a v0.4.0 tag on origin that head may append.
async function publishedBase(t) {
	const probe = checkout(t);
	const { entry } = await published(t, probe);
	const fixture = checkout(t, { baseManifest: manifest([entry]) });
	git(fixture.root, 'tag', '-a', 'v0.4.0', '-m', 'Release v0.4.0');
	git(fixture.root, 'push', '--quiet', 'origin', '--tags');
	const next = await releaseAsset(t, { tag: 'v0.4.0', commit: fixture.base, sourceDate: SOURCE_DATE });
	return {
		fixture, entry, next: next.entry,
		releases: { 'v0.4.0': githubRelease(next.entry) }, archives: { [assetUrl(next.entry)]: next.archive },
	};
}

function collector() {
	const lines = [];
	return { lines, warn: (line) => lines.push(line) };
}

test('the live index may be absent before anything was published, and is otherwise only a warning unless releasing', async (t) => {
	const empty = checkout(t);
	write(empty.website, 'published-versions.json', manifest());
	const quiet = collector();
	assert.equal((await verifyPublishedVersions(options(empty, { fetch: site().fetch, warn: quiet.warn }))).live, 0);
	assert.deepEqual(quiet.lines, []);

	const { fixture, entry, next, releases, archives } = await publishedBase(t);
	write(fixture.website, 'published-versions.json', manifest([entry]));
	const warned = collector();
	assert.equal((await verifyPublishedVersions(options(fixture, { fetch: site().fetch, warn: warned.warn }))).live, null);
	assert.equal(warned.lines.length, 1);
	assert.match(warned.lines[0], /^live site: expected https:\/\/labradorengine\.com\/versions\/index\.json while the base manifest lists 1 release\(s\), found 404; not failing, because this change appends no release\.$/);
	assert.equal((await verifyPublishedVersions(options(fixture, { fetch: site({ live: liveIndex([entry]) }).fetch }))).live, 1);

	write(fixture.website, 'published-versions.json', manifest([entry, next]));
	await assert.rejects(verifyPublishedVersions(options(fixture, { fetch: site({ releases, archives }).fetch, warn: () => assert.fail('a release fails, not warns') })),
		/live site: expected https:\/\/labradorengine\.com\/versions\/index\.json while the base manifest lists 1 release\(s\), found 404/);
	const result = await verifyPublishedVersions(options(fixture, { fetch: site({ releases, archives, live: liveIndex([entry]) }).fetch }));
	assert.deepEqual(result, { base: fixture.base, appended: ['v0.4.0'], live: 1 });
});

test('live and head must agree wherever both list a release, and either may be longer', async (t) => {
	const { fixture, entry, next, releases, archives } = await publishedBase(t);
	// A branch that forked before a release was appended lags master: consistent.
	const lagging = checkout(t);
	write(lagging.website, 'published-versions.json', manifest());
	const quiet = collector();
	assert.equal((await verifyPublishedVersions(options(lagging, { fetch: site({ live: liveIndex([entry]) }).fetch, warn: quiet.warn }))).live, 1);
	write(fixture.website, 'published-versions.json', manifest([entry]));
	assert.equal((await verifyPublishedVersions(options(fixture, { fetch: site({ live: liveIndex([entry, next]) }).fetch, warn: quiet.warn }))).live, 2);
	assert.deepEqual(quiet.lines, []);

	const differing = [
		{ ...entry, commit: 'e'.repeat(40) },
		{ ...entry, sha256: 'e'.repeat(64) },
		{ ...entry, tag: 'v0.2.0', path: '/versions/v0.2.0/' },
	];
	for (const live of differing) {
		write(fixture.website, 'published-versions.json', manifest([entry]));
		const warned = collector();
		assert.equal((await verifyPublishedVersions(options(fixture, { fetch: site({ live: liveIndex([live]) }).fetch, warn: warned.warn }))).live, null);
		assert.match(warned.lines.join('\n'), /^live site: expected live publishedOrder\[0\] to be v0\.3\.0 .*; not failing, because this change appends no release\.$/);

		write(fixture.website, 'published-versions.json', manifest([entry, next]));
		await assert.rejects(verifyPublishedVersions(options(fixture, { fetch: site({ releases, archives, live: liveIndex([live]) }).fetch })),
			/live site: expected live publishedOrder\[0\] to be v0\.3\.0/);
	}
});

test('the live index is retried through network errors, timeouts, 5xx and 429, and 200 and 404 are decisive', async (t) => {
	const fixture = checkout(t);
	write(fixture.website, 'published-versions.json', manifest());
	const { entry } = await releaseAsset(t, { commit: fixture.tagged, sourceDate: SOURCE_DATE });
	const answers = (...steps) => {
		let call = 0;
		return () => steps[Math.min(call++, steps.length - 1)]();
	};
	const failed = () => { throw new TypeError('fetch failed'); };
	const timedOut = () => { throw new DOMException('The operation was aborted due to timeout', 'TimeoutError'); };
	const busy = () => response('busy', { status: 503, url: LIVE_INDEX });
	const limited = () => response('slow down', { status: 429, url: LIVE_INDEX });
	for (const first of [failed, timedOut, busy, limited]) {
		const slept = [];
		const { fetch, calls } = site({ live: answers(first, first, liveIndex([entry])) });
		const result = await verifyPublishedVersions(options(fixture, { fetch, sleep: async (ms) => { slept.push(ms); }, warn: () => assert.fail('recovered') }));
		assert.equal(result.live, 1);
		assert.deepEqual(calls, [LIVE_INDEX, LIVE_INDEX, LIVE_INDEX]);
		assert.deepEqual(slept, [1000, 2000]);
	}
	for (const decisive of [() => response('Not found', { status: 404, url: LIVE_INDEX }), liveIndex([entry])]) {
		const slept = [];
		const { fetch, calls } = site({ live: answers(decisive, busy) });
		await verifyPublishedVersions(options(fixture, { fetch, sleep: async (ms) => { slept.push(ms); } }));
		assert.deepEqual(calls, [LIVE_INDEX]);
		assert.deepEqual(slept, []);
	}
});

test('a live index that cannot be read is a warning without a release and a failure with one', async (t) => {
	const fixture = checkout(t);
	write(fixture.website, 'published-versions.json', manifest());
	const unreadable = [
		[() => { throw new TypeError('fetch failed'); }, /live site: expected a response from https:\/\/labradorengine\.com\/versions\/index\.json, found fetch failed \(after 4 attempts\)/],
		[() => response('busy', { status: 503, url: LIVE_INDEX }), /live site: expected HTTP 200 or 404 from .*, found 503 \(after 4 attempts\)/],
		[() => response('challenge', { status: 403, url: LIVE_INDEX }), /live site: expected HTTP 200 or 404 from .*, found 403;/],
		[() => response('<html>', { url: LIVE_INDEX }), /live site: expected JSON from/],
		[() => response('{"schemaVersion":2}', { url: LIVE_INDEX }), /live site: expected a schema 1 index/],
	];
	for (const [live, expected] of unreadable) {
		const warned = collector();
		const result = await verifyPublishedVersions(options(fixture, { fetch: site({ live }).fetch, warn: warned.warn }));
		assert.equal(result.live, null);
		assert.equal(warned.lines.length, 1);
		assert.match(warned.lines[0], expected);
	}

	const releasing = checkout(t);
	const { entry, archives } = await published(t, releasing);
	write(releasing.website, 'published-versions.json', manifest([entry]));
	for (const [live] of unreadable) {
		await assert.rejects(verifyPublishedVersions(options(releasing, { fetch: site({ releases: { 'v0.3.0': githubRelease(entry) }, archives, live }).fetch })),
			/^Error: live site: /);
	}
});

test('the warning is an Actions annotation under GitHub Actions and a plain line elsewhere', (t) => {
	const written = [];
	const original = process.stderr.write;
	const actions = process.env.GITHUB_ACTIONS;
	t.after(() => {
		process.stderr.write = original;
		if (actions === undefined) delete process.env.GITHUB_ACTIONS;
		else process.env.GITHUB_ACTIONS = actions;
	});
	process.stderr.write = (text) => { written.push(text); return true; };
	process.env.GITHUB_ACTIONS = 'true';
	annotateWarning('live site: 100% down\nnext');
	delete process.env.GITHUB_ACTIONS;
	annotateWarning('live site: down');
	process.stderr.write = original;
	assert.deepEqual(written, ['::warning::live site: 100%25 down%0Anext\n', 'warning: live site: down\n']);
});

// The development dist/ the build leaves, with one heading id and one plain file.
function erratumDist(t) {
	const dist = developmentDist(temporaryDirectory(t, 'labrador-verify-dist-'));
	write(dist, 'docs/guides/one/index.html', '<!doctype html><html><body><h2 id="setup">Setup</h2></body></html>');
	write(dist, 'notes.txt', 'release notes');
	write(dist, 'versions/v0.3.0/docs/guides/old/index.html', '<!doctype html><html><body></body></html>');
	return dist;
}

function erratum(href, date = '2026-10-23') {
	return { tag: 'v0.3.0', date, title: 'A correction', href };
}

test('after the build, only the errata this change appends must link to something that exists', async (t) => {
	const probe = checkout(t);
	const { entry } = await published(t, probe);
	// The base's erratum links nowhere now; it is never checked again.
	const stale = erratum('/docs/guides/renamed/', '2026-10-22');
	const fixture = checkout(t, { baseManifest: manifest([entry], [stale]) });
	const dist = erratumDist(t);
	const noFetch = async () => assert.fail('no request for a site path');
	write(fixture.website, 'published-versions.json', manifest([entry], [stale]));
	assert.deepEqual(await verifyAppendedErrata(options(fixture, { dist, fetch: noFetch })), { base: fixture.base, errata: 0 });

	for (const href of ['/', '/docs/', '/docs/guides/one/', '/docs/guides/one', '/docs/guides/one/#setup', '/notes.txt']) {
		write(fixture.website, 'published-versions.json', manifest([entry], [stale, erratum(href)]));
		assert.deepEqual(await verifyAppendedErrata(options(fixture, { dist, fetch: noFetch })), { base: fixture.base, errata: 1 }, href);
	}
	for (const [href, expected] of [
		['/docs/guides/erata/', /errata\[1\]\.href \/docs\/guides\/erata\/: expected a page or file the build emitted at \/docs\/guides\/erata\/, found none/],
		['/docs/guides/old/', /expected a page or file the build emitted at \/docs\/guides\/old\//],
		['/docs/guides/one/#set-up', /expected an element with id "set-up" in docs\/guides\/one\/index\.html, found none/],
		['/notes.txt#top', /expected an HTML page to hold #top, found the file \/notes\.txt/],
		['/notes.txt/', /expected a page or file the build emitted at \/notes\.txt\//],
	]) {
		write(fixture.website, 'published-versions.json', manifest([entry], [stale, erratum(href)]));
		await assert.rejects(verifyAppendedErrata(options(fixture, { dist, fetch: noFetch })), expected, href);
	}
});

test('an appended repository link must answer HTTP 200', async (t) => {
	const probe = checkout(t);
	const { entry } = await published(t, probe);
	const fixture = checkout(t, { baseManifest: manifest([entry]) });
	const dist = erratumDist(t);
	const href = 'https://github.com/matt439/Labrador/issues/1';
	write(fixture.website, 'published-versions.json', manifest([entry], [erratum(href)]));
	const answering = (...statuses) => {
		const calls = [];
		const fetch = async (url, init) => {
			calls.push(`${init.method} ${url}`);
			return response('', { status: statuses[Math.min(calls.length - 1, statuses.length - 1)], url });
		};
		return { fetch, calls };
	};
	const ok = answering(200);
	assert.equal((await verifyAppendedErrata(options(fixture, { dist, fetch: ok.fetch }))).errata, 1);
	assert.deepEqual(ok.calls, [`HEAD ${href}`]);
	const headRefused = answering(405, 200);
	await verifyAppendedErrata(options(fixture, { dist, fetch: headRefused.fetch }));
	assert.deepEqual(headRefused.calls, [`HEAD ${href}`, `GET ${href}`]);
	const busy = answering(503, 200);
	await verifyAppendedErrata(options(fixture, { dist, fetch: busy.fetch }));
	assert.deepEqual(busy.calls, [`HEAD ${href}`, `HEAD ${href}`]);
	await assert.rejects(verifyAppendedErrata(options(fixture, { dist, fetch: answering(404).fetch })),
		/errata\[0\]\.href https:\/\/github\.com\/matt439\/Labrador\/issues\/1: expected HTTP 200 from .*, found 404/);
});

test('the command line takes --base, possibly empty, and an optional --dist', () => {
	assert.deepEqual(parseArguments(['--base', 'abc']), { base: 'abc' });
	assert.deepEqual(parseArguments(['--base']), { base: '' });
	assert.deepEqual(parseArguments(['--base', '--dist', 'dist']), { base: '', dist: 'dist' });
	assert.deepEqual(parseArguments(['--base', 'abc', '--dist', 'dist']), { base: 'abc', dist: 'dist' });
	for (const args of [[], ['--dist', 'dist'], ['--base', 'abc', '--dist'], ['--base', 'a', '--base', 'b'], ['--base', 'abc', 'extra']]) {
		assert.throws(() => parseArguments(args), /^Error: Usage: npm run versions:verify -- --base <commit> \[--dist <directory>\]$/, args.join(' '));
	}
});

test('the CLI requires --base and prints nothing on stdout when it fails', () => {
	const script = fileURLToPath(new URL('../scripts/verify-published-versions.mjs', import.meta.url));
	for (const args of [[], ['--head', 'x'], ['--base', 'x', 'y']]) {
		const result = spawnSync(process.execPath, [script, ...args], { encoding: 'utf8' });
		assert.equal(result.status, 1);
		assert.equal(result.stdout, '');
		assert.match(result.stderr, /^Usage: npm run versions:verify -- --base <commit>/);
	}
});
