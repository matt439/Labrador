import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { mock, test } from 'node:test';
import zlib from 'node:zlib';
import {
	ENTRY_MAX_BYTES, ENTRY_MAX_COUNT, GUNZIP_MAX_BYTES, REQUIRED_FILES,
	extractArchive, packTree, readArchive,
} from '../scripts/snapshot-archive.mjs';

function temporaryDirectory(t) {
	const root = mkdtempSync(path.join(tmpdir(), 'labrador-snapshot-archive-'));
	t.after(() => {
		const target = realpathSync(root);
		assert.equal(path.dirname(target), realpathSync(tmpdir()));
		assert.ok(path.basename(target).startsWith('labrador-snapshot-archive-'));
		rmSync(target, { recursive: true, force: true });
	});
	return root;
}

function write(root, file, contents) {
	const target = path.join(root, file);
	mkdirSync(path.dirname(target), { recursive: true });
	writeFileSync(target, contents);
}

const TREE = {
	'index.html': '<p>Home</p>',
	'404.html': '<p>Missing</p>',
	'docs/index.html': '<p>Docs</p>',
	'docs/guides/example/index.html': '<p>Guide</p>',
	[`docs/${'b'.repeat(100)}`]: 'A name that needs the ustar prefix field',
	'pagefind/pagefind.js': 'export {};',
	'pagefind/pagefind-entry.json': '{}',
	'snapshot-build.json': '{}\n',
	'_astro/empty.css': '',
	'media/block.bin': Buffer.alloc(512, 7),
	'Zeta.txt': 'Upper case sorts before lower case in byte order',
};

function tree(t, files = TREE, order = Object.keys(files)) {
	const root = temporaryDirectory(t);
	for (const file of order) write(root, file, files[file]);
	return root;
}

function entryFor(buffer) {
	return { asset: 'labrador-docs-v0.3.0.tar.gz', bytes: buffer.length, sha256: createHash('sha256').update(buffer).digest('hex') };
}

function octal(value, length) {
	return `${value.toString(8).padStart(length - 1, '0')}\0`;
}

// Hand-built headers let each refusal be tested without the writer's own checks.
function header({ name, prefix = '', type = '0', size = 0, magic = 'ustar\0', version = '00', linkname = '', checksum, mtime = octal(0, 12) }) {
	const block = Buffer.alloc(512);
	Buffer.from(name).copy(block, 0);
	block.write(octal(0o644, 8), 100, 'latin1');
	block.write(octal(0, 8), 108, 'latin1');
	block.write(octal(0, 8), 116, 'latin1');
	block.write(octal(size, 12), 124, 'latin1');
	block.write(mtime, 136, 'latin1');
	block.write(type, 156, 'latin1');
	Buffer.from(linkname).copy(block, 157);
	block.write(magic, 257, 'latin1');
	block.write(version, 263, 'latin1');
	block.write(octal(0, 8), 329, 'latin1');
	block.write(octal(0, 8), 337, 'latin1');
	Buffer.from(prefix).copy(block, 345);
	block.fill(0x20, 148, 156);
	const sum = checksum ?? block.reduce((total, byte) => total + byte, 0);
	block.write(`${sum.toString(8).padStart(6, '0')}\0 `, 148, 'latin1');
	return block;
}

function tar(entries, { endBlocks = 2, after = Buffer.alloc(0) } = {}) {
	const blocks = [];
	for (const entry of entries) {
		const data = Buffer.from(entry.data ?? '');
		blocks.push(header({ size: data.length, ...entry }), data, Buffer.alloc((512 - (data.length % 512)) % 512));
	}
	blocks.push(Buffer.alloc(512 * endBlocks), after);
	return Buffer.concat(blocks);
}

function gzip(buffer) {
	return zlib.gzipSync(buffer, { level: 9 });
}

function read(buffer, options) {
	return readArchive(buffer, entryFor(buffer), options);
}

function required() {
	return REQUIRED_FILES.map((name) => ({ name, data: 'x' })).sort((a, b) => Buffer.compare(Buffer.from(a.name), Buffer.from(b.name)));
}

test('the limits are the ones the deployment contract states', () => {
	assert.equal(ENTRY_MAX_BYTES, 25 * 1024 * 1024);
	assert.equal(ENTRY_MAX_COUNT, 20_000);
	assert.equal(GUNZIP_MAX_BYTES, 512 * 1024 * 1024);
});

