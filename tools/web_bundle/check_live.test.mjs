// check_live.mjs against a loopback server standing in for the demo's host:
// what the live site served on 2026-10-13 (docs/port/web-to-be-tested.md), and
// one departure from it per failing case.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { createServer } from 'node:http';
import { test } from 'node:test';
import { brotliCompressSync } from 'node:zlib';
import { check_live, csp_allows_wasm, record } from './check_live.mjs';

const sha256 = (bytes) => createHash('sha256').update(bytes).digest('hex');
const COMMIT = '0f4f5033b317a334d89e3cf14ab98684e0598ddd';

function site(change = {}) {
	const files = {
		'linesweeper.js': Buffer.from('export default async function createLineSweeper() {}\n'),
		'linesweeper.wasm': Buffer.from([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]),
		'poster.png': Buffer.from('not really a png'),
		'NOTICE.txt': Buffer.from('notices\n'),
	};
	const folder = change.folder ?? sha256(Buffer.concat([files['linesweeper.js'], files['linesweeper.wasm']])).slice(0, 12);
	const build = { commit: COMMIT, dirty: false, build_type: 'Release', emscripten: '6.0.12', files: {
		'index.html': { bytes: 1, brotli: 1, sha256: '0'.repeat(64) },
	} };
	for (const [name, bytes] of Object.entries(files)) {
		build.files[name] = { bytes: bytes.length, brotli: 1, sha256: sha256(bytes) };
	}
	Object.assign(build, change.build);
	if (change.served) { Object.assign(files, change.served); }
	const page = Buffer.from(`<span id="built">Built from Labrador ${COMMIT.slice(0, 7)}, November 2026</span>
<script type="module">const MODULE = './m/${folder}/linesweeper.js';</script>`);
	const routes = {
		'/demo/linesweeper/': [page, 'text/html', 'no-cache'],
		'/demo/linesweeper/BUILD.json': [Buffer.from(JSON.stringify(build)), 'application/json', 'public, max-age=0, must-revalidate'],
		'/demo/linesweeper/SITE.json': [Buffer.from(JSON.stringify({ commit: change.site_commit ?? COMMIT })), 'application/json', 'public, max-age=0, must-revalidate'],
		[`/demo/linesweeper/m/${folder}/linesweeper.js`]: [files['linesweeper.js'], 'text/javascript', 'public, max-age=31536000, immutable'],
		[`/demo/linesweeper/m/${folder}/linesweeper.wasm`]: [files['linesweeper.wasm'], 'application/wasm', change.wasm_cache ?? 'public, max-age=31536000, immutable'],
		'/demo/linesweeper/poster.png': [files['poster.png'], 'image/png', 'public, max-age=0, must-revalidate'],
		'/demo/linesweeper/NOTICE.txt': [files['NOTICE.txt'], 'text/plain; charset=utf-8', 'public, max-age=0, must-revalidate'],
	};
	const server = createServer((request, response) => {
		const route = routes[new URL(request.url, 'http://x').pathname];
		if (!route) { response.writeHead(404).end(); return; }
		const [bytes, type, cache] = route;
		const headers = { 'Content-Type': type, 'Cache-Control': cache, ...change.headers };
		const brotli = /\bbr\b/.test(request.headers['accept-encoding'] ?? '') && type !== 'image/png';
		if (brotli) { headers['Content-Encoding'] = 'br'; }
		response.writeHead(200, headers);
		response.end(brotli ? brotliCompressSync(bytes) : bytes);
	});
	return new Promise((ready) => server.listen(0, '127.0.0.1', () => ready(server)));
}

async function run(t, change, options = {}) {
	const server = await site(change);
	t.after(() => server.close());
	const url = `http://127.0.0.1:${server.address().port}/demo/linesweeper/`;
	const outcome = await check_live({ url, ...options });
	const failed = outcome.results.filter((result) => !result.passed).map((result) => result.name);
	return { outcome, failed, url };
}

test('a deploy served as the host serves it passes every check', async (t) => {
	const { outcome, failed, url } = await run(t, {});
	assert.deepEqual(failed, []);
	assert.equal(outcome.commit, COMMIT);
	assert.ok(outcome.results.length >= 25, `${outcome.results.length} checks`);
	const text = record(url, outcome, new Date('2026-11-04T00:00:00Z'));
	assert.match(text, /^check_live\.mjs, 2026-11-04T00:00:00Z: http/);
	assert.match(text, new RegExp(`served commit ${COMMIT}`));
	assert.doesNotMatch(text, /FAIL/);
});

test('the commit asked for is the commit served', async (t) => {
	assert.deepEqual((await run(t, {}, { expect_commit: '0f4f503' })).failed, []);
	assert.deepEqual((await run(t, {}, { expect_commit: '1e60fb2' })).failed, ['BUILD.json: names 1e60fb2']);
});

test('a module that is not cached as immutable fails', async (t) => {
	const { failed } = await run(t, { wasm_cache: 'public, max-age=0, must-revalidate' });
	assert.deepEqual(failed, ['linesweeper.wasm: Cache-Control immutable']);
});

test('served bytes that differ from BUILD.json fail, and so does the folder name', async (t) => {
	const { failed } = await run(t, { served: { 'linesweeper.wasm': Buffer.from('changed') } });
	assert.deepEqual(failed, ['linesweeper.wasm: size and SHA-256 are BUILD.json\'s',
		'm/<hash>/ is named by the loader and the module']);
});

test('a dirty build and a site record of another commit fail', async (t) => {
	const { failed } = await run(t, { build: { dirty: true }, site_commit: '1'.repeat(40) });
	assert.deepEqual(failed, ['BUILD.json: a full commit, clean, Release', 'SITE.json: the same commit']);
});

test('cross-origin isolation and a policy that refuses wasm fail', async (t) => {
	const { failed } = await run(t, { headers: {
		'Cross-Origin-Opener-Policy': 'same-origin',
		'Content-Security-Policy': "script-src 'self'",
	} });
	assert.deepEqual(failed, ['page: no COOP or COEP', 'page: Content-Security-Policy allows WebAssembly']);
});

test('a missing module folder fails without stopping the other checks', async (t) => {
	const { failed, outcome } = await run(t, { served: {}, folder: 'nothexnothex' });
	assert.ok(failed.includes('page: names its module folder m/<hash>/'));
	assert.ok(outcome.results.some((result) => result.name.startsWith('NOTICE.txt') && result.passed));
});

test('Content-Security-Policy directives are read as a browser reads them', () => {
	assert.equal(csp_allows_wasm("frame-ancestors 'none'"), true);
	assert.equal(csp_allows_wasm("script-src 'self' 'wasm-unsafe-eval'"), true);
	assert.equal(csp_allows_wasm("default-src 'self'"), false);
	assert.equal(csp_allows_wasm("default-src 'self'; script-src 'self' 'unsafe-eval'"), true);
	assert.equal(csp_allows_wasm("default-src 'self' 'wasm-unsafe-eval'; script-src 'self'"), false);
});
