import assert from 'node:assert/strict';
import { test } from 'node:test';
import { parseHeader } from '../src/lib/cpp-header.ts';
import { codeLink, commentHtml, forgetIndexes, freeFunctions, referencePage } from '../src/lib/reference.ts';
import { REFERENCE_HEADERS } from '../src/lib/published.ts';

test('declaration namespaces survive sibling and nested scopes; detail stays unpublished', () => {
	const document = parseHeader('fixture.h', `namespace first
{
	void same_name();
	namespace nested
	{
		void nested_name();
	}
	namespace detail
	{
		void hidden_name();
	}
}
namespace second::nested
{
	void same_name();
}
namespace second::detail
{
	void hidden_name();
}
void global_name();`);
	const declarations = document.items.flatMap((item) => item.kind === 'group' ? item.declarations : []);
	assert.deepEqual(declarations.map(({ name, namespace }) => [name, namespace]), [
		['same_name', 'first'], ['nested_name', 'first::nested'],
		['same_name', 'second::nested'], ['global_name', ''],
	]);
});

test('free functions link to generated headings, including an overload group', () => {
	assert.equal(codeLink('labrador::find_contacts', 'fixture'), '/docs/reference/collision/contacts/#find_contacts');
	assert.equal(codeLink('find_contacts(objects, contacts)', 'fixture'), '/docs/reference/collision/contacts/#find_contacts');
	assert.equal(codeLink('mattmath::are_equal(a, b)', 'fixture'), '/docs/reference/math/scalar/#are_equal');
	assert.equal(codeLink('are_equal', 'fixture'), '/docs/reference/math/scalar/#are_equal');
	assert.equal(codeLink('std::clamp', 'fixture'), undefined);
	assert.equal(codeLink('object.clamp()', 'fixture'), undefined);
	assert.equal(codeLink('object->clamp()', 'fixture'), undefined);
	assert.equal(codeLink('inflate_convex_polygon', 'fixture'), undefined);
	assert.equal(codeLink('labrador::StateContext::push', 'fixture'), '/docs/reference/core/state-context/#push');
	assert.equal(codeLink('mattmath::Scene', 'fixture'), undefined);
	assert.equal(codeLink('Key::w', 'fixture'), codeLink('Key', 'fixture'));
});

test('ambiguous names have no guessed target; same-page overloads retain the first heading', () => {
	const count = REFERENCE_HEADERS.length;
	const fixture = {
		module: 'test', source: 'website/tests/fixtures/reference-functions.h',
		slug: 'docs/reference/test/functions', title: 'Test functions', order: 1,
	};
	REFERENCE_HEADERS.push(fixture, {
		...fixture, source: 'website/tests/fixtures/reference-overloads.h',
		slug: 'docs/reference/test/overloads',
	});
	forgetIndexes();
	try {
		assert.equal(codeLink('test_first::shared_name', 'fixture'), '/docs/reference/test/functions/#shared_name');
		assert.equal(codeLink('test_second::shared_name', 'fixture'), '/docs/reference/test/functions/#shared_name-1');
		assert.equal(codeLink('shared_name', 'fixture'), undefined);
		assert.equal(codeLink('test_first::overloaded', 'fixture'), '/docs/reference/test/functions/#overloaded');
		assert.equal(codeLink('overloaded', 'fixture'), '/docs/reference/test/functions/#overloaded');
		assert.equal(codeLink('test_first::spread_out', 'fixture'), undefined);
		assert.equal(codeLink('spread_out', 'fixture'), undefined);
		assert.equal(codeLink('global_function', 'fixture'), '/docs/reference/test/functions/#global_function');
		for (const name of ['Alias', 'initialised', 'callback', 'void', 'member', 'hidden']) {
			assert.equal(freeFunctions().has(name), false, name);
		}
		assert.match(referencePage(fixture).markdown, /^## overloaded\n/m);
	} finally {
		REFERENCE_HEADERS.splice(count);
		forgetIndexes();
	}
});

test('header prose links functions without linking member calls or ordinary words', () => {
	const html = commentHtml({ line: 0, lines: [
		'Use `find_contacts`, mattmath::clamp() and `labrador::Scene`. A clamp holds it.',
		'`object.clamp()` and `object->clamp()` are member calls; `std::clamp` is external.',
	] });
	assert.match(html, /href="\/docs\/reference\/collision\/contacts\/#find_contacts"/);
	assert.match(html, /href="\/docs\/reference\/math\/scalar\/#clamp"/);
	assert.match(html, /href="\/docs\/reference\/scene\/scene\/#class-scene"/);
	assert.match(html, /A clamp holds it/);
	assert.match(html, /<code>object\.clamp\(\)<\/code>/);
	assert.match(html, /<code>object-&gt;clamp\(\)<\/code>/);
	assert.match(html, /<code>std::clamp<\/code>/);
	assert.equal(commentHtml({ line: 0, lines: ['    Scene scene;', '    find_contacts(objects, contacts);'] }),
		'```cpp\nScene scene;\nfind_contacts(objects, contacts);\n```');
});
