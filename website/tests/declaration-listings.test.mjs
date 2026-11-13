// A page that shows a public declaration quotes it with a `SourceFile`
// excerpt, which fails the build when the header changes (src/lib/excerpt.ts).
// Some listings cannot be excerpts: they pick a few members out of a class,
// gather declarations from several scopes, or add a comment the header does
// not carry. Such a listing is a `cpp` code block titled with its header's
// path, as in ```cpp title="engine/assets/json.h"```, and this test reads it
// with the reference's header reader (src/lib/cpp-header.ts) and requires each
// declaration in it to be one the header declares, spelled the same apart from
// whitespace and comments. A type in a listing must be a type of that name in
// the header, and its members members of that type.
import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';
import { parseHeader } from '../src/lib/cpp-header.ts';

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

// Whitespace and line breaks are layout: a listing may join a declaration the
// header wraps, or wrap one the header keeps on a line.
function normalise(text) {
	return text.replace(/\s+/g, ' ').replace(/\(\s/g, '(').replace(/\s\)/g, ')').trim();
}

// Every declaration in a parsed header, by the type that holds it: '' for a
// namespace or the file, otherwise the type's own name.
function declarations(items, owner = '', found = new Map(), types = new Map()) {
	for (const item of items) {
		if (item.kind === 'group') {
			for (const declaration of item.declarations) {
				const key = `${owner}\u0000${normalise(declaration.text)}`;
				found.set(key, declaration);
			}
		} else if (item.kind === 'type') {
			types.set(item.name, item.keyword);
			declarations(item.items, item.name, found, types);
		}
	}
	return { found, types };
}

// A listing's own declarations, with the type each sits in ('' at the top).
function listed(items, owner = '', out = []) {
	for (const item of items) {
		if (item.kind === 'group') {
			for (const declaration of item.declarations) {
				out.push({ owner, text: normalise(declaration.text), line: declaration.line });
			}
		} else if (item.kind === 'type') {
			out.push({ owner, type: item.name, keyword: item.keyword, line: item.line });
			listed(item.items, item.name, out);
		}
	}
	return out;
}

const fence = /^([ \t]*)```cpp[^\n]*\btitle="(engine\/[^"]+\.h)"[^\n]*\n([\s\S]*?)^\1```/gm;

// The failures of one page, each naming the page and line. A declaration at
// the top of a listing may come from any scope of its header, so a listing can
// gather a free function and a member under a comment that says which.
export function checkPage(name, text, readHeader) {
	const failures = [];
	let checked = 0;
	for (const match of text.matchAll(fence)) {
		const [, indent, header, body] = match;
		const firstLine = text.slice(0, match.index).split('\n').length + 1;
		const source = readHeader(header);
		if (source === undefined) {
			failures.push(`${name}:${firstLine - 1}: the listing names ${header}, which does not exist`);
			continue;
		}
		const block = body.split('\n').map((line) => line.startsWith(indent) ? line.slice(indent.length) : line).join('\n');
		let listing;
		let parsed;
		try {
			listing = listed(parseHeader(`${name} (listing from line ${firstLine})`, block).items);
			parsed = declarations(parseHeader(header, source).items);
		} catch (error) {
			failures.push(`${name}:${firstLine - 1}: ${error.message}`);
			continue;
		}
		const anywhere = new Set([...parsed.found.keys()].map((key) => key.split('\u0000')[1]));
		for (const entry of listing) {
			const line = firstLine + entry.line;
			checked++;
			if (entry.type) {
				if (parsed.types.get(entry.type) !== entry.keyword) {
					failures.push(`${name}:${line}: ${header} declares no ${entry.keyword} ${entry.type}`);
				}
			} else if (entry.owner === '' ? !anywhere.has(entry.text) : !parsed.found.has(`${entry.owner}\u0000${entry.text}`)) {
				const where = entry.owner === '' ? header : `${entry.owner} in ${header}`;
				failures.push(`${name}:${line}: \`${entry.text}\` is not a declaration of ${where}`);
			}
		}
	}
	return { failures, checked };
}

function readHeader(path) {
	const full = join(repository, path);
	return existsSync(full) ? read(full) : undefined;
}

test('every declaration listing titled with an engine header matches that header', () => {
	const failures = [];
	let checked = 0;
	for (const path of files(docs).filter((file) => /\.mdx?$/.test(file))) {
		const result = checkPage(relative(docs, path).replaceAll('\\', '/'), read(path), readHeader);
		failures.push(...result.failures);
		checked += result.checked;
	}
	assert.ok(checked >= 17, `only ${checked} listed declarations found; the listings or the title pattern changed`);
	assert.deepEqual(failures, []);
});

test('a drifted listing fails naming the page and line', () => {
	const header = [
		'#pragma once',
		'',
		'namespace labrador',
		'{',
		'\tint free_function(int value);',
		'',
		'\tclass Holder',
		'\t{',
		'\tpublic:',
		'\t\tstruct Part',
		'\t\t{',
		'\t\t\tint count;',
		'\t\t};',
		'',
		'\t\tvoid take(const Part& part,',
		'\t\t\tbool optional) const;',
		'\t};',
		'}',
		'',
	].join('\n');
	const headers = (path) => (path === 'engine/core/holder.h' ? header : undefined);
	const page = [
		'---',
		'title: Example',
		'---',
		'',
		'```cpp title="engine/core/holder.h"',
		'int free_function(int value);   // a trailing comment is not compared',
		'void take(const Part& part, bool optional) const;',
		'void take(const Part& part) const;',
		'',
		'struct Part',
		'{',
		'    int count;',
		'    int size;',
		'};',
		'```',
		'',
		'```cpp title="engine/core/missing.h"',
		'int nothing();',
		'```',
		'',
	].join('\n');
	const { failures, checked } = checkPage('docs/example.mdx', page, headers);
	assert.equal(checked, 6);
	assert.deepEqual(failures, [
		'docs/example.mdx:8: `void take(const Part& part) const;` is not a declaration of engine/core/holder.h',
		'docs/example.mdx:13: `int size;` is not a declaration of Part in engine/core/holder.h',
		'docs/example.mdx:17: the listing names engine/core/missing.h, which does not exist',
	]);
});
