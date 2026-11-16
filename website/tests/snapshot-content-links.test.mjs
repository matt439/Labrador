// Catch authored source links that would make a release snapshot fail after
// its complete build. RepoLink renders the selected source commit; a literal
// branch URL keeps moving even when its surrounding guide is archived.
import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test } from 'node:test';
import { defineMdastPlugin, mdxToJs } from 'satteri';

const CONTENT = fileURLToPath(new URL('../src/content/docs/', import.meta.url));

function sourceLink(url) {
	let parsed;
	try { parsed = new URL(url, 'https://labradorengine.com'); } catch { return null; }
	if (parsed.hostname !== 'github.com') return null;
	return /^\/matt439\/labrador\/(blob|tree|edit)\/([^/]+)(?:\/|$)/i.exec(parsed.pathname);
}

async function unpinnedLinks(source) {
	const links = [];
	const definitions = new Map();
	const references = new Set();
	const jsx = node => {
		const attributes = new Map(node.attributes.filter(attribute => attribute.type === 'mdxJsxAttribute')
			.map(attribute => [attribute.name, attribute.value]));
		let url = attributes.get('href');
		// Recognise plain literal expressions without evaluating JavaScript.
		// Dynamic/escaped expressions remain checked in the built snapshot.
		if (url?.type === 'mdxJsxAttributeValueExpression') {
			const literal = /^(?:"([^"\\\r\n]*)"|'([^'\\\r\n]*)')$/.exec(url.value.trim());
			url = literal?.[1] ?? literal?.[2];
		}
		if (typeof url === 'string') links.push({ url, historical: node.name === 'a' && attributes.has('data-release-example') });
	};
	const collect = defineMdastPlugin({
		name: 'snapshot-source-link-preflight',
		link(node) { links.push({ url: node.url }); },
		definition(node) { definitions.set(node.identifier.toLowerCase(), node.url); },
		linkReference(node) { references.add(node.identifier.toLowerCase()); },
		mdxJsxFlowElement: jsx,
		mdxJsxTextElement: jsx,
	});
	await mdxToJs(source.replace(/^---\r?\n[\s\S]*?\r?\n---\r?\n/, ''), { mdastPlugins: [collect] });
	for (const reference of references) if (definitions.has(reference)) links.push({ url: definitions.get(reference) });
	return links.filter(({ url, historical }) => {
		const found = sourceLink(url);
		return found && !(historical && /^(?:blob|tree)$/i.test(found[1]) && /^v(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$/.test(found[2]));
	}).map(link => link.url);
}

function* pages(directory) {
	for (const entry of readdirSync(directory, { withFileTypes: true })) {
		const file = path.join(directory, entry.name);
		if (entry.isDirectory()) yield* pages(file);
		else if (/\.mdx?$/.test(entry.name)) yield file;
	}
}

test('Markdown, reference and JSX source links cannot freeze a moving branch into a snapshot', async () => {
	for (const source of [
		'[Record](https://github.com/matt439/Labrador/blob/master/docs/audio-input.md)',
		'[Pilot][record]\n\n[record]: https://github.com/matt439/Labrador/blob/master/tools/consumer_check/README.md#compiled-guide-pilot',
		'<a href="https://github.com/Matt439/labrador/tree/main/samples/">Samples</a>',
		'[Branch root](https://github.com/matt439/Labrador/tree/master)',
		'<a href={\'https://github.com/matt439/Labrador/tree/master\'}>Branch root</a>',
		'<LinkCard href="https://github.com/matt439/Labrador/blob/master/docs/audio-input.md" title="Record" />',
	]) assert.equal((await unpinnedLinks(source)).length, 1, source);
});

test('RepoLink, code examples and explicitly marked stable release history keep their scope', async () => {
	const source = [
		'<RepoLink path="docs/audio-input.md">Record</RepoLink>',
		'<a href="https://github.com/matt439/Labrador/blob/v0.2.0/CHANGES.md" data-release-example>Earlier release</a>',
		'[External](https://github.com/microsoft/DirectXTK/blob/main/README.md)',
		'```md\n[Example](https://github.com/matt439/Labrador/blob/master/example.md)\n```',
	].join('\n\n');
	assert.deepEqual(await unpinnedLinks(source), []);
	for (const url of ['blob/master/CHANGES.md', 'edit/v0.2.0/CHANGES.md', 'blob/v0.3.0-rc.1/CHANGES.md']) {
		assert.equal((await unpinnedLinks(`<a href="https://github.com/matt439/Labrador/${url}" data-release-example>Example</a>`)).length, 1);
	}
});

test('authored literal repository source links use the revision-aware component', async () => {
	const failures = [];
	for (const file of pages(CONTENT)) {
		for (const url of await unpinnedLinks(readFileSync(file, 'utf8'))) failures.push(`${path.relative(CONTENT, file)}: ${url}; use RepoLink`);
	}
	assert.deepEqual(failures, []);
});
