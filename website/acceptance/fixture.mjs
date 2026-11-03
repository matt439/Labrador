// Build the two-release fixture that the website acceptance suite runs against.
// A private clone of the checkout's HEAD is tagged twice: v0.3.0, then v0.4.0
// with one extra guide that v0.3.0 lacks. Each tag is built and packed by the
// release commands themselves, a fixture manifest lists both with one erratum,
// and a development build assembles them from a local asset cache. The result
// exercises the version selector, the release index and the arrival notice
// with real snapshots, without a network or a published release.
//
// Everything is written under the output directory: the clone (repo/), the
// asset cache (assets/) and one log per step (logs/). The caller's checkout is
// only read by git clone. The fixture's tags, commits and manifest are
// synthetic and must never be pushed or published.
//
// Usage: node acceptance/fixture.mjs <out-dir> [--force] [--repo <checkout>]
// prints the development site's dist path on stdout; progress goes to stderr.
import { spawnSync } from 'node:child_process';
import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { canonicalize } from '../scripts/published-versions.mjs';

export const FIXTURE_TAGS = ['v0.3.0', 'v0.4.0'];
export const FIXTURE_ONLY_PATH = '/docs/guides/fixture-only/';
export const FIXTURE_ONLY_HEADING = 'a-heading-to-switch-from';

const FIXTURE_ONLY_FILE = 'website/src/content/docs/docs/guides/fixture-only.mdx';
const FIXTURE_ONLY_PAGE = `---
title: Fixture-only guide
description: A page that exists in the acceptance fixture's v0.4.0 and Development but not in its v0.3.0, for checking the version selector.
---

This page exists only in the website acceptance fixture, which
\`website/acceptance/fixture.mjs\` builds; it is never part of a real release.
It is present in the fixture's \`v0.4.0\` and in Development and absent from
its \`v0.3.0\`, so the acceptance suite can check what the version selector
does with a page that one version lacks.

Switching to \`v0.3.0\` from here should open that version's overview, with
the selector's link saying so in its visible text, and the overview should
show an arrival notice naming this page. The fixture's manifest lists an
erratum for \`v0.3.0\` that links here, so the release index shows one too.

## A heading to switch from

Switching between Development and \`v0.4.0\` from this heading should keep
both the page and this heading's anchor.
`;

// Each step's limit, so that a stalled registry fetch or a hung build fails
// that step, naming its log, instead of holding a CI job until its own limit.
const GIT_TIMEOUT_MS = 2 * 60_000;
const STEP_TIMEOUT_MS = 15 * 60_000;

const EMPTY_MANIFEST = '{\n  "schemaVersion": 1,\n  "releases": [],\n  "errata": []\n}\n';

// Variables that would make release verification refuse the source or route
// Git somewhere other than the fixture clone.
function childEnvironment(extra = {}) {
	const environment = { ...process.env };
	for (const key of Object.keys(environment)) {
		if (/^GIT_/i.test(key) || ['LABRADOR_COMMIT', 'LABRADOR_COMMIT_DATE', 'LABRADOR_DOCS_TAG', 'LABRADOR_SNAPSHOT_CACHE'].includes(key)) {
			delete environment[key];
		}
	}
	return { ...environment, ...extra };
}

function isInside(child, parent) {
	const relative = path.relative(parent, child);
	return relative === '' || (!relative.startsWith('..') && !path.isAbsolute(relative));
}

/**
 * Build the fixture from repo's HEAD into outDir and return its paths and
 * manifest. Throws with the failing step's log path when a command fails.
 */
