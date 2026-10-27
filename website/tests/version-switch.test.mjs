import assert from 'node:assert/strict';
import { test } from 'node:test';
import { missingNotice, parseVersionsIndex, stripBase, targetFor } from '../src/lib/version-switch.mjs';

const INDEX = {
	schemaVersion: 1,
	versions: [
		{ id: 'development', label: 'Development', path: '/', commit: 'c'.repeat(40), pages: ['', 'docs/', 'docs/guides/new/', 'docs/guides/shared/'] },
		{ id: 'v0.3.0', label: 'v0.3.0', path: '/versions/v0.3.0/', commit: 'a'.repeat(40), published: '2026-11-01', pages: ['', 'docs/', 'docs/guides/old/', 'docs/guides/shared/'] },
		{ id: 'v0.10.0', label: 'v0.10.0', path: '/versions/v0.10.0/', commit: 'b'.repeat(40), published: '2027-02-01', latest: true, pages: ['', 'docs/', 'docs/guides/shared/'] },
	],
	publishedOrder: ['v0.3.0', 'v0.10.0'],
};

function versions() {
	return Object.fromEntries(parseVersionsIndex(INDEX).map((version) => [version.id, version]));
}

test('a shared page keeps its route and heading in both directions between any two versions', () => {
	const { development, 'v0.3.0': older, 'v0.10.0': newer } = versions();
	for (const [from, to] of [[development, older], [older, development], [older, newer], [newer, older], [development, newer]]) {
		for (const route of ['', 'docs/', 'docs/guides/shared/']) {
			const pathname = from.path + route;
			const target = targetFor(stripBase(pathname, from.path), '#heading', from.id, to);
			assert.deepEqual(target, { href: `${to.path}${route}#heading`, missing: false }, `${from.id} ${route} to ${to.id}`);
		}
	}
});

test('an absent page leads to the overview with the missing route and origin', () => {
	const { development, 'v0.3.0': older } = versions();
	assert.deepEqual(targetFor('docs/guides/new/', '#x', 'development', older), {
		href: '/versions/v0.3.0/docs/?version-missing=docs%2Fguides%2Fnew%2F&from=development', missing: true,
	});
	assert.deepEqual(targetFor('docs/guides/old/', '', 'v0.3.0', development), {
		href: '/docs/?version-missing=docs%2Fguides%2Fold%2F&from=v0.3.0', missing: true,
	});
});

test('the shared version list belongs to no version, so it opens each overview with nothing missing', () => {
	const { development, 'v0.3.0': older } = versions();
	assert.equal(stripBase('/versions/', development.path), 'versions/');
	assert.deepEqual(targetFor('versions/', '#top', 'development', older), { href: '/versions/v0.3.0/docs/', missing: false });
	assert.deepEqual(targetFor('versions/', '', 'development', development), { href: '/docs/', missing: false });
});

test('a page outside the base, or no route at all, goes to the overview', () => {
	const { 'v0.3.0': older } = versions();
	assert.equal(stripBase('/docs/guides/shared/', '/versions/v0.3.0/'), null);
	assert.equal(stripBase('/versions/v0.3.01/docs/', '/versions/v0.3.0/'), null);
	assert.equal(stripBase('docs/', '/'), null);
	assert.equal(stripBase('/docs//x/', '/'), null);
	assert.deepEqual(targetFor(null, '#x', 'v0.10.0', older), {
		href: '/versions/v0.3.0/docs/?version-missing=&from=v0.10.0', missing: true,
	});
});

test('index.html and a missing trailing slash name the same route', () => {
	assert.equal(stripBase('/index.html', '/'), '');
	assert.equal(stripBase('/', '/'), '');
	assert.equal(stripBase('/docs', '/'), 'docs/');
	assert.equal(stripBase('/docs/index.html', '/'), 'docs/');
	assert.equal(stripBase('/versions/v0.3.0', '/versions/v0.3.0/'), '');
	assert.equal(stripBase('/versions/v0.3.0/index.html', '/versions/v0.3.0/'), '');
	assert.equal(stripBase('/versions/v0.3.0/docs/guides/x', '/versions/v0.3.0/'), 'docs/guides/x/');
});

