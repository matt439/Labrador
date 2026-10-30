// Assembles what a host page serves for one browser sample: the module, the
// files around it, the third-party notices for what the module links, and a
// record of which commit it is.
//
//   node bundle.mjs --out <folder> --repository <checkout> --emscripten <root>
//                   --build-type <type>
//                   --file <name>=<path> ...
//                   --notice <preamble> --licence <title>=<path> ...
//   node bundle.mjs --verify <linesweeper bundle folder>
//
// samples/linesweeper/CMakeLists.txt is the caller, as the build's
// linesweeper_web_dist target, and runs it on every build.
// --verify reads a copied LineSweeper bundle without changing it; its rules
// and provenance limits are documented in docs/port/web.md, section 7.1.
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
// CMake exports LABRADOR_SOURCE_REVISION to consumers at configure time. This
// build-time record reads Git again because a commit or a source edit need
// not cause CMake to reconfigure. Both count tracked modifications as dirty
// and require the source tree's own .git marker: an archive inside another
// checkout must not acquire that checkout's identity.
//
// Node's own modules only, so there is nothing to install: Emscripten's
// toolchain already runs its tests under this Node.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { copyFileSync, existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { basename, join } from 'node:path';
import { brotliCompressSync, constants } from 'node:zlib';

const options = parse_arguments(process.argv.slice(2));

if (options.verify !== undefined) {
	try {
		verify_bundle(options.verify);
	} catch (error) {
		fail(`verification failed: ${error.message}`);
	}
	process.exit(0);
}

function parse_arguments(argv) {
	if (argv[0] === '--verify') {
		if (argv.length !== 2) {
			fail('--verify takes only a LineSweeper bundle folder');
		}
		return { verify: argv[1] };
	}
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
	return execFileSync('git', ['--no-optional-locks', '-C', options.repository, ...args], {
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

// Verification reads only the supplied bundle. It checks the packaging
// record and bytes, not which compiler flags or sources produced the module.
function verify_bundle(folder) {
	const required = ['index.html', 'linesweeper.js', 'linesweeper.wasm', 'poster.png', 'NOTICE.txt'];
	const manifest = join(folder, 'BUILD.json');
	if (!lstatSync(manifest).isFile()) {
		throw new Error('BUILD.json must be a regular file');
	}
	const build = JSON.parse(readFileSync(manifest, 'utf8'));
	if (build === null || typeof build !== 'object' || Array.isArray(build)) {
		throw new Error('BUILD.json must hold an object');
	}
	if (build.build_type !== 'Release') {
		throw new Error('build_type must be Release');
	}
	// The commit before dirty: a bundle from a source tree with no .git of its
	// own, such as a starter project's engine, records both as unknown, and
	// the commit is the rule that says why.
	if (typeof build.commit !== 'string' || !/^[0-9a-f]{40}$/.test(build.commit)) {
		throw new Error('commit must be a full Git revision, not unknown');
	}
	if (build.dirty !== false) {
		throw new Error('dirty must be false');
	}
	if (typeof build.emscripten !== 'string' ||
		!/^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$/.test(build.emscripten)) {
		throw new Error('emscripten must name the toolchain version');
	}
	if (build.files === null || typeof build.files !== 'object' || Array.isArray(build.files)) {
		throw new Error('files must be an object');
	}
	for (const name of required) {
		if (!Object.hasOwn(build.files, name)) {
			throw new Error(`files is missing ${name}`);
		}
	}
	for (const [name, record] of Object.entries(build.files)) {
		if (name === '.' || name === '..' || name === 'BUILD.json' ||
			/[\\/:]/.test(name) || name.length === 0 || basename(name) !== name) {
			throw new Error(`invalid bundle file name: ${name}`);
		}
		if (record === null || typeof record !== 'object' || Array.isArray(record) ||
			!Number.isSafeInteger(record.bytes) || record.bytes < 0 ||
			!Number.isSafeInteger(record.brotli) || record.brotli < 0 ||
			typeof record.sha256 !== 'string' || !/^[0-9a-f]{64}$/.test(record.sha256)) {
			throw new Error(`invalid file record: ${name}`);
		}
		const path = join(folder, name);
		if (!lstatSync(path).isFile()) {
			throw new Error(`${name} must be a regular file`);
		}
		const bytes = readFileSync(path);
		if (createHash('sha256').update(bytes).digest('hex') !== record.sha256) {
			throw new Error(`${name}: SHA-256 does not match BUILD.json`);
		}
		if (bytes.length !== record.bytes) {
			throw new Error(`${name}: byte count does not match BUILD.json`);
		}
	}
	for (const name of readdirSync(folder)) {
		if (name !== 'BUILD.json' && !Object.hasOwn(build.files, name)) {
			throw new Error(`unrecorded bundle entry: ${name}`);
		}
	}
	process.stdout.write(`Verified clean Release LineSweeper bundle from ${build.commit}\n`);
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
	const repository = realpathSync(options.repository);
	if (existsSync(join(repository, '.git'))) {
		const gitRoot = realpathSync(git('rev-parse', '--show-toplevel'));
		const sameRoot = process.platform === 'win32'
			? repository.toLowerCase() === gitRoot.toLowerCase()
			: repository === gitRoot;
		if (sameRoot) {
			const sourceCommit = git('rev-parse', '--verify', 'HEAD');
			const sourceDirty = git('status', '--porcelain', '--untracked-files=no').length > 0;
			commit = sourceCommit;
			dirty = sourceDirty;
		}
	}
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
