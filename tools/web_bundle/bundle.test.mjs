import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdtempSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { brotliCompressSync, constants } from 'node:zlib';
import { test } from 'node:test';

const runner = fileURLToPath(new URL('./bundle.mjs', import.meta.url));
const names = ['index.html', 'linesweeper.js', 'linesweeper.wasm', 'poster.png', 'NOTICE.txt'];

function fixture(t) {
	const folder = mkdtempSync(join(tmpdir(), 'labrador-bundle-'));
	t.after(() => rmSync(folder, { recursive: true, force: true }));
	const build = {
		commit: 'a'.repeat(40), dirty: false, build_type: 'Release', emscripten: '6.0.12', files: {},
	};
	for (const name of names) {
		const bytes = Buffer.from(`fixture for ${name}\n`);
		writeFileSync(join(folder, name), bytes);
		build.files[name] = {
			bytes: bytes.length,
			brotli: brotliCompressSync(bytes, {
				params: {
					[constants.BROTLI_PARAM_QUALITY]: constants.BROTLI_MAX_QUALITY,
					[constants.BROTLI_PARAM_SIZE_HINT]: bytes.length,
				},
			}).length,
			sha256: createHash('sha256').update(bytes).digest('hex'),
		};
	}
	return { folder, build };
}

function verify(folder, ...extra) {
	const result = spawnSync(process.execPath, [runner, '--verify', folder, ...extra], {
		encoding: 'utf8', timeout: 20_000,
	});
	assert.ifError(result.error);
	return result;
}

function write_manifest(folder, build) {
	writeFileSync(join(folder, 'BUILD.json'), JSON.stringify(build));
}

test('clean Release bundle verifies without changing any files', (t) => {
	const { folder, build } = fixture(t);
	write_manifest(folder, build);
	const before = readdirSync(folder).map((name) => [name, readFileSync(join(folder, name))]);
	const result = verify(folder);
	assert.equal(result.status, 0, result.stderr);
	assert.match(result.stdout, new RegExp(`Verified clean Release.*${build.commit}`));
	assert.deepEqual(readdirSync(folder).map((name) => [name, readFileSync(join(folder, name))]), before);
});

for (const [label, change, message] of [
	['Debug build', (b) => { b.build_type = 'Debug'; }, /build_type must be Release/],
	['missing build type', (b) => { delete b.build_type; }, /build_type must be Release/],
	['dirty checkout', (b) => { b.dirty = true; }, /dirty must be false/],
	['unknown dirty state', (b) => { b.dirty = null; }, /dirty must be false/],
	['string dirty state', (b) => { b.dirty = 'false'; }, /dirty must be false/],
	['unknown commit', (b) => { b.commit = 'unknown'; }, /commit must be a full Git revision/],
	['abbreviated commit', (b) => { b.commit = 'abcdef0'; }, /commit must be a full Git revision/],
	['missing toolchain', (b) => { delete b.emscripten; }, /emscripten must name/],
	['unknown toolchain', (b) => { b.emscripten = 'unknown'; }, /emscripten must name/],
	['missing file inventory', (b) => { delete b.files; }, /files must be an object/],
	['array file inventory', (b) => { b.files = []; }, /files must be an object/],
	['wrong byte count', (b) => { b.files['index.html'].bytes += 1; }, /index.html: byte count does not match/],
	['invalid Brotli count', (b) => { b.files['index.html'].brotli = -1; }, /invalid file record: index.html/],
	['invalid hash', (b) => { b.files['index.html'].sha256 = 'oops'; }, /invalid file record: index.html/],
	['null file record', (b) => { b.files['index.html'] = null; }, /invalid file record: index.html/],
	['traversing path', (b) => { b.files['../outside'] = b.files['index.html']; }, /invalid bundle file name/],
	['Windows traversing path', (b) => { b.files['..\\outside'] = b.files['index.html']; }, /invalid bundle file name/],
	['self inventory', (b) => { b.files['BUILD.json'] = b.files['index.html']; }, /invalid bundle file name/],
]) {
	test(`rejects ${label}`, (t) => {
		const { folder, build } = fixture(t);
		change(build);
		write_manifest(folder, build);
		const result = verify(folder);
		assert.equal(result.status, 2, result.stdout);
		assert.match(result.stderr, message);
	});
}

for (const name of names) {
	test(`rejects missing ${name}, including an omitted manifest entry`, (t) => {
		const { folder, build } = fixture(t);
		rmSync(join(folder, name));
		write_manifest(folder, build);
		assert.equal(verify(folder).status, 2);
		delete build.files[name];
		write_manifest(folder, build);
		const result = verify(folder);
		assert.equal(result.status, 2);
		assert.ok(result.stderr.includes(`files is missing ${name}`), result.stderr);
	});
}

test('rejects changed bytes even when the length stays the same', (t) => {
	const { folder, build } = fixture(t);
	write_manifest(folder, build);
	writeFileSync(join(folder, 'linesweeper.wasm'), Buffer.alloc(build.files['linesweeper.wasm'].bytes));
	const result = verify(folder);
	assert.equal(result.status, 2);
	assert.match(result.stderr, /linesweeper.wasm: SHA-256 does not match/);
});

test('Brotli size is a packaging estimate, independent of the verifier runtime', (t) => {
	const { folder, build } = fixture(t);
	build.files['index.html'].brotli += 1;
	write_manifest(folder, build);
	assert.equal(verify(folder).status, 0);
});

test('additional recorded files verify with the same integrity checks', (t) => {
	const { folder, build } = fixture(t);
	writeFileSync(join(folder, 'extra.html'), readFileSync(join(folder, 'index.html')));
	build.files['extra.html'] = build.files['index.html'];
	write_manifest(folder, build);
	assert.equal(verify(folder).status, 0);
	writeFileSync(join(folder, 'extra.html'), 'changed');
	assert.match(verify(folder).stderr, /extra.html: SHA-256 does not match/);
});

test('rejects unrecorded files and folders', (t) => {
	const { folder, build } = fixture(t);
	write_manifest(folder, build);
	writeFileSync(join(folder, 'old.js'), 'stale module');
	assert.match(verify(folder).stderr, /unrecorded bundle entry: old.js/);
	rmSync(join(folder, 'old.js'));
	mkdirSync(join(folder, 'old'));
	assert.match(verify(folder).stderr, /unrecorded bundle entry: old/);
});

test('rejects a directory in place of a recorded file', (t) => {
	const { folder, build } = fixture(t);
	write_manifest(folder, build);
	rmSync(join(folder, 'linesweeper.js'));
	mkdirSync(join(folder, 'linesweeper.js'));
	assert.match(verify(folder).stderr, /linesweeper.js must be a regular file/);
});

test('rejects missing, malformed and non-object BUILD.json', (t) => {
	const { folder } = fixture(t);
	assert.equal(verify(folder).status, 2);
	for (const text of ['{', 'null', '[]', '42']) {
		writeFileSync(join(folder, 'BUILD.json'), text);
		assert.equal(verify(folder).status, 2, text);
	}
});

test('verify mode refuses packaging options and never empties its input', (t) => {
	const { folder, build } = fixture(t);
	write_manifest(folder, build);
	const result = verify(folder, '--out', folder);
	assert.equal(result.status, 2);
	assert.match(result.stderr, /--verify takes only/);
	assert.equal(readFileSync(join(folder, 'index.html'), 'utf8'), 'fixture for index.html\n');
});
