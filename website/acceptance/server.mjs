// Static server for a built website folder with the host's 404 semantics, so
// the acceptance suite sees what visitors see. A directory path without its
// trailing slash is redirected to it; a directory path serves its index.html;
// a missing path gets status 404 with the nearest 404.html above it, as the
// host's 404 page setting does, so a missing page under /versions/<tag>/ gets
// that release's own 404 page. Byte ranges are answered because Safari will
// not play MP4 without them. Names match case-sensitively, as on the host,
// whatever the local file system does. Nothing outside the root is ever read.
//
// Usage: node acceptance/server.mjs <dist> [port] [host]
// defaults to port 4321 on 127.0.0.1; port 0 picks a free port. A LAN host
// such as 0.0.0.0 is needed only to reach the fixture from another device.
import { createReadStream, readdirSync, realpathSync, statSync } from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { describeError } from '../scripts/snapshot-source.mjs';

export const CONTENT_TYPES = {
	'.html': 'text/html; charset=utf-8',
	'.css': 'text/css; charset=utf-8',
	'.js': 'text/javascript; charset=utf-8',
	'.mjs': 'text/javascript; charset=utf-8',
	'.json': 'application/json; charset=utf-8',
	'.txt': 'text/plain; charset=utf-8',
	'.xml': 'application/xml',
	'.svg': 'image/svg+xml',
	'.png': 'image/png',
	'.jpg': 'image/jpeg',
	'.jpeg': 'image/jpeg',
	'.webp': 'image/webp',
	'.avif': 'image/avif',
	'.gif': 'image/gif',
	'.ico': 'image/x-icon',
	'.mp4': 'video/mp4',
	'.webm': 'video/webm',
	'.wasm': 'application/wasm',
	'.woff': 'font/woff',
	'.woff2': 'font/woff2',
	'.gz': 'application/gzip',
	// Pagefind's index files are opaque binary.
	'.pagefind': 'application/octet-stream',
	'.pf_meta': 'application/octet-stream',
	'.pf_index': 'application/octet-stream',
	'.pf_fragment': 'application/octet-stream',
};

function isFile(file) {
	try {
		return statSync(file).isFile();
	} catch {
		return false;
	}
}

// Whether every name from the root down to file is spelled exactly as on disk.
// Windows and default macOS volumes find /Docs/ for docs/, but the host is
// case-sensitive, so a mis-cased link must be missing here as it is there.
function hasExactCase(root, file) {
	let directory = root;
	for (const name of path.relative(root, file).split(path.sep)) {
		if (name === '') continue;
		let names;
		try {
			names = readdirSync(directory);
		} catch {
			return false;
		}
		if (!names.includes(name)) return false;
		directory = path.join(directory, name);
	}
	return true;
}

function isServedFile(root, file) {
	return isFile(file) && hasExactCase(root, file);
}

/** Whether child is parent or a path inside it. */
export function isInside(child, parent) {
	const relative = path.relative(parent, child);
	return relative === '' || (!relative.startsWith('..') && !path.isAbsolute(relative));
}

/**
 * The URL path's decoded segments, or null when it cannot name a file inside
 * the root: undecodable escapes, dot segments, NUL, backslashes or a drive.
 */
function segments(pathname) {
	let decoded;
	try {
		decoded = decodeURIComponent(pathname);
	} catch {
		return null;
	}
	if (!decoded.startsWith('/') || /[\\\0]/.test(decoded)) return null;
	const parts = decoded.split('/').slice(1);
	if (parts.some((part) => part === '.' || part === '..' || /^[A-Za-z]:/.test(part))) return null;
	return parts;
}

/**
 * Decide the answer for a request path without reading file contents:
 * { status: 200, file }, { status: 301, location } or { status: 404, file }
 * (file is null when no 404.html applies), or { status: 400 } for a path that
 * cannot name anything inside the root.
 */
