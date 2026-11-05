import assert from 'node:assert/strict';
import { test } from 'node:test';
import { excerpt } from '../src/lib/excerpt.ts';

test('an excerpt drops the indentation its lines share, whether spaces or tabs', () => {
	// vcpkg.json is indented with spaces; its features list sits six deep.
	const spaces = excerpt('vcpkg.json', '"features"', '],');
	const json = spaces.code.split('\n');
	assert.equal(spaces.whole, false);
	assert.equal(json[0], '"features": [');
	assert.equal(json.at(-1), '],');
	assert.ok(json.slice(1, -1).every((line) => /^ {2}\S/.test(line)), spaces.code);
	// Samples are indented with tabs; main's try block sits one deep.
	const tabs = excerpt('samples/minimal/main_web.cpp', 'try', '}');
	const cpp = tabs.code.split('\n');
	assert.deepEqual([cpp[0], cpp[1], cpp.at(-1)], ['try', '{', '}']);
	assert.ok(cpp.slice(2, -1).every((line) => line === '' || line.startsWith('\t')), tabs.code);
});
