// Verify the selected README and built preview image without changing bytes.
import { readFileSync, readdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parse } from 'parse5';

const REPOSITORY = fileURLToPath(new URL('../../', import.meta.url));
export const CAPTURE_PATH = 'website/src/assets/captures/linesweeper-play.png';
const ORIGIN = 'https://labradorengine.com';

function* elements(node) {
	if (node.tagName) yield node;
	for (const child of node.childNodes ?? []) yield* elements(child);
}

function attrs(node) {
	return Object.fromEntries(node.attrs.map(({ name, value }) => [name, value]));
}

function capture(root) {
	const bytes = readFileSync(path.join(root, CAPTURE_PATH));
	if (bytes.length < 24 || bytes.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a') {
		throw new Error(`${CAPTURE_PATH}: expected the original PNG capture.`);
	}
	return { bytes, width: bytes.readUInt32BE(16), height: bytes.readUInt32BE(20) };
}

export function verifyReadmeImage(root = REPOSITORY) {
	const source = capture(root);
	const images = [...elements(parse(readFileSync(path.join(root, 'README.md'), 'utf8')))]
		.filter((node) => node.tagName === 'img').map(attrs)
		.filter((image) => image.src === CAPTURE_PATH);
	if (images.length !== 1) throw new Error(`README: expected one image at ${CAPTURE_PATH}.`);
	const image = images[0];
	if (image.width !== String(source.width) || image.height !== String(source.height)
		|| !image.alt?.startsWith('LineSweeper:')) {
		throw new Error('README: expected capture dimensions and meaningful LineSweeper alternative text.');
	}
	return source;
}

function* pages(directory, relative = '') {
	for (const entry of readdirSync(path.join(directory, relative), { withFileTypes: true })) {
		// Immutable published snapshots keep their original metadata.
		if (relative === 'versions' && entry.isDirectory() && /^v\d+\.\d+\.\d+$/.test(entry.name)) continue;
		const file = path.join(relative, entry.name);
		if (entry.isDirectory()) yield* pages(directory, file);
		else if (entry.isFile() && file.endsWith('.html')) yield file;
	}
}

export function verifyPreviewImageOutput(directory, { root = REPOSITORY, base = '/' } = {}) {
	const source = verifyReadmeImage(root);
	let count = 0;
	for (const file of pages(directory)) {
		const head = [...elements(parse(readFileSync(path.join(directory, file), 'utf8')))]
			.filter((node) => node.tagName === 'meta').map(attrs);
		const value = (key) => {
			const found = head.filter((meta) => (meta.property ?? meta.name) === key);
			if (found.length !== 1) throw new Error(`${file}: expected exactly one ${key}.`);
			return found[0].content;
		};
		const url = new URL(value('og:image'));
		if (url.origin !== ORIGIN || !url.pathname.startsWith(`${base}_astro/`) || url.search || url.hash) {
			throw new Error(`${file}: preview image must stay within ${ORIGIN}${base}_astro/.`);
		}
		const target = path.resolve(directory, decodeURIComponent(url.pathname.slice(base.length)));
		if (!target.startsWith(path.resolve(directory) + path.sep)
			|| !readFileSync(target).equals(source.bytes)) {
			throw new Error(`${file}: preview image must contain the original capture bytes.`);
		}
		if (value('og:image:width') !== String(source.width) || value('og:image:height') !== String(source.height)
			|| value('og:image:type') !== 'image/png' || !value('og:image:alt')?.startsWith('LineSweeper:')
			|| value('twitter:image') !== url.href || value('twitter:image:alt') !== value('og:image:alt')
			|| value('twitter:card') !== 'summary_large_image') {
			throw new Error(`${file}: preview metadata must describe the selected capture and dimensions.`);
		}
		count += 1;
	}
	if (count === 0) throw new Error(`${directory}: expected built HTML pages.`);
	return { pages: count, width: source.width, height: source.height };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		if (process.argv.length !== 2) throw new Error('Usage: node scripts/check-preview-image.mjs');
		const result = verifyPreviewImageOutput(path.join(REPOSITORY, 'website/dist'));
		process.stdout.write(`Verified README and preview image in ${result.pages} pages (${result.width}x${result.height}, original PNG bytes).\n`);
	} catch (error) {
		process.stderr.write(`${error.message}\n`);
		process.exitCode = 1;
	}
}
