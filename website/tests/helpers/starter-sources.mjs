// The text files placed in a starter project. Keep the include/comment edits
// in step with assemble.py; its Python fixture compares these outputs with
// the actual assembler. The website gate needs only Node to read this model.
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';

function files(folder) {
	return readdirSync(folder).flatMap((name) => {
		const path = join(folder, name);
		return statSync(path).isDirectory() ? files(path) : [path];
	});
}

function read(path) {
	return readFileSync(path, 'utf8').replaceAll('\r\n', '\n');
}

function editedComments(source, replacements, html) {
	let insideHtml = false;
	return source.split('\n').map((line) => {
		const prefix = line.match(/^[ \t]*/)[0];
		const body = line.slice(prefix.length);
		if (html && body.startsWith('<!--')) insideHtml = true;
		const comment = html ? insideHtml : body.startsWith('//');
		const result = comment && Object.hasOwn(replacements, body)
			? prefix + replacements[body] : line;
		if (html && body.includes('-->')) insideHtml = false;
		return result;
	}).join('\n');
}

export function starterSources(repository) {
	const sample = join(repository, 'samples/minimal');
	const walkthrough = join(repository, 'website/src/walkthrough');
	const comments = JSON.parse(read(join(walkthrough, 'game-comments.json')));
	const sources = new Map();
	for (const path of files(sample)) {
		if (!/\.(?:h|cpp|html|json|md|txt|manifest)$/.test(path)) continue;
		const name = relative(sample, path).replaceAll('\\', '/');
		let text = read(path);
		if (/\.(?:h|cpp)$/.test(path)) {
			text = text.replaceAll(/^#include "samples\/minimal\//gm, '#include "game/');
		}
		if (/\.(?:h|cpp|html)$/.test(path)) {
			text = editedComments(text, comments[name] ?? {}, path.endsWith('.html'));
		}
		sources.set(`game/${name}`, text);
	}
	const placed = {
		'root-CMakeLists.txt': 'CMakeLists.txt',
		'game-CMakeLists.txt': 'game/CMakeLists.txt',
		'game-README.md': 'game/README.md',
		'CMakePresets.json': 'CMakePresets.json',
		'gitignore.txt': '.gitignore',
		'ci.yml': '.github/workflows/ci.yml',
	};
	for (const [source, destination] of Object.entries(placed)) {
		sources.set(destination, read(join(walkthrough, source)));
	}
	const manifest = JSON.parse(read(join(repository, 'vcpkg.json')));
	sources.set('vcpkg.json', JSON.stringify({
		name: 'mygame',
		'version-string': '0.1.0',
		dependencies: manifest.dependencies,
		'builtin-baseline': manifest['builtin-baseline'],
	}, null, 2) + '\n');
	return sources;
}
