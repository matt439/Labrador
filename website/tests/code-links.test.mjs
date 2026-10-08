import assert from 'node:assert/strict';
import test from 'node:test';
import { ExpressiveCode } from '@astrojs/starlight/expressive-code';
import { apiCodeLinks, codeLinks } from '../src/lib/code-links.ts';

function matches(code) {
	return codeLinks(code).map(({ start, end, url }) => ({ text: code.slice(start, end), url }));
}

function descendants(node, predicate) {
	return [
		...(predicate(node) ? [node] : []),
		...(node.children ?? []).flatMap((child) => descendants(child, predicate)),
	];
}

function text(node) {
	return node.type === 'text' ? node.value : (node.children ?? []).map(text).join('');
}

function hasClass(node, name) {
	return node.type === 'element' && node.properties.className?.includes(name);
}

test('qualified engine types, members and free calls reach their documented sections', () => {
	assert.deepEqual(matches([
		'labrador::Scene scene;',
		'mattmath::Vector2F point;',
		'auto draw = &labrador::Scene::draw;',
		'auto path = labrador::path_from_utf8("assets/test.dds");',
	].join('\n')), [
		{ text: 'labrador::Scene', url: '/docs/reference/scene/scene/#class-scene' },
		{ text: 'mattmath::Vector2F', url: '/docs/reference/math/vector2f/#struct-vector2f' },
		{ text: 'labrador::Scene::draw', url: '/docs/reference/scene/scene/#draw' },
		{ text: 'labrador::path_from_utf8', url: '/docs/reference/core/file-path/#path_from_utf8--path_to_utf8' },
	]);
});

test('a block links each destination once, even across aliases and function families', () => {
	const code = [
		'Scene first;',
		'labrador::Scene second;',
		'::Scene third;',
		'path_from_utf8("one");',
		'labrador::path_from_utf8("two");',
		'path_to_utf8(path);',
	].join('\n');
	assert.deepEqual(matches(code).map(({ text }) => text), ['Scene', 'path_from_utf8']);
	assert.equal(new Set(codeLinks(code).map(({ url }) => url)).size, 2);
	assert.deepEqual(matches('Scene another;').map(({ text }) => text), ['Scene']);
});

test('comments and literals cannot consume the first real API mention', () => {
	const code = String.raw`
// Scene path_from_utf8("ignored")
/* labrador::Scene
   path_from_utf8("ignored") */
auto ordinary = "Scene \"quoted\" path_from_utf8(ignored)";
auto wide = L"Scene path_from_utf8(ignored)";
auto raw = u8R"delimiter(Scene
path_from_utf8("ignored"))delimiter";
auto character = '\'';
auto number = 1'000;
Scene scene;
path_from_utf8("actual");`;
	assert.deepEqual(matches(code).map(({ text }) => text), ['Scene', 'path_from_utf8']);
	for (const link of codeLinks(code)) {
		assert.ok(link.start > code.indexOf('1\'000;'));
	}
});

test('external namespaces, object calls, larger identifiers and bare function values stay plain', () => {
	const code = [
		'other::Scene scene;',
		'other::labrador::Scene another;',
		'other::path_from_utf8("external");',
		'object.path_from_utf8("method");',
		'pointer->path_from_utf8("method");',
		'auto callback = path_from_utf8;',
		'MyScene scene2;',
		'path_from_utf8_suffix("unrelated");',
	].join('\n');
	assert.deepEqual(codeLinks(code), []);
});

test('comments between member access and a method do not make a free-function link', () => {
	assert.deepEqual(codeLinks('object. /* explanation */ path_from_utf8("method");'), []);
	assert.deepEqual(codeLinks('pointer-> // explanation\n path_from_utf8("method");'), []);
});

test('rendered code keeps text, syntax highlighting, copy payload and native links', async () => {
	const source = 'labrador::Scene scene;\npath_from_utf8("assets/test.dds");\nScene duplicate;';
	const renderer = new ExpressiveCode({ plugins: [apiCodeLinks] });
	const { renderedGroupAst } = await renderer.render({ code: source, language: 'cpp' });
	assert.equal(descendants(renderedGroupAst, (node) => node.type === 'root').length, 0,
		'annotations must flatten fragments into valid element children for the Markdown processor');
	const lines = descendants(renderedGroupAst, (node) => hasClass(node, 'code'));
	assert.equal(lines.map(text).join('\n'), source);
	const links = descendants(renderedGroupAst, (node) => node.type === 'element' && node.tagName === 'a');
	assert.deepEqual(links.map(text), ['labrador::Scene', 'path_from_utf8']);
	for (const link of links) {
		assert.ok(link.properties.href.startsWith('/docs/reference/'));
		assert.ok(hasClass(link, 'api-link'));
		assert.equal(link.properties.tabIndex, undefined, 'native anchors remain keyboard accessible');
		assert.ok(descendants(link, (node) => node.type === 'element' && node.tagName === 'span' && node.properties.style).length,
			'syntax highlight spans remain inside the link');
	}
	const copy = descendants(renderedGroupAst, (node) => node.type === 'element' && node.tagName === 'button' && node.properties.dataCode)[0];
	assert.ok(copy, 'the standard copy button remains present');
	assert.equal(copy.properties.dataCode.replace(/\u007f/g, '\n'), source);
});

test('non-C++ snippets are not linked', async () => {
	const renderer = new ExpressiveCode({ plugins: [apiCodeLinks] });
	const { renderedGroupAst } = await renderer.render({ code: 'Scene path_from_utf8()', language: 'text' });
	assert.equal(descendants(renderedGroupAst, (node) => node.type === 'element' && node.tagName === 'a').length, 0);
});

test('a highlighted substring does not split a qualified API link into keyboard stops', async () => {
	const renderer = new ExpressiveCode({ plugins: [apiCodeLinks] });
	const { renderedGroupAst } = await renderer.render({
		code: 'labrador::Scene scene;', language: 'cpp', props: { mark: 'Scene' },
	});
	const links = descendants(renderedGroupAst, (node) => node.type === 'element' && node.tagName === 'a');
	assert.equal(links.length, 1);
	assert.equal(text(links[0]), 'labrador::Scene');
	assert.equal(descendants(renderedGroupAst, (node) => node.type === 'root').length, 0);
	assert.ok(descendants(links[0], (node) => node.type === 'element' && node.tagName === 'mark').length,
		'the requested substring marker remains inside the single link');
});