test('packing the same tree twice, in any creation order, gives an identical tar', (t) => {
	const first = packTree(tree(t));
	const again = packTree(tree(t));
	const reversed = packTree(tree(t, TREE, Object.keys(TREE).reverse()));
	assert.deepEqual(zlib.gunzipSync(again), zlib.gunzipSync(first));
	assert.deepEqual(zlib.gunzipSync(reversed), zlib.gunzipSync(first));
	assert.equal(zlib.gunzipSync(first).length % 10_240, 0);
});

test('headers carry fixed ownership, mode and time, and files are in byte order', (t) => {
	const archive = packTree(tree(t));
	assert.deepEqual([...archive.subarray(0, 4)], [0x1f, 0x8b, 8, 0], 'gzip without a stored name');
	assert.equal(archive.readUInt32LE(4), 0, 'gzip mtime');
	const raw = zlib.gunzipSync(archive);
	const names = [];
	for (let offset = 0; raw.subarray(offset, offset + 512).some((byte) => byte !== 0);) {
		const block = raw.subarray(offset, offset + 512);
		assert.equal(block.toString('latin1', 100, 108), '0000644\0');
		assert.equal(block.toString('latin1', 108, 116), '0000000\0');
		assert.equal(block.toString('latin1', 116, 124), '0000000\0');
		assert.equal(block.toString('latin1', 136, 148), '00000000000\0');
		assert.equal(block.toString('latin1', 156, 157), '0');
		assert.equal(block.toString('latin1', 257, 265), 'ustar\u000000');
		assert.ok(block.subarray(265, 329).every((byte) => byte === 0), 'empty uname and gname');
		const name = block.toString('utf8', 0, 100).replace(/\0+$/, '');
		const prefix = block.toString('utf8', 345, 500).replace(/\0+$/, '');
		names.push(prefix ? `${prefix}/${name}` : name);
		const size = Number.parseInt(block.toString('latin1', 124, 135), 8);
		offset += 512 + Math.ceil(size / 512) * 512;
	}
	assert.deepEqual(names, [...Object.keys(TREE)].sort((a, b) => Buffer.compare(Buffer.from(a), Buffer.from(b))));
	assert.ok(names.includes(`docs/${'b'.repeat(100)}`));
});

test('a packed tree reads back byte for byte', (t) => {
	const files = read(packTree(tree(t)));
	assert.deepEqual([...files.keys()].sort(), Object.keys(TREE).sort());
	for (const [name, contents] of Object.entries(TREE)) assert.deepEqual(files.get(name), Buffer.from(contents), name);
});

test('the writer refuses a path that cannot be split into ustar fields', (t) => {
	const root = tree(t);
	write(root, 'c'.repeat(101), 'too long');
	assert.throws(() => packTree(root), /cannot be split; PAX and GNU long names are not written/);
});

test('the writer refuses names the reader would refuse', (t) => {
	const root = tree(t);
	write(root, 'versions/v0.2.0/index.html', 'nested');
	assert.throws(() => packTree(root), /versions\/ folder/);
});

test('link, directory and extension entries are refused', () => {
	for (const [type, expected] of [
		['2', /found a symbolic link/], ['1', /found a hard link/], ['5', /found a directory/],
		['x', /found a PAX extended header/], ['g', /found a PAX global header/], ['L', /found a GNU long name/],
		['3', /found a character device/], ['6', /found a FIFO/],
	]) {
		assert.throws(() => read(gzip(tar([{ name: 'index.html', type }]))), expected, type);
	}
	assert.throws(() => read(gzip(tar([{ name: 'index.html', linkname: 'other' }]))), /expected no link name/);
});

