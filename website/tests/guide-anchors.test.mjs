// A guide step that says 'search for `...`', 'Search with **Ctrl+F** for
// `...`', 'find the line `...`' or 'find the next `...`' sends
// the reader to a line of their own project. A `SourceFile` excerpt fails the
// build when its file drifts (src/lib/excerpt.ts); these prose anchors are
// checked here instead. Each must occur in a file the reader's project starts
// with, or in text an earlier step told them to add.
import assert from 'node:assert/strict';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';

const repository = fileURLToPath(new URL('../../', import.meta.url));
const docs = join(repository, 'website/src/content/docs');

function files(folder) {
	return readdirSync(folder).flatMap((name) => {
		const path = join(folder, name);
		return statSync(path).isDirectory() ? files(path) : [path];
	});
}

function read(path) {
	return readFileSync(path, 'utf8').replaceAll('\r\n', '\n');
}

// What the starter's project holds before any guide: samples/minimal as
// game/, with its includes edited as tools/consumer_check/assemble.py edits
// them, and the walkthrough's build files.
const starter = [
	...files(join(repository, 'samples/minimal')).map((path) =>
		read(path).replaceAll('#include "samples/minimal/', '#include "game/')),
	...files(join(repository, 'website/src/walkthrough')).map(read),
];

// The components 'Copy the components into your game' copies into game/components.
const components = ['option_row.h', 'option_row.cpp', 'slider.h', 'slider.cpp']
	.map((name) => read(join(repository, 'samples/components', name)));

// A page that continues another guide's finished game also accepts that
// guide's code blocks. The page must link to the guide it names.
const buildsOn = {
	'docs/guides/settings-and-saves.mdx': ['docs/guides/your-components.mdx'],
	'docs/guides/your-two-players.mdx': ['docs/guides/your-camera.mdx'],
};

const anchor = /\b(?:search for|search with \*\*Ctrl\+F\*\* for|find the line|find the next)\s+`([^`]+)`/gi;

function codeBlocks(text) {
	return [...text.matchAll(/```[^\n]*\n([\s\S]*?)```/g)].map((block) => block[1]);
}

function pageName(path) {
	return relative(docs, path).replaceAll('\\', '/');
}

const pages = files(docs).filter((path) => /\.mdx?$/.test(path));

test('each guide page another page builds on exists and is linked from it', () => {
	for (const [page, earlier] of Object.entries(buildsOn)) {
		const text = read(join(docs, page));
		for (const other of earlier) {
			const link = `](/${other.replace(/\.mdx?$/, '/')})`;
			assert.ok(text.includes(link), `${page} names ${other} as the guide it continues but does not link to it`);
		}
	}
});

test('every search-for anchor occurs in the starter, the copied components or an earlier step', () => {
	const failures = [];
	let checked = 0;
	for (const path of pages) {
		const name = pageName(path);
		const text = read(path);
		const continued = (buildsOn[name] ?? []).flatMap((other) => codeBlocks(read(join(docs, other))));
		for (const match of text.matchAll(anchor)) {
			const quoted = match[1];
			const before = text.slice(0, match.index);
			const line = before.split('\n').length;
			checked++;
			if (quoted.includes('\n')) {
				failures.push(`${name}:${line}: the anchor \`${quoted}\` wraps onto a second line; a reader searches for one line`);
				continue;
			}
			const sources = [...starter, ...components, ...continued, ...codeBlocks(before)];
			if (!sources.some((source) => source.includes(quoted))) {
				failures.push(`${name}:${line}: \`${quoted}\` is in no starter file, copied component or earlier code block`);
			}
		}
	}
	assert.ok(checked > 100, `only ${checked} anchors found; the pattern no longer matches the guides' wording`);
	assert.deepEqual(failures, []);
});
