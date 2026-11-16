import assert from 'node:assert/strict';
import test from 'node:test';
import GithubSlugger from 'github-slugger';
import { declarationSignature } from '../src/lib/cpp-header.ts';
import { REFERENCE_HEADERS } from '../src/lib/published.ts';
import { codeLink, referencePage } from '../src/lib/reference.ts';
import { readRepositoryFile, REVISION } from '../src/lib/repository.ts';
import { COMMON_TASKS, QUICK_REFERENCE_SLUG, quickReferencePage, selectedDeclarations } from '../src/lib/quick-reference.ts';

test('the curated tasks generate published declarations and real contract anchors', () => {
	const markdown = quickReferencePage();
	assert.equal(COMMON_TASKS.length, 8);
	assert.match(markdown, /For game code after the starter is running/);
	assert.match(markdown, /linked full contracts own preconditions, errors, lifetimes and platform limits/);
	assert.ok(markdown.includes(REVISION.short));
	assert.ok(markdown.includes('/docs/guides/your-version/'));
	assert.ok(markdown.includes('[Browse all modules and headers](/docs/reference/)'));
	const selected = new Set();
	for (const task of COMMON_TASKS) {
		assert.ok(markdown.includes(`## ${task.title}\n`));
		assert.ok(task.api.length > 0);
		const guidePath = `website/src/content/docs${task.guide.url.replace(/\/$/, '')}.mdx`;
		assert.ok(readRepositoryFile(guidePath).startsWith('---'), guidePath);
		for (const selection of task.api) {
			assert.ok(!selected.has(selection.name), selection.name);
			selected.add(selection.name);
			const header = REFERENCE_HEADERS.find((header) => header.source === selection.header);
			assert.equal(header?.audience, 'game code');
			const contract = codeLink(selection.name, QUICK_REFERENCE_SLUG);
			const slugger = new GithubSlugger();
			const anchors = [...referencePage(header).markdown.matchAll(/^#{2,3} (.+)$/gm)]
				.map((match) => slugger.slug(match[1]));
			assert.ok(contract.startsWith(`/${header.slug}/#`), selection.name);
			assert.ok(anchors.includes(contract.split('#')[1]), contract);
			assert.ok(markdown.includes(`[\`${selection.name}\`](${contract})`));
			for (const declaration of selectedDeclarations(selection, readRepositoryFile(selection.header))) {
				assert.ok(markdown.includes(declaration.text), `${selection.header}:${declaration.line + 1}`);
			}
		}
	}
	assert.equal(selected.size, 19);
});

test('changed defaults and added public overloads flow from the existing reader', () => {
	const selection = { header: 'fixture.h', name: 'Game::take' };
	const source = `namespace labrador
{
	class Game
	{
	public:
		void take(int count = 7);
		void take(float count) const;
	private:
		void take(bool hidden);
	};
}`;
	assert.deepEqual(selectedDeclarations(selection, source).map((entry) => entry.text), [
		'void take(int count = 7);', 'void take(float count) const;',
	]);
	assert.equal(selectedDeclarations(selection, source.replace('= 7', '= 9'))[0].text, 'void take(int count = 9);');
	assert.throws(() => selectedDeclarations(selection, source.replaceAll('take(', 'renamed(')), /Game::take.*fixture\.h.*no such public declaration/);
	assert.throws(() => selectedDeclarations(selection, source.replace('public:', 'private:')), /no such public declaration/);
	assert.throws(() => selectedDeclarations(selection, source.replace('namespace labrador', 'namespace other')), /no such public declaration/);
});

test('the shared signature formatter strips implementation while retaining caller defaults', () => {
	assert.equal(declarationSignature('Game(int value = 3) : value_(value) {}'), 'Game(int value = 3);');
	assert.equal(declarationSignature('bool valid() const { return value_ != 0; }'), 'bool valid() const;');
	assert.equal(declarationSignature('Game() = default;'), 'Game() = default;');
	assert.equal(declarationSignature('int operator()(int count = 2) const { return count; }'), 'int operator()(int count = 2) const;');
	const scene = selectedDeclarations({ header: 'engine/scene/scene.h', name: 'Scene::add' }, readRepositoryFile('engine/scene/scene.h'));
	assert.equal(scene.length, 1);
	assert.match(scene[0].text, /^template <typename T>\nT\* add\(std::unique_ptr<T> object\);$/);
	assert.ok(!scene[0].text.includes('pending_objects_'));
});

test('unknown and non-game headers fail instead of quietly publishing a partial task', () => {
	const task = (header, name) => [{ title: 'Fixture', summary: '', guide: { title: 'Input', url: '/docs/guides/input/' }, api: [{ header, name }] }];
	assert.throws(() => quickReferencePage(task('engine/render/not_a_header.h', 'Thing::take')), /published game-code header/);
	assert.throws(() => quickReferencePage(task('engine/render/resource_factory.h', 'ResourceFactory::take')), /published game-code header/);
	assert.throws(() => quickReferencePage(task('engine/input/keyboard.h', 'Keyboard::not_a_member')), /no such public declaration/);
});
