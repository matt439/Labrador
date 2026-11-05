import assert from 'node:assert/strict';
import { mkdtempSync, rmSync } from 'node:fs';
import net from 'node:net';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { parseRange, startServer } from '../acceptance/server.mjs';
import { write } from './fixtures/snapshot-site.mjs';

// A miniature built site: development pages, one release with its own 404
// page, a binary search index and a secret beside the root.
async function serveSite(t) {
	const parent = mkdtempSync(path.join(tmpdir(), 'labrador-acceptance-server-'));
	const root = path.join(parent, 'dist');
	write(root, 'index.html', '<p>home</p>');
	write(root, '404.html', '<p>development 404</p>');
	write(root, 'docs/index.html', '<p>docs</p>');
	write(root, 'docs/guides/ci/index.html', '<p>ci guide</p>');
	write(root, 'versions/index.json', '{"schemaVersion":1}\n');
	write(root, 'versions/v0.3.0/index.html', '<p>v0.3.0 home</p>');
	write(root, 'versions/v0.3.0/404.html', '<p>v0.3.0 404</p>');
	write(root, 'versions/v0.3.0/docs/index.html', '<p>v0.3.0 docs</p>');
	write(root, 'pagefind/pagefind-entry.json', '{}');
	write(root, 'pagefind/index/en_1.pf_index', Buffer.from([0, 1, 2, 3]));
	write(root, 'pagefind/fragment/en_1.pf_fragment', Buffer.from([4]));
	write(root, 'pagefind/pagefind.en_1.pf_meta', Buffer.from([5]));
	write(root, 'pagefind/wasm.en.pagefind', Buffer.from([6]));
	write(root, 'module.wasm', Buffer.from([0, 0x61, 0x73, 0x6d]));
	write(root, 'bytes.bin', Buffer.from('0123456789'));
	write(parent, 'secret.txt', 'outside the root');
	const server = await startServer({ root, port: 0 });
	t.after(async () => {
		await server.close();
		rmSync(parent, { recursive: true, force: true });
	});
	return server;
}

function get(server, pathname, init = {}) {
	return fetch(new URL(pathname, server.url), { redirect: 'manual', ...init });
}

// fetch normalizes dot segments away, so traversal must be sent raw.
function rawRequest(server, target) {
	return new Promise((resolve, reject) => {
		const socket = net.connect(server.port, '127.0.0.1', () => {
			socket.write(`GET ${target} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`);
		});
		let text = '';
		socket.setEncoding('latin1');
		socket.on('data', (chunk) => { text += chunk; });
		socket.on('end', () => resolve({ status: Number(text.split(' ', 2)[1]), text }));
		socket.on('error', reject);
	});
}

test('startServer listens on loopback by default with an ephemeral port', async (t) => {
	const server = await serveSite(t);
	assert.match(server.url, /^http:\/\/127\.0\.0\.1:\d+\/$/);
	assert.notEqual(server.port, 0);
	const response = await get(server, '/');
	assert.equal(response.status, 200);
	assert.equal(response.headers.get('content-type'), 'text/html; charset=utf-8');
	assert.equal(await response.text(), '<p>home</p>');
});

test('a directory without its trailing slash redirects, keeping the query', async (t) => {
	const server = await serveSite(t);
	const response = await get(server, '/docs/guides/ci?from=v0.3.0');
	assert.equal(response.status, 301);
	assert.equal(response.headers.get('location'), '/docs/guides/ci/?from=v0.3.0');
	const followed = await get(server, '/docs/guides/ci/');
	assert.equal(followed.status, 200);
	assert.equal(await followed.text(), '<p>ci guide</p>');
});

test('a missing path gets 404 with the nearest 404.html at two depths', async (t) => {
	const server = await serveSite(t);
	const development = await get(server, '/docs/guides/missing/');
	assert.equal(development.status, 404);
	assert.equal(development.headers.get('content-type'), 'text/html; charset=utf-8');
	assert.equal(await development.text(), '<p>development 404</p>');
	const release = await get(server, '/versions/v0.3.0/docs/guides/missing/');
	assert.equal(release.status, 404);
	assert.equal(await release.text(), '<p>v0.3.0 404</p>');
	const releaseFile = await get(server, '/versions/v0.3.0/missing.png');
	assert.equal(releaseFile.status, 404);
	assert.equal(await releaseFile.text(), '<p>v0.3.0 404</p>');
	// A release without its own 404 page falls back to the development one.
	const unknown = await get(server, '/versions/v9.9.9/docs/');
	assert.equal(unknown.status, 404);
	assert.equal(await unknown.text(), '<p>development 404</p>');
});

