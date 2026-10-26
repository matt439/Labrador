import assert from 'node:assert/strict';
import { test } from 'node:test';
import { codeLink, freeFunctions, indexExplanations, referencePage, symbols } from '../src/lib/reference.ts';
import { REFERENCE_HEADERS } from '../src/lib/published.ts';
import { referenceMentions } from '../src/lib/reference-mentions.ts';
import { referenceLinks } from '../src/lib/markdown-plugins.ts';
import { markdownToHtml } from 'satteri';

const resolver = {
	codeLink,
	hasType: (name) => symbols().has(name),
	hasFunction: (name) => freeFunctions().has(name),
};

test('known guide and concept API links generate return links for the six teaching pages', () => {
	const pages = new Map([
		['engine/input/gamepads.h', '/docs/guides/input/'],
		['engine/scene/scene.h', '/docs/concepts/drawing/'],
		['engine/render/label.h', '/docs/guides/sprites-and-text/'],
		['engine/render/camera.h', '/docs/concepts/drawing/'],
		['engine/app/application.h', '/docs/concepts/game/'],
		['engine/render/renderer.h', '/docs/guides/sprites-and-text/'],
	]);
	for (const [source, target] of pages) {
		const header = REFERENCE_HEADERS.find((entry) => entry.source === source);
		const section = referencePage(header).markdown.split('## Explained in\n')[1];
		assert.ok(section, source);
		assert.ok(section.includes(`href="${target}"`), `${source}: ${target}`);
	}
});

test('explanation links follow current source mentions and do not depend on source order', () => {
	const guide = {
		source: 'guide.mdx', title: 'Read a pad', url: '/docs/guides/read-a-pad/',
		markdown: '`Gamepads` reads four slots. `Gamepads::state` reads a snapshot.',
	};
	const concept = {
		source: 'concept.mdx', title: 'Devices', url: '/docs/concepts/devices/',
		markdown: '[The pads](/docs/reference/input/gamepads/#class-gamepads) are borrowed.',
	};
	const target = '/docs/reference/input/gamepads/';
	const links = indexExplanations([guide, concept]).get(target);
	assert.deepEqual(links, [
		{ title: 'Devices', url: concept.url }, { title: 'Read a pad', url: guide.url },
	]);
	assert.deepEqual(indexExplanations([concept, guide]).get(target), links);
	assert.deepEqual(indexExplanations([{ ...guide, markdown: 'Read the devices.' }, concept]).get(target),
		[{ title: 'Devices', url: concept.url }]);
	assert.equal(indexExplanations([{ ...guide, markdown: 'Read the devices.' }]).has(target), false);
});

test('mentions use rendered inline links, C++ and source excerpts, not imports or unlinked text', () => {
	const markdown = [
		'import Scene from "not-an-api-link";',
		'## `Renderer`',
		'[`Scene`](https://example.com/external) is an external link label.',
		'`Gamepads` is linked, as is [the camera][camera].',
		'[camera]: /docs/reference/render/camera/#struct-camera',
		'```cpp',
		'// Application is only a comment.',
		'const char* name = "Renderer";',
		'Label greeting;',
		'```',
		'```text',
		'Application application;',
		'```',
		'<SourceFile path="sample.hpp" from="void draw" to="}" />',
		'<SourceFile path="data.json" />',
		'<SourceFile path="CMakeLists.txt" />',
	].join('\n\n');
	const passages = [];
	const mentions = referenceMentions(markdown, 'fixture.mdx', resolver, (source, from, to) => {
		passages.push([source, from, to]);
		return 'Scene scene;';
	});
	assert.deepEqual([...mentions].sort(), [
		'/docs/reference/input/gamepads/', '/docs/reference/render/camera/',
		'/docs/reference/render/label/', '/docs/reference/scene/scene/',
	]);
	assert.deepEqual(passages, [['sample.hpp', 'void draw', '}']]);
});

test('the explanation index reports source excerpts as cache dependencies', () => {
	const dependencies = new Set();
	indexExplanations([{
		source: 'guide.mdx', title: 'Rules', url: '/docs/guides/rules/',
		markdown: '<SourceFile path="samples/minimal/rules/motion.h" />',
	}], dependencies);
	assert.deepEqual([...dependencies], ['samples/minimal/rules/motion.h']);
});

test('explicit MDX reference links count but unused definitions do not', () => {
	const markdown = [
		'<a href="/docs/reference/scene/scene/#class-scene">The scene</a>',
		'<LinkCard href="/docs/reference/render/label/" title="Labels" />',
		'[unused]: /docs/reference/app/application/',
	].join('\n\n');
	assert.deepEqual([...referenceMentions(markdown, 'fixture.mdx', resolver)].sort(), [
		'/docs/reference/render/label/', '/docs/reference/scene/scene/',
	]);
});

test('the source index and Markdown plugin agree on manual, automatic and external links', async () => {
	const markdown = [
		'## `Renderer`',
		'[`Scene`](https://example.com/external) is external. `Scene` is automatic.',
		'[`Camera`](/docs/reference/render/camera/#struct-camera) is manual.',
		'`Gamepads` is automatic.',
	].join('\n\n');
	const { html } = await markdownToHtml(markdown, { mdastPlugins: [referenceLinks] });
	const rendered = new Set([...html.matchAll(/href="(\/docs\/reference\/[^"#]+)(?:#[^"]*)?"/g)]
		.map((match) => match[1]));
	assert.deepEqual([...referenceMentions(markdown, 'fixture.md', resolver)].sort(), [...rendered].sort());
	assert.ok(rendered.has('/docs/reference/scene/scene/'));
	assert.ok(!rendered.has('/docs/reference/render/renderer/'));
});
