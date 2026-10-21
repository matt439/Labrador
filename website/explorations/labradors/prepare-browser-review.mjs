import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const repo = fileURLToPath(new URL('../../../', import.meta.url));
const output = path.join(repo, 'out/labrador-dog-concepts');
const harness = path.join(output, 'browser-harness');
await mkdir(harness, { recursive: true });
for (const file of ['index.html', 'duo.html', 'guides.html']) {
	const html = await readFile(path.join(output, 'offline', file), 'utf8');
	await writeFile(path.join(harness, file), html.replace('<head>', '<head><link rel="icon" href="data:,"><script>window.labradorExit = new Promise(() => {});</script>'));
}