export function resolveRequest(root, requestUrl) {
	// The raw target, not URL parsing, which would quietly resolve dot segments.
	const queryAt = requestUrl.search(/[?#]/);
	const url = {
		pathname: queryAt === -1 ? requestUrl : requestUrl.slice(0, queryAt),
		search: queryAt === -1 || requestUrl[queryAt] === '#' ? '' : requestUrl.slice(queryAt).split('#')[0],
	};
	const parts = segments(url.pathname);
	if (parts === null) return { status: 400 };
	const full = path.join(root, ...parts);
	if (!isInside(full, root)) return { status: 400 };
	if (isServedFile(root, full)) return { status: 200, file: full };
	if (isServedFile(root, path.join(full, 'index.html'))) {
		if (url.pathname.endsWith('/')) return { status: 200, file: path.join(full, 'index.html') };
		return { status: 301, location: `${url.pathname}/${url.search}` };
	}
	if (!url.pathname.endsWith('/') && isServedFile(root, `${full}.html`)) return { status: 200, file: `${full}.html` };
	// The nearest 404.html at or above the requested path's directory.
	let directory = url.pathname.endsWith('/') ? full : path.dirname(full);
	while (isInside(directory, root)) {
		const candidate = path.join(directory, '404.html');
		if (isServedFile(root, candidate)) return { status: 404, file: candidate };
		if (directory === root) break;
		directory = path.dirname(directory);
	}
	return { status: 404, file: null };
}

function contentType(file) {
	return CONTENT_TYPES[path.extname(file).toLowerCase()] ?? 'application/octet-stream';
}

/** Parse a single bytes= range against a size: { start, end }, 'unsatisfiable', or null to ignore it. */
export function parseRange(header, size) {
	const match = /^bytes=(\d*)-(\d*)$/.exec(header ?? '');
	if (!match || (match[1] === '' && match[2] === '')) return null;
	let start;
	let end;
	if (match[1] === '') {
		const suffix = Number(match[2]);
		if (suffix === 0) return 'unsatisfiable';
		start = Math.max(0, size - suffix);
		end = size - 1;
	} else {
		start = Number(match[1]);
		end = match[2] === '' ? size - 1 : Math.min(Number(match[2]), size - 1);
	}
	if (start >= size || start > end) return 'unsatisfiable';
	return { start, end };
}

function handle(root, request, response, log) {
	if (request.method !== 'GET' && request.method !== 'HEAD') {
		response.writeHead(405, { Allow: 'GET, HEAD' });
		response.end();
		return;
	}
	const answer = resolveRequest(root, request.url);
	log(`${answer.status} ${request.method} ${request.url}`);
	if (answer.status === 400) {
		response.writeHead(400, { 'Content-Type': 'text/plain; charset=utf-8' });
		response.end('Bad request\n');
		return;
	}
	if (answer.status === 301) {
		response.writeHead(301, { Location: answer.location });
		response.end();
		return;
	}
	if (!answer.file) {
		response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
		response.end(request.method === 'HEAD' ? undefined : 'Not found\n');
		return;
	}
	const size = statSync(answer.file).size;
	const headers = {
		'Content-Type': contentType(answer.file),
		'Accept-Ranges': 'bytes',
		'Cache-Control': 'no-store',
	};
	const range = answer.status === 200 ? parseRange(request.headers.range, size) : null;
	if (range === 'unsatisfiable') {
		response.writeHead(416, { 'Content-Range': `bytes */${size}` });
		response.end();
		return;
	}
	let status = answer.status;
	let options = {};
	if (range) {
		status = 206;
		headers['Content-Range'] = `bytes ${range.start}-${range.end}/${size}`;
		options = range;
	}
	headers['Content-Length'] = range ? range.end - range.start + 1 : size;
	response.writeHead(status, headers);
	if (request.method === 'HEAD' || headers['Content-Length'] === 0) {
		response.end();
		return;
	}
	const stream = createReadStream(answer.file, options);
	stream.on('error', () => response.destroy());
	stream.pipe(response);
}

/**
 * Serve root until close() resolves. Port 0 picks a free port; the returned
 * url always ends in a slash and names a host the caller can fetch.
 */
export function startServer({ root, port = 4321, host = '127.0.0.1', log = () => {} } = {}) {
	if (!root) throw new Error('startServer: expected a root folder.');
	const resolvedRoot = realpathSync(path.resolve(root));
	if (!statSync(resolvedRoot).isDirectory()) throw new Error(`${resolvedRoot}: expected a folder to serve.`);
	const server = http.createServer((request, response) => {
		try {
			handle(resolvedRoot, request, response, log);
		} catch (error) {
			log(`500 ${request.method} ${request.url}: ${error.message}`);
			if (!response.headersSent) response.writeHead(500);
			response.end();
		}
	});
	const sockets = new Set();
	server.on('connection', (socket) => {
		sockets.add(socket);
		socket.on('close', () => sockets.delete(socket));
	});
	return new Promise((resolve, reject) => {
		server.once('error', reject);
		server.listen(port, host, () => {
			server.off('error', reject);
			const bound = server.address().port;
			// A wildcard address is not fetchable everywhere; loopback is.
			const urlHost = host === '0.0.0.0' || host === '::' ? '127.0.0.1' : host.includes(':') ? `[${host}]` : host;
			resolve({
				url: `http://${urlHost}:${bound}/`,
				port: bound,
				root: resolvedRoot,
				close() {
					return new Promise((done) => {
						server.close(() => done());
						for (const socket of sockets) socket.destroy();
					});
				},
			});
		});
	});
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		const [root, port = '4321', host = '127.0.0.1'] = process.argv.slice(2);
		if (!root || !/^\d+$/.test(port)) throw new Error('Usage: node acceptance/server.mjs <dist> [port] [host]');
		const server = await startServer({
			root, port: Number(port), host, log: (line) => process.stderr.write(`${line}\n`),
		});
		process.stdout.write(`serving ${server.root} at ${server.url}\n`);
		const stop = () => server.close().then(() => process.exit(0));
		process.once('SIGINT', stop);
		process.once('SIGTERM', stop);
	} catch (error) {
		process.stderr.write(`${describeError(error)}\n`);
		process.exitCode = 1;
	}
}
