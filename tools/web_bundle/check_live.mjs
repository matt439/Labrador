// Checks a deployed LineSweeper demo from the outside, as a browser sees it,
// and prints a dated record of what it found.
//
//   node check_live.mjs [--url <page>] [--expect-commit <revision>]
//                       [--out <record.md>]
//
// --url defaults to https://mattharding.dev/demo/linesweeper/. --expect-commit,
// a full or abbreviated Git revision, fails the run unless the served
// BUILD.json names it. --out writes the record to a file as well as stdout.
//
// Run it after each deploy of the host; docs/port/web-to-be-tested.md, in
// "The live demo (M5)", says where its output is kept. It reads only; it
// needs Node and the network, not a checkout, a build or a browser.
//
// WHAT IT CHECKS, the server's half of docs/port/web.md, section 7.2:
//
// - the page: 200, text/html, Cache-Control no-cache, Brotli when Brotli is
//   offered; the folder its script names, m/<hash>/;
// - BUILD.json: a full commit, dirty false, a Release build, and the commit
//   asked for, if one was; SITE.json, where the host serves one, names the
//   same commit, and so does the page's "Built from Labrador" line;
// - the loader and the module, under m/<hash>/: text/javascript and
//   application/wasm, Cache-Control public, max-age=31536000, immutable, and
//   Brotli; the folder name is the first twelve hex digits of the SHA-256 of
//   the loader followed by the module, as the host's sync-demo names it;
// - the loader, the module, the poster and the notices: fetched with
//   Accept-Encoding: identity, each one's size and SHA-256 are BUILD.json's,
//   and each Brotli response decodes to the same bytes;
// - the poster, the notices and BUILD.json revalidate on every use
//   (max-age=0 or no-cache), so a new deploy is seen at once;
// - no Cross-Origin-Opener-Policy or Cross-Origin-Embedder-Policy, which a
//   single-threaded module does not need, and no Content-Security-Policy whose
//   script-src (or default-src) would refuse to compile the module.
//
// Every check runs, and each prints PASS or FAIL with what was served; any
// FAIL makes the exit status 1. A request that cannot be made at all is a
// FAIL of that check, not a crash.
//
// Node's own modules only, so there is nothing to install.

import { createHash } from 'node:crypto';
import { writeFileSync } from 'node:fs';
import http from 'node:http';
import https from 'node:https';
import { pathToFileURL } from 'node:url';
import { brotliDecompressSync, gunzipSync, inflateSync } from 'node:zlib';

const DEFAULT_URL = 'https://mattharding.dev/demo/linesweeper/';
const IMMUTABLE = 'public, max-age=31536000, immutable';
const SERVED = ['linesweeper.js', 'linesweeper.wasm', 'poster.png', 'NOTICE.txt'];
const TYPES = {
	'linesweeper.js': ['text/javascript', /^(text|application)\/javascript\b/],
	'linesweeper.wasm': ['application/wasm', /^application\/wasm$/],
	'poster.png': ['image/png', /^image\/png$/],
	'NOTICE.txt': ['text/plain', /^text\/plain\b/],
};

function parse_arguments(argv) {
	const parsed = { url: DEFAULT_URL };
	for (let i = 0; i < argv.length; i += 2) {
		const name = argv[i];
		const value = argv[i + 1];
		if (!['--url', '--expect-commit', '--out'].includes(name) || value === undefined) {
			process.stderr.write(`check_live: unexpected argument "${name}"\n` +
				'usage: node check_live.mjs [--url <page>] [--expect-commit <revision>] [--out <record.md>]\n');
			process.exit(2);
		}
		parsed[name.slice(2).replace('-', '_')] = value;
	}
	if (parsed.expect_commit !== undefined && !/^[0-9a-f]{7,40}$/.test(parsed.expect_commit)) {
		process.stderr.write('check_live: --expect-commit takes a Git revision in hex, 7 to 40 digits\n');
		process.exit(2);
	}
	return parsed;
}