test('the index lists Development, then releases newest first by semver, the newest marked latest', () => {
	const parsed = parseVersionsIndex(JSON.stringify({ ...INDEX, versions: [INDEX.versions[1], INDEX.versions[2], INDEX.versions[0]] }));
	assert.deepEqual(parsed.map((version) => version.id), ['development', 'v0.10.0', 'v0.3.0']);
	assert.deepEqual(parsed.map((version) => version.latest), [false, true, false]);
	assert.equal(parsed[2].published, '2026-11-01');
	// The flag is derived, not read: an index claiming an older latest is not believed.
	const claimed = structuredClone(INDEX);
	claimed.versions[1].latest = true;
	claimed.versions[2].latest = false;
	assert.equal(parseVersionsIndex(claimed).find((version) => version.latest).id, 'v0.10.0');
});

test('an unknown schema is refused and untrusted entries are dropped', () => {
	for (const index of [{ ...INDEX, schemaVersion: 2 }, { versions: INDEX.versions }, null, { schemaVersion: 1 }]) {
		assert.throws(() => parseVersionsIndex(index), /schemaVersion 1|versions array/);
	}
	const page = (id, path) => ({ id, label: id, path, pages: [''] });
	const parsed = parseVersionsIndex({
		schemaVersion: 1,
		future: 'ignored',
		versions: [
			{ ...INDEX.versions[0], extra: true },
			page('development', '/elsewhere/'),
			page('v0.3.0', '/versions/v0.4.0/'),
			page('v0.3', '/versions/v0.3/'),
			page('v0.3.0-rc.1', '/versions/v0.3.0-rc.1/'),
			page('v0.4.0', 'https://example.invalid/versions/v0.4.0/'),
			{ id: 'v0.5.0', path: '/versions/v0.5.0/' },
			null,
			'v0.6.0',
			INDEX.versions[1],
			{ ...INDEX.versions[1], path: '/versions/v0.3.0/', label: 'Duplicate' },
			{ ...page('v0.7.0', '/versions/v0.7.0/'), label: '', pages: ['docs/', '/docs/', '../x/', 'a//b/', '<b>/', 7] },
		],
	});
	assert.deepEqual(parsed.map((version) => [version.id, version.label]), [['development', 'Development'], ['v0.7.0', 'v0.7.0'], ['v0.3.0', 'v0.3.0']]);
	assert.deepEqual(parsed[1].pages, ['docs/']);
});

test('the arrival notice repeats a plain route and a real version, and nothing else', () => {
	const notice = (query) => missingNotice(new URLSearchParams(query), 'v0.3.0');
	const general = 'The page you were reading is not part of v0.3.0, so this is the v0.3.0 documentation overview.';
	assert.equal(notice(''), null);
	assert.equal(notice('q=x'), null);
	assert.equal(notice('version-missing=docs%2Fguides%2Ffoo%2F&from=v0.4.0'),
		'The page /docs/guides/foo/ that you were reading in v0.4.0 is not part of v0.3.0, so this is the v0.3.0 documentation overview.');
	assert.equal(notice('version-missing=docs%2Fguides%2Ffoo%2F&from=development'),
		'The page /docs/guides/foo/ that you were reading in Development is not part of v0.3.0, so this is the v0.3.0 documentation overview.');
	for (const query of [
		'version-missing=',
		'version-missing=%3Cimg%20src%3Dx%3E&from=v0.4.0',
		'version-missing=%2F%2Fevil.example%2F&from=v0.4.0',
		'version-missing=docs%2F%2Fx%2F&from=v0.4.0',
		'version-missing=docs%2F..%2F..%2Fx%2F&from=v0.4.0',
		`version-missing=${'a'.repeat(201)}&from=v0.4.0`,
		'version-missing=docs%2F&from=%3Cb%3E',
		'version-missing=docs%2F&from=v0.4',
		'version-missing=docs%2F',
	]) {
		assert.equal(notice(query), general, query);
	}
	assert.match(notice(`version-missing=${'a'.repeat(200)}&from=v0.4.0`), /^The page \/a{200} /);
});