test('unsafe paths are refused', () => {
	for (const [name, expected] of [
		['/etc/passwd', /absolute path/],
		['../outside.html', /segment "\.\."/],
		['docs/../../outside.html', /segment "\.\."/],
		['./index.html', /segment "\."/],
		['docs//index.html', /segment ""/],
		['docs\\index.html', /backslash/],
		['C:/Windows/index.html', /colon/],
		['index.html:stream', /colon/],
		...['<', '>', '"', '|', '?', '*'].map((character) => [`a${character}b.html`, /a character Windows forbids in names/]),
		['docs/a#b.html', /"#", which ends the path of a URL/],
		['docs/index.html\u0001', /control character/],
		['docs./index.html', /ending in a dot or space/],
		['docs /index.html', /ending in a dot or space/],
		['docs/CON', /Windows reserved name/],
		['aux.html', /Windows reserved name/],
		['lpt1.txt', /Windows reserved name/],
		['versions/v0.2.0/index.html', /versions\/ folder/],
		['Versions/index.html', /versions\/ folder/],
		['_headers', /hosting configuration name/],
		['docs/_redirects', /hosting configuration name/],
		['.assetsignore', /hosting configuration name/],
	]) {
		assert.throws(() => read(gzip(tar([{ name }]))), expected, name);
	}
	assert.throws(() => read(gzip(tar([{ name: 'docs', prefix: '', data: '' }, { name: '', prefix: 'docs' }]))), /segment ""/);
});

test('names must be strict UTF-8 without hidden bytes after their end', () => {
	assert.throws(() => read(gzip(tar([{ name: Buffer.from([0x64, 0xff, 0x2e]) }]))), /invalid UTF-8/);
	assert.throws(() => read(gzip(tar([{ name: Buffer.from('index.html\0hidden') }]))), /padded with NUL/);
});

test('entries must be unique, in byte order, and not collide on case-insensitive disks', () => {
	assert.throws(() => read(gzip(tar([{ name: 'b.html' }, { name: 'a.html' }]))), /ascending byte order/);
	assert.throws(() => read(gzip(tar([{ name: 'a.html' }, { name: 'a.html' }]))), /duplicate/);
	assert.throws(() => read(gzip(tar([{ name: 'Docs.html' }, { name: 'docs.html' }]))), /case folding/);
	assert.throws(() => read(gzip(tar([{ name: 'cafe\u0301.html' }, { name: 'caf\u00e9.html' }]))), /NFC normalization/);
	assert.throws(() => read(gzip(tar([{ name: 'docs' }, { name: 'docs/index.html' }]))), /also the folder of "docs\/index.html"/);
	assert.throws(() => read(gzip(tar([{ name: 'Docs' }, { name: 'docs/index.html' }]))), /also the folder/);
	assert.throws(() => read(gzip(tar([{ name: 'A/x.html' }, { name: 'a/y.html' }]))),
		/"a\/y\.html": expected one spelling of each folder, found the folder "a" also spelled "A" by "A\/x\.html"/);
	assert.throws(() => read(gzip(tar([{ name: 'docs/Guides/x.html' }, { name: 'docs/guides/y.html' }]))), /folder "docs\/guides" also spelled "docs\/Guides"/);
	// NTFS and ext4 keep NFC-equal folder names apart; APFS does not.
	assert.throws(() => read(gzip(tar([{ name: 'cafe\u0301/x.html' }, { name: 'caf\u00e9/y.html' }]))), /expected one spelling of each folder/);
});

test('header integrity and the end of the archive are checked', () => {
	assert.throws(() => read(gzip(tar([{ name: 'index.html', checksum: 1 }]))), /expected header checksum/);
	assert.throws(() => read(gzip(tar([{ name: 'index.html', magic: 'ustar ', version: ' \0' }]))), /ustar magic/);
	assert.throws(() => read(gzip(tar([{ name: 'index.html', mtime: '0000000009\0\0' }]))), /octal mtime/);
	assert.throws(() => read(gzip(tar([{ name: 'index.html' }], { endBlocks: 1 }))), /found only one/);
	assert.throws(() => read(gzip(tar([{ name: 'index.html' }], { endBlocks: 0 }))), /ends without them/);
	assert.throws(() => read(gzip(tar([{ name: 'index.html' }], { after: Buffer.from('trailing garbage'.padEnd(512, '!')) }))), /only zeros after the end blocks/);
	assert.throws(() => read(gzip(tar([{ name: 'index.html', data: 'x'.repeat(600) }]).subarray(0, 1024))), /truncated|end blocks/);
});

test('an oversize entry is refused from its header, before its data is read', () => {
	assert.throws(() => read(gzip(tar([{ name: 'index.html', size: ENTRY_MAX_BYTES + 1 }]))), new RegExp(`at most ${ENTRY_MAX_BYTES} bytes, found ${ENTRY_MAX_BYTES + 1}`));
});