// One GET, with the encoding offered, answering the status, the headers and
// the body as it crossed the wire and decoded. Redirects are not followed: the
// demo's addresses are its own, and a redirect is a finding.
function get(url, encoding) {
	const client = new URL(url).protocol === 'http:' ? http : https;
	return new Promise((answer, refuse) => {
		const request = client.get(url, { headers: {
			'Accept-Encoding': encoding,
			'User-Agent': 'labrador-check-live',
		} }, (response) => {
			const chunks = [];
			response.on('data', (chunk) => chunks.push(chunk));
			response.on('error', refuse);
			response.on('end', () => {
				const wire = Buffer.concat(chunks);
				const coding = (response.headers['content-encoding'] ?? 'identity').toLowerCase();
				let body;
				try {
					body = coding === 'br' ? brotliDecompressSync(wire)
						: coding === 'gzip' ? gunzipSync(wire)
							: coding === 'deflate' ? inflateSync(wire)
								: coding === 'identity' ? wire : null;
				} catch (error) {
					refuse(new Error(`${url}: its ${coding} body does not decode: ${error.message}`));
					return;
				}
				answer({ status: response.statusCode, headers: response.headers, wire, body, coding });
			});
		});
		request.setTimeout(30_000, () => request.destroy(new Error(`${url}: no answer within 30 s`)));
		request.on('error', refuse);
	});
}

const sha256 = (bytes) => createHash('sha256').update(bytes).digest('hex');

// Whether a Content-Security-Policy lets the page compile WebAssembly: its
// script-src, or default-src where it has none, must allow 'wasm-unsafe-eval'
// (or 'unsafe-eval'). A policy with neither directive restricts no script.
export function csp_allows_wasm(policy) {
	const directives = new Map(policy.split(';').map((part) => part.trim().split(/\s+/))
		.filter((words) => words[0]).map(([name, ...values]) => [name.toLowerCase(), values]));
	const sources = directives.get('script-src') ?? directives.get('default-src');
	return sources === undefined ||
		sources.some((source) => ["'wasm-unsafe-eval'", "'unsafe-eval'"].includes(source.toLowerCase()));
}

const revalidates = (value) => /(^|,\s*)no-cache\b|max-age=0\b/.test(value ?? '');