export function buildFixture({
	repo = fileURLToPath(new URL('../../', import.meta.url)),
	outDir,
	force = false,
	log = (message) => process.stderr.write(`${message}\n`),
} = {}) {
	if (!outDir) throw new Error('buildFixture: expected an output directory.');
	const source = realpathSync(repo);
	const out = path.resolve(outDir);
	// Neither tree may contain the other: the clone must not land in the
	// caller's checkout, and --force must never delete it.
	if (isInside(out, source) || isInside(source, out)) {
		throw new Error(`${out}: expected an output directory outside the checkout ${source}.`);
	}
	if (existsSync(out) && readdirSync(out).length) {
		if (!force) throw new Error(`${out}: expected an empty or missing output directory; pass --force to replace its contents.`);
		rmSync(out, { recursive: true, force: true, maxRetries: 5 });
	}
	const clone = path.join(out, 'repo');
	const website = path.join(clone, 'website');
	const assets = path.join(out, 'assets');
	const logs = path.join(out, 'logs');
	mkdirSync(assets, { recursive: true });
	mkdirSync(logs, { recursive: true });

	let stepNumber = 0;
	// Runs one command, keeps its whole output in logs/, and returns stdout.
	function run(name, command, args, { cwd = clone, env = childEnvironment(), shell = false, timeoutMs = STEP_TIMEOUT_MS } = {}) {
		stepNumber += 1;
		const logFile = path.join(logs, `${String(stepNumber).padStart(2, '0')}-${name}.log`);
		log(`fixture: ${name}`);
		const result = spawnSync(command, args, {
			cwd, env, shell, encoding: 'utf8', maxBuffer: 256 * 1024 * 1024, stdio: ['ignore', 'pipe', 'pipe'],
			timeout: timeoutMs, killSignal: 'SIGKILL',
		});
		const shown = shell ? command : [command, ...args].join(' ');
		writeFileSync(logFile, `$ ${shown}\n(cwd ${cwd})\n\n--- stdout ---\n${result.stdout ?? ''}\n--- stderr ---\n${result.stderr ?? ''}\n--- exit ${result.error?.message ?? result.signal ?? result.status} ---\n`);
		if (result.error?.code === 'ETIMEDOUT') {
			throw new Error(`fixture: ${name} did not finish within ${timeoutMs / 60_000} minutes and was stopped; see ${logFile}`);
		}
		if (result.error || result.status !== 0) {
			const tail = `${result.stderr ?? ''}${result.stdout ?? ''}`.trimEnd().split('\n').slice(-15).join('\n');
			throw new Error(`fixture: ${name} failed (${result.error?.message ?? result.signal ?? `exit ${result.status}`}); see ${logFile}\n${tail}`);
		}
		return result.stdout;
	}
	const git = (name, ...args) => run(name, 'git', args, { timeoutMs: GIT_TIMEOUT_MS }).trim();
	// npm is a batch file on Windows, which only a shell can start. The
	// command lines are fixed text, so the shell sees no caller input.
	const npm = (name, commandLine, env) => run(name, `npm ${commandLine}`, [], { cwd: website, env, shell: true });
	const node = (name, script, ...args) => run(name, process.execPath, [script, ...args], { cwd: website });

	const head = run('resolve-head', 'git', ['-C', source, 'rev-parse', '--verify', 'HEAD^{commit}'], {
		cwd: source, env: childEnvironment({ GIT_OPTIONAL_LOCKS: '0' }), timeoutMs: GIT_TIMEOUT_MS,
	}).trim();
	// Long paths: Windows refuses some of the site's generated paths otherwise.
	// No tags: the checkout's own release tags must not collide with the fixture's.
	run('clone', 'git', ['-c', 'core.longpaths=true', 'clone', '--quiet', '--no-hardlinks', '--no-tags', '--no-checkout', source, clone], { cwd: out });
	git('configure', 'config', 'core.longpaths', 'true');
	for (const [key, value] of [
		['user.name', 'Labrador acceptance fixture'],
		['user.email', 'fixture@localhost'],
		['commit.gpgsign', 'false'],
		['tag.gpgsign', 'false'],
		// Text files keep the bytes they have in the commit on every platform.
		['core.autocrlf', 'false'],
	]) {
		git('configure', 'config', key, value);
	}
	git('checkout', 'checkout', '--quiet', '--detach', head);

	function setVersion(version) {
		const file = path.join(clone, 'vcpkg.json');
		const text = readFileSync(file, 'utf8');
		const pattern = /("version-string"\s*:\s*")[^"]*(")/g;
		if ((text.match(pattern) ?? []).length !== 1) throw new Error(`${file}: expected one "version-string", found ${(text.match(pattern) ?? []).length}.`);
		writeFileSync(file, text.replace(pattern, `$1${version}$2`));
	}

	const entries = [];
	function release(tag, message) {
		setVersion(tag.slice(1));
		git(`commit-${tag}`, 'add', '--all');
		git(`commit-${tag}`, 'commit', '--quiet', '-m', message);
		git(`tag-${tag}`, 'tag', '-a', tag, '-m', `Acceptance fixture ${tag}`);
		node(`snapshot-build-${tag}`, 'scripts/build-snapshot.mjs', tag);
		const printed = node(`snapshot-pack-${tag}`, 'scripts/pack-snapshot.mjs', tag);
		const entry = JSON.parse(printed);
		copyFileSync(path.join(website, 'release-assets', entry.asset), path.join(assets, entry.asset));
		entries.push(entry);
	}

	// The fixture's releases are the only ones its development build lists,
	// whatever the checkout's own manifest says.
	writeFileSync(path.join(website, 'published-versions.json'), EMPTY_MANIFEST);
	npm('npm-ci', 'ci --no-audit --no-fund', childEnvironment());
	release('v0.3.0', 'Acceptance fixture: version 0.3.0');
	const page = path.join(clone, FIXTURE_ONLY_FILE);
	mkdirSync(path.dirname(page), { recursive: true });
	writeFileSync(page, FIXTURE_ONLY_PAGE);
	release('v0.4.0', 'Acceptance fixture: version 0.4.0 with one extra guide');

	const manifest = {
		schemaVersion: 1,
		releases: entries,
		errata: [{
			tag: 'v0.3.0',
			date: new Date().toISOString().slice(0, 10),
			title: 'Fixture erratum: v0.3.0 lacks the fixture-only guide',
			href: FIXTURE_ONLY_PATH,
		}],
	};
	writeFileSync(path.join(website, 'published-versions.json'), canonicalize(manifest));
	npm('build', 'run build', childEnvironment({ LABRADOR_SNAPSHOT_CACHE: assets }));

	const dist = path.join(website, 'dist');
	for (const required of ['versions/index.json', 'versions/v0.3.0/index.html', 'versions/v0.4.0/index.html']) {
		if (!existsSync(path.join(dist, required))) throw new Error(`fixture: expected dist/${required} after the build, found nothing.`);
	}
	log(`fixture: built ${dist}`);
	return { dist, clone, assets, logs, manifest, commit: head };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		const args = process.argv.slice(2);
		let force = false;
		let repo;
		const positional = [];
		for (let index = 0; index < args.length; index += 1) {
			if (args[index] === '--force') force = true;
			else if (args[index] === '--repo' && index + 1 < args.length) repo = args[++index];
			else if (args[index].startsWith('--')) throw new Error(`Unknown option ${args[index]}.`);
			else positional.push(args[index]);
		}
		if (positional.length !== 1) throw new Error('Usage: node acceptance/fixture.mjs <out-dir> [--force] [--repo <checkout>]');
		const { dist } = buildFixture({ repo, outDir: positional[0], force });
		process.stdout.write(`${dist}\n`);
	} catch (error) {
		process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
		process.exitCode = 1;
	}
}
