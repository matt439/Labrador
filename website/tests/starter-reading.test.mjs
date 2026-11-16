import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';
import { excerpt } from '../src/lib/excerpt.ts';
import { starterSources } from './helpers/starter-sources.mjs';

const repository = fileURLToPath(new URL('../../', import.meta.url));
const page = readFileSync(new URL('../src/content/docs/docs/concepts/read-your-code.mdx', import.meta.url), 'utf8');

// SourceFile checks the original template. This also checks the placed game,
// whose includes, README and selected comments are changed during assembly.
test('the beginner route quotes the assembled three files in their reading order', () => {
	const starter = starterSources(repository);
	const visited = [];
	let previousLine = -1;
	let checked = 0;
	for (const [, attributes] of page.matchAll(/<SourceFile\b([\s\S]*?)\/>/g)) {
		const properties = Object.fromEntries([...attributes.matchAll(/(\w+)="([^"]*)"/g)]
			.map(([, name, value]) => [name, value.replaceAll('&quot;', '"')]));
		assert.ok(properties.path && properties.from && properties.to,
			'each reading-route excerpt needs a path and maintained start/end anchors');
		const original = excerpt(properties.path, properties.from, properties.to);
		assert.ok(original.end >= original.start, properties.path);
		const placed = properties.path.replace(/^samples\/minimal\//, 'game/');
		assert.ok(starter.has(placed), `the assembled starter has no ${placed}`);
		const lines = starter.get(placed).split('\n');
		const from = properties.from.replaceAll('#include "samples/minimal/', '#include "game/');
		const to = properties.to.replaceAll('#include "samples/minimal/', '#include "game/');
		const starts = lines.flatMap((line, index) => line.trim().startsWith(from) ? [index] : []);
		assert.equal(starts.length, 1, `${placed}: the quoted start ${from} must occur once`);
		const start = starts[0];
		assert.ok(lines.slice(start).some((line) => line.trim().startsWith(to)),
			`${placed}: the quoted end ${to} is absent after ${from}`);
		if (visited.at(-1) !== placed) {
			visited.push(placed);
			previousLine = -1;
		}
		assert.ok(start > previousLine, `${placed}: the route moved backwards at ${from}`);
		previousLine = start;
		checked++;
	}
	assert.ok(checked >= 15, `only ${checked} maintained excerpts in the reading route`);
	assert.deepEqual(visited, ['game/main.cpp', 'game/states/hello_state.h', 'game/states/hello_state.cpp']);
});
