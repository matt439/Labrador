import assert from 'node:assert/strict';
import { test } from 'node:test';
import {
	codeLink, commentHtml, explanationDependencies, forgetIndexes,
	freeFunctions, indexExplanations, moduleReferencePage, referencePage, symbols,
} from '../src/lib/reference.ts';
import { REFERENCE_HEADERS, REFERENCE_MODULES } from '../src/lib/published.ts';

function snapshot(headers) {
	// Call order may vary, but each page's bytes and the exported maps' actual
	// iteration order remain part of the comparison.
	const pages = headers.map(referencePage).sort((a, b) =>
		a.header.source < b.header.source ? -1 : a.header.source > b.header.source ? 1 : 0);
	return {
		pages,
		modules: REFERENCE_MODULES.map((module) => ({
			name: module.name, markdown: moduleReferencePage(module),
		})),
		symbols: [...symbols()],
		functions: [...freeFunctions()],
		dependencies: [...explanationDependencies()],
	};
}

test('index caches reuse maps, preserve borrowed snapshots and defer explanation discovery', () => {
	forgetIndexes();
	try {
		assert.deepEqual(explanationDependencies(), []);
		const originalSymbols = symbols();
		const originalFunctions = freeFunctions();
		assert.equal(symbols(), originalSymbols);
		assert.equal(freeFunctions(), originalFunctions);
		assert.ok(codeLink('Scene::draw()', 'cache fixture'));
		indexExplanations([{
			source: 'cache-fixture.mdx', title: 'Scene', url: '/cache-fixture/',
			markdown: '`Scene` owns objects.',
		}]);
		assert.deepEqual(explanationDependencies(), [], 'index-only calls do not render guide return links');
		const symbolEntries = [...originalSymbols];
		const functionEntries = [...originalFunctions];
		referencePage(REFERENCE_HEADERS[0]);
		const dependencies = [...explanationDependencies()];
		assert.ok(dependencies.length > 0, 'rendering a reference discovers authored source excerpts');

		forgetIndexes();
		assert.deepEqual(explanationDependencies(), []);
		assert.notEqual(symbols(), originalSymbols);
		assert.notEqual(freeFunctions(), originalFunctions);
		assert.deepEqual([...originalSymbols], symbolEntries, 'reset does not clear maps held by callers');
		assert.deepEqual([...originalFunctions], functionEntries);
		assert.deepEqual([...symbols()], symbolEntries);
		assert.deepEqual([...freeFunctions()], functionEntries);
		assert.deepEqual(explanationDependencies(), []);
		referencePage(REFERENCE_HEADERS[0]);
		assert.deepEqual(explanationDependencies(), dependencies);
	} finally {
		forgetIndexes();
	}
});

test('cold, warm and reset reference output is independent of initialization and render order', () => {
	forgetIndexes();
	try {
		const cold = snapshot(REFERENCE_HEADERS);
		const reversed = [...REFERENCE_HEADERS].reverse();
		assert.deepEqual(snapshot(reversed), cold, 'warm caches preserve every page byte');
		for (const initialize of [
			() => freeFunctions(),
			() => codeLink('Scene::draw()', 'cache fixture'),
			() => commentHtml({ line: 0, lines: ['Use `Scene::draw()` and T1.'] }),
		]) {
			forgetIndexes();
			initialize();
			assert.deepEqual(explanationDependencies(), []);
			assert.deepEqual(snapshot(reversed), cold);
		}
	} finally {
		forgetIndexes();
	}
});

test('forgetIndexes refreshes a changed published inventory without mutating old maps', () => {
	const count = REFERENCE_HEADERS.length;
	forgetIndexes();
	try {
		const original = freeFunctions();
		assert.equal(original.has('test_first::overloaded'), false);
		REFERENCE_HEADERS.push({
			module: 'test', source: 'website/tests/fixtures/reference-functions.h',
			slug: 'docs/reference/test/cache-functions', title: 'Cache functions',
			order: 1, audience: 'game code',
		});
		assert.equal(freeFunctions(), original, 'inventory edits require explicit invalidation');
		assert.equal(original.has('test_first::overloaded'), false);
		forgetIndexes();
		const refreshed = freeFunctions();
		assert.notEqual(refreshed, original);
		assert.equal(codeLink('test_first::overloaded', 'cache fixture'),
			'/docs/reference/test/cache-functions/#overloaded');
		assert.equal(original.has('test_first::overloaded'), false);
		const refreshedEntries = [...refreshed];
		REFERENCE_HEADERS.splice(count);
		forgetIndexes();
		assert.equal(freeFunctions().has('test_first::overloaded'), false);
		assert.deepEqual([...refreshed], refreshedEntries);
	} finally {
		REFERENCE_HEADERS.splice(count);
		forgetIndexes();
	}
});
