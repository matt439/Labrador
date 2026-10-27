// Read-only qualification of one independently built release site. This does
// not create a published-release record or copy files into a deployment.
import { readFileSync, readdirSync, realpathSync, statSync } from 'node:fs';
import path from 'node:path';
import { Worker } from 'node:worker_threads';
import { parse } from 'parse5';

const SITE_ORIGIN = 'https://labradorengine.com';
const REPOSITORY_PATH = '/matt439/Labrador';
const VERSION_INDEX = `${SITE_ORIGIN}/versions/`;

function sourcePrefix(source) {
	if (source?.schemaVersion !== 1 || !/^v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$/.test(source.tag)
		|| !/^(?:[0-9a-f]{40}|[0-9a-f]{64})$/.test(source.commit)
		|| source.snapshotPath !== `/versions/${source.tag}/`) {
		throw new Error('Snapshot output requires a verified release-source record.');
	}
	return source.snapshotPath;
}

function outputFiles(directory) {
	const files = [];
	function walk(folder) {
		for (const entry of readdirSync(folder, { withFileTypes: true })) {
			const filename = path.join(folder, entry.name);
			if (entry.isSymbolicLink()) throw new Error(`Snapshot output contains a symbolic link: ${filename}`);
			if (entry.isDirectory()) walk(filename);
			else if (entry.isFile()) files.push(path.relative(directory, filename).split(path.sep).join('/'));
			else throw new Error(`Snapshot output contains a non-file entry: ${filename}`);
		}
	}
	walk(directory);
	return files;
}

/** The route an output HTML file is served at, relative to its site's base. */
export function pagePath(file) {
	if (file === 'index.html') return '';
	return file.endsWith('/index.html') ? file.slice(0, -'index.html'.length) : file.replace(/\.html$/, '/');
}

function* elements(node) {
	if (node.tagName) yield node;
	for (const child of node.childNodes ?? []) yield* elements(child);
}

function visibleText(node) {
	if (['script', 'style', 'template'].includes(node.tagName)) return '';
	if (node.nodeName === '#text') return node.value;
	return (node.childNodes ?? []).map(visibleText).join(' ');
}

// The HTML parser decodes character references before URL resolution. Treat
// commas in a data URL as part of its URL, as the browser's srcset parser does.
function srcsetUrls(value) {
	const urls = [];
	let remaining = value.trim();
	while (remaining) {
		remaining = remaining.replace(/^[\s,]+/, '');
		if (!remaining) break;
		const match = /^\S+/.exec(remaining);
		const token = match[0];
		remaining = remaining.slice(token.length);
		if (token.endsWith(',')) urls.push(token.replace(/,+$/, ''));
		else {
			urls.push(token);
			// Descriptors contain no URLs. A comma ends this candidate.
			const separator = remaining.indexOf(',');
			remaining = separator < 0 ? '' : remaining.slice(separator + 1);
		}
	}
	return urls;
}

// Run the emitted Pagefind module in its own Node worker. Its fetch function
// can only read the isolated output; it cannot accidentally query development
// search or the network. Explicit basePath models importing this module from
// its hosted snapshot URL. No baseUrl override hides the module's routing.
const SEARCH_WORKER = `
const { parentPort, workerData } = require('node:worker_threads');
const { readFileSync } = require('node:fs');
const path = require('node:path');
(async () => {
  const { directory, prefix, origin } = workerData;
  globalThis.fetch = async (input) => {
    const url = new URL(typeof input === 'string' ? input : input.url ?? input.href, origin);
    if (url.origin !== origin || !url.pathname.startsWith(prefix)) {
      throw new Error('Pagefind fetched outside its snapshot: ' + url.href);
    }
    const relative = decodeURIComponent(url.pathname.slice(prefix.length));
    if (relative.includes('\\\\') || relative.includes('\\0')) throw new Error('Unsafe Pagefind asset path');
    const file = path.resolve(directory, relative);
    if (!file.startsWith(directory + path.sep)) throw new Error('Pagefind asset escaped its output');
    return new Response(readFileSync(file), { headers: { 'Content-Type': 'application/octet-stream' } });
  };
  const code = readFileSync(path.join(directory, 'pagefind/pagefind.js'));
  const runtime = await import('data:text/javascript;base64,' + code.toString('base64'));
  await runtime.options({ basePath: origin + prefix + 'pagefind/', noWorker: true });
  const all = await runtime.search(null);
  if (!all || !Array.isArray(all.results) || all.results.length === 0) throw new Error('Pagefind returned no indexed pages');
  const pages = await Promise.all(all.results.map((result) => result.data()));
  const term = pages.map((page) => page.content ?? '').join(' ').match(/[A-Za-z]{3,24}/)?.[0];
  if (!term) throw new Error('Pagefind returned no searchable content');
  const query = await runtime.search(term);
  if (!query?.results?.length) throw new Error('Pagefind could not search an indexed term: ' + term);
  const matches = await Promise.all(query.results.map((result) => result.data()));
  const urls = [...pages, ...matches].flatMap((page) => [page.url, ...(page.sub_results ?? []).map((item) => item.url)]);
  await runtime.destroy();
  parentPort.postMessage({ urls, pages: pages.length });
})().catch((error) => parentPort.postMessage({ error: error.message }));
`;

