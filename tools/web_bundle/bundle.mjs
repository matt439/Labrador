// Assembles what a host page serves for one browser sample: the module, the
// files around it, the third-party notices for what the module links, and a
// record of which commit it is.
//
//   node bundle.mjs --out <folder> --repository <checkout> --emscripten <root>
//                   --build-type <type>
//                   --file <name>=<path> ...
//                   --notice <preamble> --licence <title>=<path> ...
//
// samples/linesweeper/CMakeLists.txt is the caller, as the build's
// linesweeper_web_dist target, and runs it on every build.
//
// WHAT IT WRITES, into --out, which it empties first so that a file dropped
// from the list is dropped from the bundle:
//
// - every --file, copied under its name;
// - NOTICE.txt: --notice's text, then each --licence in full under its title,
//   and a licence whose text is byte for byte one already written is named
//   beside it rather than written twice (libc++, libc++abi and compiler-rt
//   share one);
// - BUILD.json: the commit the checkout was at, whether it had changes not in
//   that commit, the build type, the Emscripten version read from --emscripten,
//   and for every file its size, its size under Brotli at quality 11 - what a
//   host compressing ahead of time serves - and its SHA-256.
//
// The commit is read when the bundle is made, not when the module was linked.
// The build runs this after every link and on every build, so the two differ
// only for a bundle copied out of a build directory nobody has built since the
// checkout moved, and "dirty" is the guard against the other way round.
//
// Node's own modules only, so there is nothing to install: Emscripten's
// toolchain already runs its tests under this Node.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { basename, join } from 'node:path';
import { brotliCompressSync, constants } from 'node:zlib';

const options = parse_arguments(process.argv.slice(2));

function parse_arguments(argv) {
	const parsed = { file: [], licence: [] };
	for (let i = 0; i < argv.length; i += 2) {
		const name = argv[i];
		const value = argv[i + 1];
		if (!name.startsWith('--') || value === undefined) {
			fail(`unexpected argument "${name}"`);
		}
		const key = name.slice(2);
		if (key === 'file' || key === 'licence') {
			const equals = value.indexOf('=');
			if (equals <= 0) {
				fail(`--${key} takes <name>=<path>, not "${value}"`);
			}
			parsed[key].push({ name: value.slice(0, equals), path: value.slice(equals + 1) });
		} else if (['out', 'repository', 'emscripten', 'build-type', 'notice'].includes(key)) {
			parsed[key] = value;
		} else {
			fail(`unknown option ${name}`);
		}
	}
	for (const required of ['out', 'repository', 'emscripten', 'notice']) {
		if (parsed[required] === undefined) {
			fail(`--${required} is required`);
		}
	}
	if (parsed.file.length === 0) {
		fail('--file is required at least once');
	}
	return parsed;
}

function fail(why) {
	process.stderr.write(`bundle: ${why}\n`);
	process.exit(2);
}

function git(...args) {
	return execFileSync('git', ['-C', options.repository, ...args], {
		encoding: 'utf8',
		stdio: ['ignore', 'pipe', 'pipe'],
	}).trim();
}

function brotli_size(bytes) {
	return brotliCompressSync(bytes, {
		params: {
			[constants.BROTLI_PARAM_QUALITY]: constants.BROTLI_MAX_QUALITY,
			[constants.BROTLI_PARAM_SIZE_HINT]: bytes.length,
		},
	}).length;
}

function notice_text() {
	let text = readFileSync(options.notice, 'utf8').replace(/\r\n/g, '\n').trimEnd() + '\n';
	const written = new Map();
	for (const licence of options.licence) {
		const body = readFileSync(licence.path, 'utf8').replace(/\r\n/g, '\n').trimEnd();
		const shared = written.get(body);
		const rule = '='.repeat(78);
		if (shared !== undefined) {
			text += `\n${rule}\n${licence.name}\n${rule}\n\nThe same text as ${shared}, above.\n`;
			continue;
		}
		written.set(body, licence.name);
		text += `\n${rule}\n${licence.name}\n${rule}\n\n${body}\n`;
	}
	return text;
}

rmSync(options.out, { recursive: true, force: true });
mkdirSync(options.out, { recursive: true });

for (const file of options.file) {
	if (basename(file.name) !== file.name) {
		fail(`--file names a file in the bundle's own folder, not "${file.name}"`);
	}
	copyFileSync(file.path, join(options.out, file.name));
}
writeFileSync(join(options.out, 'NOTICE.txt'), notice_text());

let commit = 'unknown';
let dirty = null;
try {
	commit = git('rev-parse', 'HEAD');
	dirty = git('status', '--porcelain', '--untracked-files=no').length > 0;
} catch {
	// A source tree that is not a checkout still bundles; BUILD.json says it
	// could not tell rather than inventing an answer.
}

const files = {};
for (const name of [...options.file.map((file) => file.name), 'NOTICE.txt'].sort()) {
	const bytes = readFileSync(join(options.out, name));
	files[name] = {
		bytes: bytes.length,
		brotli: brotli_size(bytes),
		sha256: createHash('sha256').update(bytes).digest('hex'),
	};
}

const build = {
	commit,
	dirty,
	build_type: options['build-type'] ?? null,
	emscripten: readFileSync(join(options.emscripten, 'emscripten-version.txt'), 'utf8')
		.trim().replace(/^"|"$/g, ''),
	files,
};
writeFileSync(join(options.out, 'BUILD.json'), JSON.stringify(build, null, '\t') + '\n');

const width = Math.max(...Object.keys(files).map((name) => name.length));
for (const [name, file] of Object.entries(files)) {
	process.stdout.write(`  ${name.padEnd(width)}  ${String(file.bytes).padStart(8)} bytes, ` +
		`${String(file.brotli).padStart(7)} under Brotli\n`);
}
process.stdout.write(`  from ${commit}${dirty ? ' with uncommitted changes' : ''}\n`);