export async function check_live({ url, expect_commit }) {
	const results = [];
	const check = (name, passed, found) => {
		results.push({ name, passed, found });
	};
	// A check whose request fails outright is a failure of that check, and
	// the checks that needed its answer are not run.
	const attempt = async (name, work) => {
		try {
			return await work();
		} catch (error) {
			check(name, false, error.message);
			return undefined;
		}
	};
	const page_url = new URL(url);
	const at = (path) => new URL(path, page_url).href;

	const page = await attempt('page', () => get(page_url.href, 'br'));
	let folder;
	if (page) {
		const type = page.headers['content-type'] ?? '';
		check('page: 200, text/html', page.status === 200 && /^text\/html\b/.test(type),
			`${page.status}, ${type}`);
		check('page: Cache-Control no-cache', page.headers['cache-control'] === 'no-cache',
			page.headers['cache-control'] ?? 'none');
		check('page: Brotli when offered', page.coding === 'br', page.coding);
		const isolation = ['cross-origin-opener-policy', 'cross-origin-embedder-policy']
			.filter((name) => page.headers[name] !== undefined);
		check('page: no COOP or COEP', isolation.length === 0,
			isolation.length === 0 ? 'none' : isolation.map((name) => `${name}: ${page.headers[name]}`).join('; '));
		const policy = page.headers['content-security-policy'];
		check('page: Content-Security-Policy allows WebAssembly',
			policy === undefined || csp_allows_wasm(policy), policy ?? 'none');
		const named = /["'](\.\/)?m\/([0-9a-f]{12})\/linesweeper\.js["']/.exec(page.body?.toString('utf8') ?? '');
		folder = named?.[2];
		check('page: names its module folder m/<hash>/', folder !== undefined, folder ?? 'no m/<hash>/linesweeper.js');
	}

	const build_response = await attempt('BUILD.json', () => get(at('BUILD.json'), 'identity'));
	let build;
	if (build_response) {
		try {
			build = JSON.parse(build_response.body.toString('utf8'));
		} catch (error) {
			check('BUILD.json: parses', false, `${build_response.status}: ${error.message}`);
		}
		check('BUILD.json: revalidates', revalidates(build_response.headers['cache-control']),
			build_response.headers['cache-control'] ?? 'none');
	}
	if (build) {
		check('BUILD.json: a full commit, clean, Release',
			/^[0-9a-f]{40}$/.test(build.commit ?? '') && build.dirty === false && build.build_type === 'Release',
			`${build.commit}, dirty ${build.dirty}, ${build.build_type}, Emscripten ${build.emscripten}`);
		if (expect_commit !== undefined) {
			check(`BUILD.json: names ${expect_commit}`, (build.commit ?? '').startsWith(expect_commit), build.commit);
		}
		const site = await attempt('SITE.json', () => get(at('SITE.json'), 'identity'));
		if (site && site.status === 200) {
			let named;
			try { named = JSON.parse(site.body.toString('utf8')).commit; } catch { named = 'unreadable'; }
			check('SITE.json: the same commit', named === build.commit, named);
		}
		if (page) {
			const built = /Built from Labrador ([0-9a-f]{7,40})/.exec(page.body?.toString('utf8') ?? '');
			check('page: "Built from Labrador" names the same commit',
				built !== null && (build.commit ?? '').startsWith(built[1]), built?.[1] ?? 'no such line');
		}
	}

	const served = {};
	for (const name of SERVED) {
		const path = ['linesweeper.js', 'linesweeper.wasm'].includes(name)
			? (folder === undefined ? undefined : `m/${folder}/${name}`) : name;
		if (path === undefined) { continue; }
		const plain = await attempt(`${name}: identity`, () => get(at(path), 'identity'));
		const brotli = await attempt(`${name}: Brotli`, () => get(at(path), 'br'));
		if (!plain || !brotli) { continue; }
		served[name] = plain.body;
		const type = plain.headers['content-type'] ?? '';
		const cache = plain.headers['cache-control'] ?? 'none';
		const [label, pattern] = TYPES[name];
		check(`${name}: 200, ${label}`, plain.status === 200 && pattern.test(type), `${plain.status}, ${type}`);
		if (path.startsWith('m/')) {
			check(`${name}: Cache-Control immutable`, cache === IMMUTABLE, cache);
		} else {
			check(`${name}: revalidates`, revalidates(cache), cache);
		}
		if (name !== 'poster.png') {
			check(`${name}: Brotli when offered`, brotli.coding === 'br',
				`${brotli.coding}, ${brotli.wire.length} bytes on the wire`);
		}
		check(`${name}: the Brotli response decodes to the same bytes`, brotli.body?.equals(plain.body) === true,
			`${brotli.body?.length ?? 'undecodable'} against ${plain.body.length} bytes`);
		const record = build?.files?.[name];
		if (record) {
			const digest = sha256(plain.body);
			check(`${name}: size and SHA-256 are BUILD.json's`,
				plain.body.length === record.bytes && digest === record.sha256,
				`${plain.body.length} bytes, ${digest.slice(0, 12)}... (recorded ${record.bytes}, ${record.sha256.slice(0, 12)}...)`);
		} else if (build) {
			check(`${name}: recorded in BUILD.json`, false, 'no record');
		}
	}
	if (folder !== undefined && served['linesweeper.js'] && served['linesweeper.wasm']) {
		const hash = sha256(Buffer.concat([served['linesweeper.js'], served['linesweeper.wasm']])).slice(0, 12);
		check('m/<hash>/ is named by the loader and the module', hash === folder, `${folder}, computed ${hash}`);
	}
	return { results, commit: build?.commit, folder };
}

export function record(url, { results, commit, folder }, when = new Date()) {
	const failed = results.filter((result) => !result.passed).length;
	const lines = [
		`check_live.mjs, ${when.toISOString().replace(/\.\d+Z$/, 'Z')}: ${url}`,
		`served commit ${commit ?? 'unknown'}, module folder m/${folder ?? 'unknown'}/; ` +
			`${results.length - failed} of ${results.length} checks passed`,
		'',
		...results.map((result) => `${result.passed ? 'PASS' : 'FAIL'}  ${result.name}: ${result.found}`),
	];
	return `${lines.join('\n')}\n`;
}

if (process.argv[1] !== undefined && import.meta.url === pathToFileURL(process.argv[1]).href) {
	const options = parse_arguments(process.argv.slice(2));
	const outcome = await check_live(options);
	const text = record(options.url, outcome);
	process.stdout.write(text);
	if (options.out !== undefined) { writeFileSync(options.out, text); }
	process.exit(outcome.results.every((result) => result.passed) && outcome.results.length > 0 ? 0 : 1);
}