function verifySearch(directory, prefix) {
	return new Promise((resolve, reject) => {
		let reported = false;
		const worker = new Worker(SEARCH_WORKER, {
			eval: true, execArgv: [], workerData: { directory, prefix, origin: SITE_ORIGIN },
		});
		const timer = setTimeout(() => {
			void worker.terminate();
			reject(new Error('Snapshot Pagefind verification timed out after 30 seconds.'));
		}, 30_000);
		worker.once('message', (result) => {
			reported = true;
			clearTimeout(timer);
			void worker.terminate();
			if (result.error) reject(new Error(`Snapshot Pagefind verification failed: ${result.error}`));
			else resolve(result);
		});
		worker.once('error', (error) => {
			clearTimeout(timer);
			reject(error);
		});
		worker.once('exit', (code) => {
			clearTimeout(timer);
			if (!reported) reject(new Error(`Snapshot Pagefind verifier exited with code ${code} before reporting a result.`));
		});
	});
}

/** Verify a flat Astro output served below the source record's snapshotPath. */
export async function verifySnapshotOutput(outputDirectory, sourceRecord) {
	const prefix = sourcePrefix(sourceRecord);
	const directory = realpathSync(outputDirectory);
	if (!statSync(directory).isDirectory()) throw new Error('Snapshot output must be a directory.');
	const files = new Set(outputFiles(directory));
	const pages = [...files].filter((file) => file.endsWith('.html'));
	if (!pages.length || !files.has('index.html')) throw new Error('Snapshot output has no homepage.');
	let references = 0;
	function verifyUrl(value, pageUrl, location, { canonical = false, search = false, releaseExample = false, versionIndex = false } = {}) {
		if (versionIndex) {
			// Every version links to the development site's one list of versions,
			// which no snapshot contains. Only that exact page may be marked.
			let target;
			try { target = new URL(value ?? '', pageUrl).href; }
			catch { throw new Error(`${location}: invalid URL ${JSON.stringify(value)}`); }
			if (target !== VERSION_INDEX) throw new Error(`${location}: expected a version-index link to ${VERSION_INDEX}, found ${target}`);
			return;
		}
		if (!value || value.startsWith('#')) return;
		let url;
		try { url = new URL(value, pageUrl); }
		catch { throw new Error(`${location}: invalid URL ${JSON.stringify(value)}`); }
		if (!['http:', 'https:'].includes(url.protocol)) {
			if (canonical || search) throw new Error(`${location}: expected a snapshot HTTP URL.`);
			return;
		}
		if (url.origin === 'https://github.com' && url.pathname.toLowerCase().startsWith(`${REPOSITORY_PATH.toLowerCase()}/`)) {
			const match = /^\/matt439\/Labrador\/(blob|tree|edit)\/([^/]+)(?:\/|$)/i.exec(url.pathname);
			// A guide may deliberately compare historical releases. Only an
			// explicitly marked stable-tag example can leave the current source.
			const historical = releaseExample && match && match[1].toLowerCase() !== 'edit'
				&& /^v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$/.test(match[2]);
			if (match && !historical && (match[1].toLowerCase() === 'edit' || match[2] !== sourceRecord.commit)) {
				throw new Error(`${location}: source link does not name the full release commit: ${url.href}`);
			}
		}
		if (url.origin !== SITE_ORIGIN) {
			if (canonical || search) throw new Error(`${location}: URL is outside the snapshot origin: ${url.href}`);
			return;
		}
		if (!url.pathname.startsWith(prefix)) throw new Error(`${location}: URL escapes snapshot prefix ${prefix}: ${url.href}`);
		if (canonical && (url.pathname !== pageUrl.pathname || url.search || url.hash)) {
			throw new Error(`${location}: canonical URL does not identify its snapshot page: ${url.href}`);
		}
		let relative;
		try { relative = decodeURIComponent(url.pathname.slice(prefix.length)); }
		catch { throw new Error(`${location}: malformed URL encoding: ${url.href}`); }
		if (relative.includes('\\') || relative.includes('\0') || relative.split('/').some((part) => part === '..' || part === '.')) {
			throw new Error(`${location}: unsafe snapshot path: ${url.href}`);
		}
		const candidates = [relative, `${relative.replace(/\/$/, '')}/index.html`, `${relative.replace(/\/$/, '')}.html`];
		if (relative === '') candidates.push('index.html');
		if (!candidates.some((file) => files.has(file))) throw new Error(`${location}: missing snapshot target: ${url.href}`);
		references++;
	}
	for (const file of pages) {
		const pageUrl = new URL(prefix + pagePath(file), SITE_ORIGIN);
		const document = parse(readFileSync(path.join(directory, file), 'utf8'));
		const nodes = [...elements(document)];
		const body = nodes.find((node) => node.tagName === 'body');
		if (!visibleText(body ?? document).split(/\s+/).some((word) => word.replace(/^[([]|[)\],.:;]$/g, '') === sourceRecord.tag)) {
			throw new Error(`${file}: missing visible release label ${sourceRecord.tag}.`);
		}
		let hasCommit = false;
		let canonicals = 0;
		for (const node of nodes) {
			const attrs = Object.fromEntries(node.attrs.map((attribute) => [attribute.name, attribute.value]));
			if (node.tagName === 'base') throw new Error(`${file}: base elements can change snapshot URL resolution.`);
			if (node.tagName === 'a' && attrs.href === `https://github.com${REPOSITORY_PATH}/commit/${sourceRecord.commit}`) hasCommit = true;
			const canonical = node.tagName === 'link' && (attrs.rel ?? '').split(/\s+/).includes('canonical');
			if (canonical) {
				canonicals++;
				if (!/^https:\/\//.test(attrs.href ?? '')) throw new Error(`${file}: canonical URL must be absolute HTTPS.`);
			}
			for (const attr of ['href', 'src', 'poster']) {
				if (attrs[attr] !== undefined) verifyUrl(attrs[attr], pageUrl, `${file} ${node.tagName}[${attr}]`, {
					canonical: canonical && attr === 'href',
					releaseExample: node.tagName === 'a' && attr === 'href' && Object.hasOwn(attrs, 'data-release-example'),
					versionIndex: node.tagName === 'a' && attr === 'href' && Object.hasOwn(attrs, 'data-version-index'),
				});
			}
			if (attrs.srcset) for (const url of srcsetUrls(attrs.srcset)) verifyUrl(url, pageUrl, `${file} ${node.tagName}[srcset]`);
			if (node.tagName === 'meta' && ['og:url', 'og:image', 'twitter:image'].includes(attrs.property ?? attrs.name)) {
				verifyUrl(attrs.content, pageUrl, `${file} meta[content]`);
			}
		}
		if (!hasCommit) throw new Error(`${file}: missing full release source-commit link.`);
		if (canonicals !== 1) throw new Error(`${file}: expected one canonical URL, found ${canonicals}.`);
	}
	for (const required of ['pagefind/pagefind.js', 'pagefind/pagefind-entry.json']) {
		if (!files.has(required)) throw new Error(`Snapshot output is missing ${required}.`);
	}
	const entry = JSON.parse(readFileSync(path.join(directory, 'pagefind/pagefind-entry.json'), 'utf8'));
	const indexedPages = Object.values(entry.languages ?? {}).reduce((total, language) => total + (language.page_count ?? 0), 0);
	if (!Number.isSafeInteger(indexedPages) || indexedPages < 1) throw new Error('Snapshot Pagefind entry has no indexed pages.');
	const search = await verifySearch(directory, prefix);
	if (search.pages !== indexedPages) throw new Error(`Snapshot Pagefind returned ${search.pages} pages; its entry declares ${indexedPages}.`);
	for (const url of search.urls) {
		if (typeof url !== 'string' || !url) throw new Error('Snapshot Pagefind returned a result without a URL.');
		verifyUrl(url, SITE_ORIGIN + prefix, 'Pagefind result', { search: true });
	}
	return { pages: pages.length, references, searchPages: search.pages };
}
