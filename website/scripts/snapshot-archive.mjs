// A release snapshot travels as one gzip-wrapped ustar archive attached to its
// GitHub release. The writer emits a deterministic subset of ustar and the
// reader accepts nothing outside that subset, so an archive cannot place links,
// directories or unsafe names into the deployment it is mounted in.
import { createHash } from 'node:crypto';
import { closeSync, lstatSync, mkdirSync, openSync, readdirSync, readFileSync, realpathSync, rmSync, statSync, writeSync } from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { releaseVersion } from './snapshot-source.mjs';

export const ENTRY_MAX_BYTES = 25 * 1024 * 1024;
export const ENTRY_MAX_COUNT = 20_000;
export const GUNZIP_MAX_BYTES = 512 * 1024 * 1024;
export const REQUIRED_FILES = [
	'index.html', '404.html', 'docs/index.html',
	'pagefind/pagefind.js', 'pagefind/pagefind-entry.json', 'snapshot-build.json',
];

const BLOCK = 512;
const RECORD = 10_240;
const NAME_BYTES = 100;
const PREFIX_BYTES = 155;
// Cloudflare reads these as deployment configuration rather than as content.
const CONFIGURATION_NAMES = new Set(['_headers', '_redirects', '.assetsignore']);
const RESERVED_NAME = /^(?:con|prn|aux|nul|com[0-9¹²³]|lpt[0-9¹²³]|conin\$|conout\$)(?:\..*)?$/i;
const TYPE_NAMES = {
	'1': 'a hard link', '2': 'a symbolic link', '3': 'a character device', '4': 'a block device',
	'5': 'a directory', '6': 'a FIFO', '7': 'a contiguous file', 'x': 'a PAX extended header',
	'g': 'a PAX global header', 'L': 'a GNU long name', 'K': 'a GNU long link name',
};
const UTF8 = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true });

function byteCompare(a, b) {
	return Buffer.compare(Buffer.from(a, 'utf8'), Buffer.from(b, 'utf8'));
}