test('more than the entry limit is refused', () => {
	const entries = Array.from({ length: ENTRY_MAX_COUNT + 1 }, (_, index) => ({ name: `f${String(index).padStart(5, '0')}` }));
	assert.throws(() => read(gzip(tar(entries))), new RegExp(`at most ${ENTRY_MAX_COUNT} entries`));
});

test('length and hash are refused before decompression runs', (t) => {
	const archive = packTree(tree(t));
	const inflate = mock.method(zlib, 'inflateRawSync');
	t.after(() => inflate.mock.restore());
	assert.throws(() => readArchive(archive, { ...entryFor(archive), bytes: archive.length + 1 }), /expected \d+ bytes, found/);
	assert.throws(() => readArchive(archive, { ...entryFor(archive), sha256: '0'.repeat(64) }), /expected SHA-256 0{64}, found/);
	assert.equal(inflate.mock.callCount(), 0);
	read(archive);
	assert.equal(inflate.mock.callCount(), 1, 'the spy observes the reader');
});

test('decompression is capped, and the gzip stream must be one member ending the file', (t) => {
	const archive = packTree(tree(t));
	assert.throws(() => read(archive, { maxOutputLength: 4096 }), /at most 4096 uncompressed bytes, found more/);
	assert.throws(() => read(Buffer.concat([archive, archive])), /one gzip member/);
	assert.throws(() => read(Buffer.concat([archive, Buffer.from('garbage')])), /7 trailing bytes/);
	const named = zlib.gzipSync(zlib.gunzipSync(archive));
	named[3] = 0x08;
	assert.throws(() => read(named), /without a name/);
	const corrupt = Buffer.from(archive);
	corrupt.writeUInt32LE(corrupt.readUInt32LE(corrupt.length - 8) ^ 1, corrupt.length - 8);
	assert.throws(() => read(corrupt), /CRC or length mismatch/);
	assert.throws(() => read(Buffer.from('not a gzip stream at all')), /gzip stream/);
});

test('a well-formed archive missing a required snapshot file is refused', (t) => {
	for (const missing of REQUIRED_FILES) {
		const root = tree(t);
		rmSync(path.join(root, missing));
		assert.throws(() => read(packTree(root)), new RegExp(`expected the snapshot file ${missing.replace('.', '\\.')}, found it missing`));
	}
	read(gzip(tar(required())));
});

test('extraction writes a new partial folder and reproduces every file', (t) => {
	const parent = temporaryDirectory(t);
	const files = read(packTree(tree(t)));
	const partial = extractArchive(files, parent, 'v0.3.0');
	assert.equal(partial, path.join(realpathSync(parent), '.v0.3.0.partial'));
	for (const [name, contents] of files) assert.deepEqual(readFileSync(path.join(partial, name)), contents, name);
});

test('extraction refuses an existing partial folder and leaves it alone', (t) => {
	const parent = temporaryDirectory(t);
	write(parent, '.v0.3.0.partial/earlier.txt', 'earlier attempt');
	assert.throws(() => extractArchive(read(gzip(tar(required()))), parent, 'v0.3.0'), /expected no existing partial extraction, found EEXIST/);
	assert.equal(readFileSync(path.join(parent, '.v0.3.0.partial/earlier.txt'), 'utf8'), 'earlier attempt');
});

test('extraction refuses paths that escape, and removes its partial folder', (t) => {
	const parent = temporaryDirectory(t);
	const outside = path.join(parent, 'outside.txt');
	for (const name of ['../outside.txt', path.join(parent, 'outside.txt'), 'docs/../../outside.txt']) {
		const files = new Map([['index.html', Buffer.from('x')], [name, Buffer.from('escaped')]]);
		assert.throws(() => extractArchive(files, parent, 'v0.3.0'), /expected a safe relative file path/, name);
		assert.equal(existsSync(outside), false, name);
		assert.equal(existsSync(path.join(parent, '.v0.3.0.partial')), false, name);
	}
	const twoSpellings = new Map([['A/x.html', Buffer.from('x')], ['a/y.html', Buffer.from('y')]]);
	assert.throws(() => extractArchive(twoSpellings, parent, 'v0.3.0'), /expected one spelling of each folder/);
	assert.equal(existsSync(path.join(parent, '.v0.3.0.partial')), false);
	for (const tag of ['../v0.3.0', 'v0.3', 'v0.2.0']) {
		assert.throws(() => extractArchive(new Map(), parent, tag), Error, tag);
	}
});
