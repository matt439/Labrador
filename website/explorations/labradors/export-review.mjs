import { readFile, writeFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Inline the isolated Astro build's own assets for double-clickable reviews.
// Run from website/ after building with this study's config.
const website = fileURLToPath(new URL('../../', import.meta.url));
const output = path.resolve(website, '../out/labrador-dog-concepts');
const review = path.join(output, 'offline');
await mkdir(review, { recursive: true });

for (const [route, filename] of [['', 'index.html'], ['duo', 'duo.html'], ['guides', 'guides.html']]) {
	let html = await readFile(path.join(output, route, 'index.html'), 'utf8');
	const cssLinks = [...html.matchAll(/<link rel="stylesheet" href="(\/_astro\/[^"?]+\.css)">/g)];
	if (!cssLinks.length) throw new Error(`No generated stylesheet found for ${filename}`);
	for (const [tag, url] of cssLinks) {
		const css = await readFile(path.join(output, url.slice(1)), 'utf8');
		html = html.replace(tag, `<style>${css.replaceAll('</style', '<\\/style')}</style>`);
	}
	for (const [attribute, url] of [...html.matchAll(/src="(\/_astro\/[^"?]+)"/g)]) {
		const extension = path.extname(url);
		const mime = { '.svg': 'image/svg+xml', '.png': 'image/png' }[extension];
		if (!mime) throw new Error(`Unrecognised image type: ${url}`);
		const bytes = await readFile(path.join(output, url.slice(1)));
		html = html.replace(attribute, `src="data:${mime};base64,${bytes.toString('base64')}"`);
	}
	for (const [url, file] of [['/', 'index.html'], ['/duo/', 'duo.html'], ['/guides/', 'guides.html']]) {
		html = html.replaceAll(`href="${url}"`, `href="${file}"`);
	}
	if (/\b(?:src|href)="\//.test(html)) throw new Error(`Non-portable resource remains in ${filename}`);
	await writeFile(path.join(review, filename), html, 'utf8');
	console.log(`Wrote offline/${filename}`);
}

await writeFile(path.join(review, 'README.txt'), [
	'Vader & Ani — Labrador website voice study',
	'',
	'Open index.html in your browser. Keep the three HTML files together.',
	'Use the direction links to compare dry commentary, duo banter and warm guides.',
	'Each page has light/dark themes, two page contexts and asides/footer-only/off controls.',
	'Images, styles and scripts are embedded. No server or installation is required.',
	'Documentation links open the existing public Labrador website when clicked.',
	'All portraits are placeholders. All characterization is proposed fiction.',
	'Seven screenshots and three rendered Chrome test reports are included in the screenshots folder.',
	'Full screen-reader review, all-control keyboard traversal and actual 200% browser zoom remain outstanding.',
	'Nothing has been published or integrated into the real website.',
	'',
].join('\n'), 'utf8');