function checkPath(name) {
	const refuse = (reason) => { throw new Error(`Archive path ${JSON.stringify(name)}: expected a safe relative file path, found ${reason}.`); };
	if (name === '') refuse('an empty path');
	if (name.startsWith('/')) refuse('an absolute path');
	if (name.includes('\\')) refuse('a backslash');
	if (name.includes(':')) refuse('a colon, which Windows reads as a drive or stream');
	if (/[<>"|?*]/.test(name)) refuse('a character Windows forbids in names (< > " | ? *)');
	if (name.includes('#')) refuse('a "#", which ends the path of a URL');
	if (/[\u0000-\u001f\u007f-\u009f]/.test(name)) refuse('a NUL or control character');
	for (const segment of name.split('/')) {
		if (segment === '' || segment === '.' || segment === '..') refuse(`the segment ${JSON.stringify(segment)}`);
		if (/[. ]$/.test(segment)) refuse(`the segment ${JSON.stringify(segment)} ending in a dot or space`);
		if (RESERVED_NAME.test(segment)) refuse(`the Windows reserved name ${JSON.stringify(segment)}`);
		if (CONFIGURATION_NAMES.has(segment.toLowerCase())) refuse(`the hosting configuration name ${JSON.stringify(segment)}`);
	}
	// The development site mounts every snapshot below its own /versions/.
	if (/^versions\//i.test(name)) refuse('a versions/ folder, which would nest another version');
}

// The caller supplies paths already in ascending UTF-8 byte order.
function checkPathSet(names) {
	const folded = new Map();
	const folders = new Map();
	// A folder is made once by the spelling of its first file. A second spelling
	// of the same folded folder, by case or by Unicode normalization, is one
	// folder on some file systems and two on others.
	const spellings = new Map();
	for (const name of names) {
		checkPath(name);
		const normal = name.normalize('NFC');
		const key = normal.toLowerCase();
		if (folded.has(key)) {
			throw new Error(`Archive path ${JSON.stringify(name)}: expected a name unique after NFC normalization and case folding, found it collides with ${JSON.stringify(folded.get(key))}.`);
		}
		folded.set(key, name);
		const segments = key.split('/');
		const spelled = name.split('/');
		for (let index = 1; index < segments.length; index += 1) {
			const folder = segments.slice(0, index).join('/');
			const spelling = spelled.slice(0, index).join('/');
			if (!folders.has(folder)) {
				folders.set(folder, name);
				spellings.set(folder, spelling);
			} else if (spellings.get(folder) !== spelling) {
				throw new Error(`Archive path ${JSON.stringify(name)}: expected one spelling of each folder, found the folder ${JSON.stringify(spelling)} also spelled ${JSON.stringify(spellings.get(folder))} by ${JSON.stringify(folders.get(folder))}.`);
			}
		}
	}
	for (const [folder, child] of folders) {
		if (folded.has(folder)) {
			throw new Error(`Archive path ${JSON.stringify(folded.get(folder))}: expected a file or a folder, found it is also the folder of ${JSON.stringify(child)}.`);
		}
	}
}

function splitName(name) {
	const bytes = Buffer.from(name, 'utf8');
	if (bytes.length <= NAME_BYTES) return { prefix: Buffer.alloc(0), name: bytes };
	for (let index = 0; index < bytes.length; index += 1) {
		if (bytes[index] !== 0x2f) continue;
		const prefix = bytes.subarray(0, index);
		const rest = bytes.subarray(index + 1);
		if (prefix.length <= PREFIX_BYTES && rest.length <= NAME_BYTES) return { prefix, name: rest };
	}
	throw new Error(`Archive path ${JSON.stringify(name)}: expected at most ${NAME_BYTES} bytes, or a prefix of at most ${PREFIX_BYTES} and a name of at most ${NAME_BYTES} bytes split at "/", found ${bytes.length} bytes that cannot be split; PAX and GNU long names are not written.`);
}

function octal(header, offset, length, value) {
	header.write(`${value.toString(8).padStart(length - 1, '0')}\0`, offset, length, 'ascii');
}

function fileHeader(name, size) {
	const header = Buffer.alloc(BLOCK);
	const split = splitName(name);
	split.name.copy(header, 0);
	octal(header, 100, 8, 0o644);
	octal(header, 108, 8, 0);
	octal(header, 116, 8, 0);
	octal(header, 124, 12, size);
	octal(header, 136, 12, 0);
	header.write('0', 156, 'ascii');
	header.write('ustar\0', 257, 'ascii');
	header.write('00', 263, 'ascii');
	octal(header, 329, 8, 0);
	octal(header, 337, 8, 0);
	split.prefix.copy(header, 345);
	header.fill(0x20, 148, 156);
	const sum = header.reduce((total, byte) => total + byte, 0);
	header.write(`${sum.toString(8).padStart(6, '0')}\0 `, 148, 8, 'ascii');
	return header;
}

function treeFiles(directory) {
	const files = [];
	function walk(folder) {
		for (const entry of readdirSync(folder, { withFileTypes: true })) {
			const filename = path.join(folder, entry.name);
			if (entry.isSymbolicLink()) throw new Error(`Snapshot tree: expected regular files, found a symbolic link: ${filename}`);
			if (entry.isDirectory()) walk(filename);
			else if (entry.isFile()) files.push(path.relative(directory, filename).split(path.sep).join('/'));
			else throw new Error(`Snapshot tree: expected regular files, found a special entry: ${filename}`);
		}
	}
	walk(directory);
	return files.sort(byteCompare);
}

/**
 * Pack a tree's regular files as gzip around ustar. Only the uncompressed tar
 * is reproducible; the manifest pins the bytes actually attached.
 */
export function packTree(directory) {
	const root = realpathSync(directory);
	if (!statSync(root).isDirectory()) throw new Error(`Snapshot tree: expected a directory, found ${directory}.`);
	const files = treeFiles(root);
	if (files.length > ENTRY_MAX_COUNT) {
		throw new Error(`Snapshot tree: expected at most ${ENTRY_MAX_COUNT} files, found ${files.length}.`);
	}
	checkPathSet(files);
	const blocks = [];
	let length = 0;
	for (const name of files) {
		const data = readFileSync(path.join(root, ...name.split('/')));
		if (data.length > ENTRY_MAX_BYTES) {
			throw new Error(`Archive path ${JSON.stringify(name)}: expected at most ${ENTRY_MAX_BYTES} bytes, found ${data.length}.`);
		}
		const padding = (BLOCK - (data.length % BLOCK)) % BLOCK;
		blocks.push(fileHeader(name, data.length), data, Buffer.alloc(padding));
		length += BLOCK + data.length + padding;
	}
	length += 2 * BLOCK;
	const total = Math.ceil(length / RECORD) * RECORD;
	blocks.push(Buffer.alloc(total - length + 2 * BLOCK));
	return zlib.gzipSync(Buffer.concat(blocks, total), { level: 9 });
}

function gunzipOne(buffer, maxOutputLength) {
	if (buffer.length < 18 || buffer[0] !== 0x1f || buffer[1] !== 0x8b || buffer[2] !== 8) {
		throw new Error('Archive: expected a gzip stream using deflate, found another format.');
	}
	if (buffer[3] !== 0) {
		throw new Error(`Archive: expected a gzip header without a name, comment or extra field, found flags 0x${buffer[3].toString(16)}.`);
	}
	let result;
	try {
		result = zlib.inflateRawSync(buffer.subarray(10), { maxOutputLength, info: true });
	} catch (error) {
		if (error.code === 'ERR_BUFFER_TOO_LARGE') {
			throw new Error(`Archive: expected at most ${maxOutputLength} uncompressed bytes, found more.`);
		}
		throw new Error(`Archive: expected valid deflate data, found ${error.message}.`);
	}
	const end = 10 + result.engine.bytesWritten;
	if (end + 8 > buffer.length) throw new Error('Archive: expected a gzip trailer, found the stream truncated.');
	if (end + 8 < buffer.length) {
		const rest = buffer.subarray(end + 8);
		throw new Error(rest[0] === 0x1f && rest[1] === 0x8b
			? 'Archive: expected one gzip member, found another member after it.'
			: `Archive: expected the gzip stream to end the file, found ${rest.length} trailing bytes.`);
	}
	const tar = result.buffer;
	if (buffer.readUInt32LE(end) !== zlib.crc32(tar) || buffer.readUInt32LE(end + 4) !== tar.length % 2 ** 32) {
		throw new Error('Archive: expected the gzip trailer to match the uncompressed data, found a CRC or length mismatch.');
	}
	return tar;
}

function field(header, offset, length, label, location) {
	const bytes = header.subarray(offset, offset + length);
	const end = bytes.indexOf(0);
	const used = end < 0 ? bytes : bytes.subarray(0, end);
	if (end >= 0 && bytes.subarray(end).some((byte) => byte !== 0)) {
		throw new Error(`${location}: expected the ${label} field padded with NUL, found bytes after its end.`);
	}
	try { return UTF8.decode(used); }
	catch { throw new Error(`${location}: expected a UTF-8 ${label}, found invalid UTF-8.`); }
}

function octalField(header, offset, length, label, location) {
	const text = header.toString('latin1', offset, offset + length);
	const match = /^([0-7]+)[ \0]*$/.exec(text);
	if (!match) throw new Error(`${location}: expected an octal ${label} field, found ${JSON.stringify(text)}.`);
	return Number.parseInt(match[1], 8);
}

/**
 * Verify an attached archive against its manifest entry, then parse it. The
 * length and hash are checked before any decompression runs.
 */
export function readArchive(buffer, entry, { maxOutputLength = GUNZIP_MAX_BYTES } = {}) {
	const label = entry?.asset ?? 'archive';
	if (!Buffer.isBuffer(buffer)) throw new Error(`${label}: expected archive bytes, found ${typeof buffer}.`);
	if (buffer.length !== entry.bytes) throw new Error(`${label}: expected ${entry.bytes} bytes, found ${buffer.length}.`);
	const digest = createHash('sha256').update(buffer).digest('hex');
	if (digest !== entry.sha256) throw new Error(`${label}: expected SHA-256 ${entry.sha256}, found ${digest}.`);
	const tar = gunzipOne(buffer, maxOutputLength);
	if (tar.length % BLOCK) throw new Error(`Archive: expected whole ${BLOCK}-byte blocks, found ${tar.length} bytes.`);
	const files = new Map();
	let previous = null;
	let offset = 0;
	for (;;) {
		if (offset + BLOCK > tar.length) throw new Error('Archive: expected two zero end blocks, found the archive ends without them.');
		const header = tar.subarray(offset, offset + BLOCK);
		if (header.every((byte) => byte === 0)) {
			if (offset + 2 * BLOCK > tar.length || tar.subarray(offset + BLOCK, offset + 2 * BLOCK).some((byte) => byte !== 0)) {
				throw new Error('Archive: expected two zero end blocks, found only one.');
			}
			if (tar.subarray(offset + 2 * BLOCK).some((byte) => byte !== 0)) {
				throw new Error('Archive: expected only zeros after the end blocks, found data.');
			}
			break;
		}
		const location = `Archive entry at byte ${offset}`;
		const stored = octalField(header, 148, 8, 'checksum', location);
		const sum = header.reduce((total, byte, index) => total + (index >= 148 && index < 156 ? 0x20 : byte), 0);
		if (stored !== sum) throw new Error(`${location}: expected header checksum ${sum.toString(8)}, found ${stored.toString(8)}.`);
		if (header.toString('latin1', 257, 263) !== 'ustar\0' || header.toString('latin1', 263, 265) !== '00') {
			throw new Error(`${location}: expected ustar magic and version 00, found ${JSON.stringify(header.toString('latin1', 257, 265))}.`);
		}
		const name = field(header, 0, NAME_BYTES, 'name', location);
		const prefix = field(header, 345, PREFIX_BYTES, 'prefix', location);
		const filename = prefix ? `${prefix}/${name}` : name;
		const where = `Archive entry ${JSON.stringify(filename)}`;
		const type = String.fromCharCode(header[156]);
		if (type !== '0' && type !== '\0') {
			throw new Error(`${where}: expected a regular file, found ${TYPE_NAMES[type] ?? `typeflag ${JSON.stringify(type)}`}.`);
		}
		if (field(header, 157, 100, 'link name', where) !== '') throw new Error(`${where}: expected no link name, found one.`);
		for (const [start, length, text] of [[100, 8, 'mode'], [108, 8, 'uid'], [116, 8, 'gid'], [136, 12, 'mtime'], [329, 8, 'device major'], [337, 8, 'device minor']]) {
			octalField(header, start, length, text, where);
		}
		const size = octalField(header, 124, 12, 'size', where);
		if (size > ENTRY_MAX_BYTES) throw new Error(`${where}: expected at most ${ENTRY_MAX_BYTES} bytes, found ${size}.`);
		if (files.size >= ENTRY_MAX_COUNT) throw new Error(`Archive: expected at most ${ENTRY_MAX_COUNT} entries, found more.`);
		checkPath(filename);
		if (previous !== null) {
			const order = byteCompare(previous, filename);
			if (order === 0) throw new Error(`${where}: expected each path once, found a duplicate.`);
			if (order > 0) throw new Error(`${where}: expected paths in ascending byte order, found it after ${JSON.stringify(previous)}.`);
		}
		const start = offset + BLOCK;
		const padded = Math.ceil(size / BLOCK) * BLOCK;
		if (start + padded > tar.length) throw new Error(`${where}: expected ${size} bytes of data, found the archive truncated.`);
		files.set(filename, tar.subarray(start, start + size));
		previous = filename;
		offset = start + padded;
	}
	checkPathSet(files.keys());
	for (const required of REQUIRED_FILES) {
		if (!files.has(required)) throw new Error(`Archive: expected the snapshot file ${required}, found it missing.`);
	}
	return files;
}

/**
 * Write parsed files into a new <parent>/.<tag>.partial/ folder and return it.
 * The caller renames it into place only after verifying the result.
 */
export function extractArchive(files, destinationParent, tag) {
	releaseVersion(tag);
	const names = [...files.keys()].sort(byteCompare);
	checkPathSet(names);
	const parent = realpathSync(destinationParent);
	const partial = path.join(parent, `.${tag}.partial`);
	try {
		mkdirSync(partial);
	} catch (error) {
		throw new Error(`${partial}: expected no existing partial extraction, found ${error.code ?? error.message}.`);
	}
	try {
		const root = realpathSync(partial);
		const folders = new Set();
		for (const name of names) {
			const segments = name.split('/');
			for (let index = 1; index < segments.length; index += 1) {
				const folder = segments.slice(0, index).join('/');
				if (folders.has(folder)) continue;
				mkdirSync(path.join(root, ...segments.slice(0, index)));
				folders.add(folder);
			}
			const filename = path.join(root, ...segments);
			const descriptor = openSync(filename, 'wx');
			try {
				const data = files.get(name);
				let written = 0;
				while (written < data.length) written += writeSync(descriptor, data, written, data.length - written);
			} finally {
				closeSync(descriptor);
			}
			const resolved = realpathSync(filename);
			if (!resolved.startsWith(root + path.sep) || !lstatSync(resolved).isFile()) {
				throw new Error(`Archive path ${JSON.stringify(name)}: expected a file inside ${root}, found ${resolved}.`);
			}
		}
	} catch (error) {
		rmSync(partial, { recursive: true, force: true });
		throw error;
	}
	return partial;
}
