import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

// The render test writes rejected frames to its Emscripten filesystem.
export const PIXEL_FILES = `JSON.stringify((() => {
	const fs = Module.FS;
	if (!fs) throw new Error('Missing pixel-test filesystem');
	if (!fs.readdir('/').includes('golden-actual')) return {};
	const out = Object.create(null);
	for (const name of fs.readdir('/golden-actual')) {
		if (name === '.' || name === '..') continue;
		const bytes = fs.readFile('/golden-actual/' + name);
		let text = '';
		for (let i = 0; i < bytes.length; i += 0x8000)
			text += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
		out[name] = btoa(text);
	}
	return out;
})())`;

export function save_pixel_files(files, output_dir) {
	for (const [name, data] of Object.entries(files)) {
		if (!/^[A-Za-z0-9_.-]+\.png$/.test(name) || name.includes('..') || typeof data !== 'string')
			throw new Error(`Invalid pixel artifact name or data: ${name}`);
		mkdirSync(output_dir, { recursive: true });
		writeFileSync(join(output_dir, name), Buffer.from(data, 'base64'));
	}
}