test('names match case-sensitively, as on the host, on any file system', async (t) => {
	const server = await serveSite(t);
	assert.equal((await get(server, '/versions/v0.3.0/docs/')).status, 200);
	for (const misCased of ['/Versions/v0.3.0/docs/', '/versions/V0.3.0/docs/', '/versions/v0.3.0/Docs/', '/Bytes.bin', '/DOCS']) {
		const response = await get(server, misCased);
		assert.equal(response.status, 404, misCased);
		await response.arrayBuffer();
	}
	// A mis-cased release folder is not that release, so its 404 page is not used either.
	const release = await get(server, '/versions/V0.3.0/missing/');
	assert.equal(await release.text(), '<p>development 404</p>');
});

test('path traversal is refused and never reads outside the root', async (t) => {
	const server = await serveSite(t);
	for (const target of ['/../secret.txt', '/docs/../../secret.txt', '/%2e%2e/secret.txt', '/..%2fsecret.txt', '/..%5csecret.txt', '/%00']) {
		const response = await rawRequest(server, target);
		assert.equal(response.status, 400, target);
		assert.doesNotMatch(response.text, /outside the root/, target);
	}
});

test('content types cover search index files and WebAssembly', async (t) => {
	const server = await serveSite(t);
	const expected = {
		'/pagefind/index/en_1.pf_index': 'application/octet-stream',
		'/pagefind/fragment/en_1.pf_fragment': 'application/octet-stream',
		'/pagefind/pagefind.en_1.pf_meta': 'application/octet-stream',
		'/pagefind/wasm.en.pagefind': 'application/octet-stream',
		'/pagefind/pagefind-entry.json': 'application/json; charset=utf-8',
		'/module.wasm': 'application/wasm',
	};
	for (const [pathname, type] of Object.entries(expected)) {
		const response = await get(server, pathname);
		assert.equal(response.status, 200, pathname);
		assert.equal(response.headers.get('content-type'), type, pathname);
		await response.arrayBuffer();
	}
});

test('byte ranges are answered with 206 and refused with 416 past the end', async (t) => {
	const server = await serveSite(t);
	const middle = await get(server, '/bytes.bin', { headers: { Range: 'bytes=2-5' } });
	assert.equal(middle.status, 206);
	assert.equal(middle.headers.get('content-range'), 'bytes 2-5/10');
	assert.equal(middle.headers.get('content-length'), '4');
	assert.equal(await middle.text(), '2345');
	const open = await get(server, '/bytes.bin', { headers: { Range: 'bytes=7-' } });
	assert.equal(open.status, 206);
	assert.equal(await open.text(), '789');
	const suffix = await get(server, '/bytes.bin', { headers: { Range: 'bytes=-3' } });
	assert.equal(suffix.status, 206);
	assert.equal(suffix.headers.get('content-range'), 'bytes 7-9/10');
	assert.equal(await suffix.text(), '789');
	const past = await get(server, '/bytes.bin', { headers: { Range: 'bytes=10-' } });
	assert.equal(past.status, 416);
	assert.equal(past.headers.get('content-range'), 'bytes */10');
	await past.arrayBuffer();
	const whole = await get(server, '/bytes.bin');
	assert.equal(whole.status, 200);
	assert.equal(whole.headers.get('accept-ranges'), 'bytes');
	assert.equal(await whole.text(), '0123456789');
});

test('HEAD answers headers without a body, for files, ranges and 404 pages', async (t) => {
	const server = await serveSite(t);
	const file = await get(server, '/bytes.bin', { method: 'HEAD' });
	assert.equal(file.status, 200);
	assert.equal(file.headers.get('content-length'), '10');
	assert.equal(await file.text(), '');
	const range = await get(server, '/bytes.bin', { method: 'HEAD', headers: { Range: 'bytes=0-0' } });
	assert.equal(range.status, 206);
	assert.equal(range.headers.get('content-length'), '1');
	assert.equal(await range.text(), '');
	const missing = await get(server, '/versions/v0.3.0/nothing/', { method: 'HEAD' });
	assert.equal(missing.status, 404);
	assert.equal(missing.headers.get('content-type'), 'text/html; charset=utf-8');
	assert.equal(await missing.text(), '');
	const post = await get(server, '/', { method: 'POST' });
	assert.equal(post.status, 405);
	await post.arrayBuffer();
});

test('parseRange accepts single ranges and ignores what it cannot parse', () => {
	assert.deepEqual(parseRange('bytes=0-3', 10), { start: 0, end: 3 });
	assert.deepEqual(parseRange('bytes=5-100', 10), { start: 5, end: 9 });
	assert.deepEqual(parseRange('bytes=-20', 10), { start: 0, end: 9 });
	assert.equal(parseRange('bytes=4-2', 10), 'unsatisfiable');
	assert.equal(parseRange('bytes=-0', 10), 'unsatisfiable');
	assert.equal(parseRange('bytes=0-1,4-5', 10), null);
	assert.equal(parseRange('items=0-1', 10), null);
	assert.equal(parseRange(undefined, 10), null);
});
