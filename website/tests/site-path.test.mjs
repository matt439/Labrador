import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';

function run(source, tag) {
	const env = { ...process.env };
	if (tag === undefined) delete env.LABRADOR_DOCS_TAG;
	else env.LABRADOR_DOCS_TAG = tag;
	return spawnSync(process.execPath, ['--input-type=module', '--eval', source], {
		cwd: process.cwd(), env, encoding: 'utf8',
	});
}

test('development paths remain unchanged', () => {
	const result = run(`
		import assert from 'node:assert/strict';
		import { SITE_BASE, sitePath, stripSiteBase } from './src/lib/site-path.ts';
		assert.equal(SITE_BASE, '/');
		for (const url of ['/', '/docs/reference/#about', '//example.com/file', 'https://example.com/', '#about', '../guide/']) {
			assert.equal(sitePath(url), url);
			assert.equal(stripSiteBase(url), url);
		}
	`);
	assert.equal(result.status, 0, result.stderr);
});

test('release paths retain query/fragment and are prefixed once', () => {
	const result = run(`
		import assert from 'node:assert/strict';
		import { SITE_BASE, sitePath, stripSiteBase } from './src/lib/site-path.ts';
		import { NAVIGATION, isCurrent } from './src/lib/navigation.ts';
		assert.equal(SITE_BASE, '/versions/v0.3.0/');
		assert.equal(sitePath('/'), SITE_BASE);
		assert.equal(sitePath('/docs/reference/?q=Scene#class-scene'), SITE_BASE + 'docs/reference/?q=Scene#class-scene');
		assert.equal(sitePath(sitePath('/docs/')), SITE_BASE + 'docs/');
		assert.equal(sitePath(SITE_BASE.slice(0, -1)), SITE_BASE.slice(0, -1));
		assert.equal(sitePath(SITE_BASE.slice(0, -1) + '?query#anchor'), SITE_BASE.slice(0, -1) + '?query#anchor');
		assert.equal(stripSiteBase(SITE_BASE), '/');
		assert.equal(stripSiteBase(SITE_BASE + 'docs/reference/'), '/docs/reference/');
		assert.equal(stripSiteBase('/versions/v0.3.00/docs/'), '/versions/v0.3.00/docs/');
		for (const url of ['', '//example.com/file', 'https://example.com/', 'mailto:a@example.com', '#about', '../guide/']) {
			assert.equal(sitePath(url), url);
		}
		assert.ok(NAVIGATION.every((item) => item.href.startsWith(SITE_BASE)));
		assert.equal(isCurrent(SITE_BASE + 'docs/reference/', SITE_BASE + 'docs/'), true);
		assert.equal(isCurrent(SITE_BASE + 'examples/', SITE_BASE + 'docs/'), false);
	`,'v0.3.0');
	assert.equal(result.status, 0, result.stderr);
});

test('release path mode rejects prereleases, malformed tags and earlier releases', () => {
	for (const tag of ['v0.2.0', 'v0.3.0-rc.1', 'v00.3.0', 'v0.3.0/../../', '']) {
		const result = run("await import('./src/lib/site-path.ts');", tag);
		assert.notEqual(result.status, 0, tag);
		assert.match(result.stderr, /stable release tag|snapshots start at v0\.3\.0/);
	}
});

test('frontmatter routes hero and explicit pagination links before downstream middleware', () => {
	for (const tag of [undefined, 'v0.3.0']) {
		const result = run(`
			import assert from 'node:assert/strict';
			import { onRequest } from './src/route-data.ts';
			import { SITE_BASE } from './src/lib/site-path.ts';
			const data = {
				hero: { actions: [{ link: '/docs/' }, { link: 'https://example.com/' }] },
				prev: { link: '/why/' }, next: false,
			};
			const pagination = { prev: { href: '/why/' }, next: { href: SITE_BASE + 'docs/' } };
			const context = { locals: { starlightRoute: { entry: { data }, pagination } } };
			let called = false;
			await onRequest(context, async () => {
				assert.equal(data.hero.actions[0].link, SITE_BASE + 'docs/');
				assert.equal(data.hero.actions[1].link, 'https://example.com/');
				assert.equal(data.prev.link, SITE_BASE + 'why/');
				assert.equal(data.next, false);
				assert.equal(pagination.prev.href, SITE_BASE + 'why/');
				assert.equal(pagination.next.href, SITE_BASE + 'docs/');
				called = true;
			});
			assert.equal(called, true);
		`, tag);
		assert.equal(result.status, 0, result.stderr);
	}
});

test('Markdown and MDX use version paths without changing code or external links', () => {
	// Load the development source identity first: this unit test exercises the
	// routing layer independently from the release-checkout provenance gate.
	const result = run(`
		import assert from 'node:assert/strict';
		import { markdownToHtml, mdxToJs } from 'satteri';
		await import('./src/lib/repository.ts');
		process.env.LABRADOR_DOCS_TAG = 'v0.3.0';
		const { checkoutLinks, referenceLinks } = await import('./src/lib/markdown-plugins.ts');
		const { referencePage } = await import('./src/lib/reference.ts');
		const { REFERENCE_HEADERS } = await import('./src/lib/published.ts');
		const { apiCodeLinks } = await import('./src/lib/code-links.ts');
		const { ExpressiveCode } = await import('@astrojs/starlight/expressive-code');
		const markdown = [
			'[Guide](/docs/guides/input/#held-pressed-and-released)',
			'[Reference][api]',
			'[api]: /docs/reference/input/keyboard/',
			'![Mark](/favicon.svg)',
			'[External](https://example.com/) [Protocol](//example.com/) [Local](#here)',
			String.fromCharCode(96) + 'Scene' + String.fromCharCode(96) + ' is linked automatically.',
			'~~~text\\n/docs/ is code\\n~~~',
		].join('\\n\\n');
		const { html } = await markdownToHtml(markdown, { mdastPlugins: [checkoutLinks, referenceLinks] });
		assert.match(html, /href="\\/versions\\/v0\\.3\\.0\\/docs\\/guides\\/input\\/#held-pressed-and-released"/);
		assert.match(html, /href="\\/versions\\/v0\\.3\\.0\\/docs\\/reference\\/input\\/keyboard\\/"/);
		assert.match(html, /src="\\/versions\\/v0\\.3\\.0\\/favicon\\.svg"/);
		assert.match(html, /href="\\/versions\\/v0\\.3\\.0\\/docs\\/reference\\/scene\\/scene\\/#class-scene"/);
		assert.match(html, /href="https:\\/\\/example\\.com\\/"/);
		assert.match(html, /href="\\/\\/example\\.com\\/"/);
		assert.match(html, /href="#here"/);
		assert.match(html, /<code[^>]*>\\/docs\\/ is code/);
		const { code } = await mdxToJs('<LinkCard href="/docs/" title="Docs" />\\n\\n<a href="/why/">Why</a>', {
			mdastPlugins: [checkoutLinks],
		});
		assert.match(code, /\\/versions\\/v0\\.3\\.0\\/docs\\//);
		assert.match(code, /\\/versions\\/v0\\.3\\.0\\/why\\//);
		const reference = referencePage(REFERENCE_HEADERS.find((header) => header.source === 'engine/scene/scene.h')).markdown;
		assert.match(reference, /href="\\/versions\\/v0\\.3\\.0\\/docs\\/reference\\/"/);
		assert.doesNotMatch(reference, /href="\\/docs\\//);
		const renderer = new ExpressiveCode({ plugins: [apiCodeLinks] });
		const rendered = await renderer.render({ code: 'Scene scene;', language: 'cpp' });
		assert.match(JSON.stringify(rendered.renderedGroupAst), /\\/versions\\/v0\\.3\\.0\\/docs\\/reference\\/scene\\/scene\\/#class-scene/);
	`);
	assert.equal(result.status, 0, result.stderr);
});
