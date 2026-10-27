import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { verifySnapshotSource } from '../scripts/snapshot-source.mjs';

const SOURCE_DATE = '2026-10-20';
const COMMIT_ENV = {
	...process.env,
	GIT_AUTHOR_DATE: `${SOURCE_DATE}T12:00:00+00:00`,
	GIT_COMMITTER_DATE: `${SOURCE_DATE}T12:00:00+00:00`,
};

function git(root, ...args) {
	return execFileSync('git', args, {
		cwd: root,
		encoding: 'utf8',
		env: COMMIT_ENV,
		stdio: ['ignore', 'pipe', 'pipe'],
	}).trim();
}

function write(root, file, contents) {
	const target = path.join(root, file);
	mkdirSync(path.dirname(target), { recursive: true });
	writeFileSync(target, contents);
}

function temporaryDirectory(t) {
	const root = mkdtempSync(path.join(tmpdir(), 'labrador-snapshot-'));
	t.after(() => {
		const target = realpathSync(root);
		assert.equal(path.dirname(target), realpathSync(tmpdir()), 'cleanup stays directly inside the temporary directory');
		assert.ok(path.basename(target).startsWith('labrador-snapshot-'), 'cleanup names a snapshot test fixture');
		rmSync(target, { recursive: true, force: true });
	});
	return root;
}

function checkout(t, { tag = 'v0.3.0', version = tag.slice(1), annotated = true, lockfile = true, cli = false } = {}) {
	const root = temporaryDirectory(t);
	git(root, 'init', '--quiet');
	git(root, 'config', 'user.name', 'Snapshot fixture');
	git(root, 'config', 'user.email', 'snapshot@example.invalid');
	git(root, 'config', 'core.autocrlf', 'false');
	write(root, '.gitignore', 'out/\nwebsite/node_modules/\nwebsite/dist/\n');
	write(root, 'vcpkg.json', JSON.stringify({ name: 'labrador', 'version-string': version }) + '\n');
	write(root, 'engine/core/example.h', '#pragma once\n');
	write(root, 'website/src/content/docs/docs/index.mdx', '# Documentation\n');
	if (lockfile) write(root, 'website/package-lock.json', '{"lockfileVersion":3}\n');
	if (cli) write(root, 'website/scripts/snapshot-source.mjs', readFileSync(new URL('../scripts/snapshot-source.mjs', import.meta.url)));
	git(root, 'add', '.');
	git(root, 'commit', '--quiet', '-m', 'Record snapshot fixture');
	if (annotated) git(root, 'tag', '-a', tag, '-m', `Release ${tag}`);
	else git(root, 'tag', tag);
	return { root, tag, commit: git(root, 'rev-parse', 'HEAD') };
}

test('a clean release checkout produces the exact commit-bound snapshot record', (t) => {
	const { root, tag, commit } = checkout(t);
	assert.deepEqual(verifySnapshotSource(root, tag), {
		schemaVersion: 1,
		tag,
		commit,
		sourceDate: SOURCE_DATE,
		snapshotPath: '/versions/v0.3.0/',
	});
});

test('annotated release tags resolve to their commit rather than their tag object', (t) => {
	const { root, tag, commit } = checkout(t, { annotated: true });
	assert.notEqual(git(root, 'rev-parse', `refs/tags/${tag}`), commit);
	assert.equal(verifySnapshotSource(root, tag).commit, commit);
});

test('lightweight tags do not satisfy the annotated release procedure', (t) => {
	const { root, tag } = checkout(t, { annotated: false });
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('an annotated tag object must carry the same release name as its ref', (t) => {
	const { root, tag } = checkout(t);
	git(root, 'tag', '-a', 'v0.4.0', '-m', 'Different release name');
	git(root, 'update-ref', `refs/tags/${tag}`, git(root, 'rev-parse', 'refs/tags/v0.4.0'));
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('later minor and major releases remain eligible for documentation snapshots', (t) => {
	for (const tag of ['v0.10.0', 'v1.0.0']) {
		const { root, commit } = checkout(t, { tag });
		assert.equal(verifySnapshotSource(root, tag).commit, commit);
		assert.equal(verifySnapshotSource(root, tag).snapshotPath, `/versions/${tag}/`);
	}
});

test('tags before the first snapshotted release cannot be backfilled', (t) => {
	for (const tag of ['v0.0.99', 'v0.1.0', 'v0.2.0', 'v0.2.99']) {
		const { root } = checkout(t, { tag });
		assert.throws(() => verifySnapshotSource(root, tag), Error, tag);
	}
});

test('only canonical stable release tags are accepted', (t) => {
	const { root } = checkout(t);
	for (const tag of [
		'', '0.3.0', 'v0.3', 'v0.3.0.0', 'v00.3.0', 'v0.03.0', 'v0.3.00',
		'v0.3.0-rc.1', 'v0.3.0+build', 'v0.3.0/path', '../v0.3.0', '--help',
		' v0.3.0', 'v0.3.0 ', 'v0.3.0\n',
	]) {
		assert.throws(() => verifySnapshotSource(root, tag), Error, JSON.stringify(tag));
	}
});

test('a matching branch name does not substitute for a local release tag', (t) => {
	const { root, tag } = checkout(t);
	git(root, 'tag', '-d', tag);
	git(root, 'branch', tag);
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('the release tag must identify the checked-out HEAD', (t) => {
	const { root, tag } = checkout(t);
	write(root, 'engine/core/example.h', '#pragma once\n// Later development\n');
	git(root, 'add', '.');
	git(root, 'commit', '--quiet', '-m', 'Move beyond release');
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('unstaged engine changes prevent a snapshot even when website sources are clean', (t) => {
	const { root, tag } = checkout(t);
	write(root, 'engine/core/example.h', '#pragma once\n// Local change\n');
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('staged changes prevent a snapshot', (t) => {
	const { root, tag } = checkout(t);
	write(root, 'website/src/content/docs/docs/index.mdx', '# Changed documentation\n');
	git(root, 'add', '.');
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('untracked files outside the website also prevent a snapshot', (t) => {
	const { root, tag } = checkout(t);
	write(root, 'samples/new-sample.cpp', 'void new_sample() {}\n');
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('ignored build outputs do not make a clean source checkout ineligible', (t) => {
	const { root, tag, commit } = checkout(t);
	write(root, 'out/build/generated.txt', 'Generated engine output\n');
	write(root, 'website/node_modules/generated.txt', 'Installed dependency\n');
	write(root, 'website/dist/index.html', '<p>Generated site</p>\n');
	assert.equal(verifySnapshotSource(root, tag).commit, commit);
});

test('ignored source inputs cannot silently enter a frozen snapshot', (t) => {
	const { root, tag } = checkout(t);
	const folders = ['.github', 'docs/design', 'engine', 'samples', 'tests', 'website/src', 'website/public', 'website/scripts', 'website/tests'];
	for (const folder of folders) {
		const source = `${folder}/ignored-snapshot-input.txt`;
		write(root, '.git/info/exclude', `${source}\n`);
		write(root, source, 'Uncommitted snapshot input\n');
		assert.equal(git(root, 'status', '--porcelain'), '', source);
		assert.throws(() => verifySnapshotSource(root, tag), Error, source);
		rmSync(path.join(root, source));
	}
});

test('ignored root configuration cannot override the tracked website build', (t) => {
	const { root, tag } = checkout(t);
	write(root, 'website/astro.config.ts', 'export default {};\n');
	git(root, 'add', '.');
	git(root, 'commit', '--quiet', '-m', 'Record website configuration');
	git(root, 'tag', '-f', '-a', tag, '-m', `Release ${tag}`);
	for (const source of ['website/astro.config.mjs', 'website/.env', 'website/.env.production', 'website/.npmrc']) {
		write(root, '.git/info/exclude', `${source}\n`);
		write(root, source, source.endsWith('.mjs') ? 'export default { base: "/different/" };\n' : 'SNAPSHOT_OVERRIDE=untracked\n');
		assert.equal(git(root, 'status', '--porcelain'), '', source);
		assert.throws(() => verifySnapshotSource(root, tag), Error, source);
		rmSync(path.join(root, source));
	}
});

test('index flags cannot conceal dirty tracked release inputs', (t) => {
	for (const flag of ['--assume-unchanged', '--skip-worktree']) {
		const { root, tag } = checkout(t);
		git(root, 'update-index', flag, 'engine/core/example.h');
		write(root, 'engine/core/example.h', '#pragma once\n// Hidden local change\n');
		assert.equal(git(root, 'status', '--porcelain'), '', flag);
		assert.throws(() => verifySnapshotSource(root, tag), Error, flag);
	}
});

test('a clean linked worktree can verify its own release checkout', (t) => {
	const { root, tag, commit } = checkout(t);
	const worktree = path.join(temporaryDirectory(t), 'release-worktree');
	git(root, 'worktree', 'add', '--quiet', '--detach', worktree, tag);
	assert.equal(verifySnapshotSource(worktree, tag).commit, commit);
});

test('Git routing environment cannot substitute another repository for the requested checkout', (t) => {
	const { root, tag, commit } = checkout(t);
	const other = checkout(t, { tag: 'v1.0.0' });
	const overrides = {
		GIT_DIR: path.join(other.root, '.git'),
		GIT_WORK_TREE: other.root,
		GIT_INDEX_FILE: path.join(other.root, '.git', 'index'),
	};
	const previous = new Map(Object.keys(overrides).map((key) => [key, process.env[key]]));
	t.after(() => {
		for (const [key, value] of previous) {
			if (value === undefined) delete process.env[key];
			else process.env[key] = value;
		}
	});
	Object.assign(process.env, overrides);
	assert.equal(verifySnapshotSource(root, tag).commit, commit);
});

test('Git replacement objects cannot attribute different source bytes to a release commit', (t) => {
	const { root, tag, commit } = checkout(t);
	write(root, 'engine/core/example.h', '#pragma once\n// Replacement source bytes\n');
	git(root, 'add', '.');
	git(root, 'commit', '--quiet', '-m', 'Different source tree');
	const replacement = git(root, 'rev-parse', 'HEAD');
	git(root, 'replace', commit, replacement);
	git(root, 'reset', '--hard', commit);
	assert.equal(git(root, 'rev-parse', 'HEAD'), commit);
	assert.equal(git(root, 'status', '--porcelain'), '', 'normal Git status trusts the replacement tree');
	assert.match(readFileSync(path.join(root, 'engine/core/example.h'), 'utf8'), /Replacement source bytes/);
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('a tracked lockfile symlink cannot read dependency identity outside the release tree', (t) => {
	const { root, tag } = checkout(t);
	// core.symlinks=false represents a tracked symlink as its target text on
	// Windows too, so this fixture needs no OS symlink privilege.
	git(root, 'config', 'core.symlinks', 'false');
	write(root, 'website/package-lock.json', '../../external-lockfile.json');
	const blob = git(root, 'hash-object', '-w', 'website/package-lock.json');
	git(root, 'update-index', '--cacheinfo', `120000,${blob},website/package-lock.json`);
	git(root, 'commit', '--quiet', '-m', 'Record external lockfile link');
	git(root, 'tag', '-f', '-a', tag, '-m', `Release ${tag}`);
	assert.equal(git(root, 'status', '--porcelain'), '');
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('tracked gitlinks cannot supply files outside the frozen source revision', (t) => {
	const { root, tag, commit } = checkout(t);
	mkdirSync(path.join(root, 'external-input'));
	git(root, 'update-index', '--add', '--cacheinfo', `160000,${commit},external-input`);
	git(root, 'commit', '--quiet', '-m', 'Record external source gitlink');
	git(root, 'tag', '-f', '-a', tag, '-m', `Release ${tag}`);
	assert.equal(git(root, 'status', '--porcelain'), '');
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('the tagged package version must agree with the release tag', (t) => {
	const { root, tag } = checkout(t, { version: '0.4.0' });
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('a missing tracked dependency lockfile prevents a snapshot', (t) => {
	const { root, tag } = checkout(t, { lockfile: false });
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('an ignored untracked lockfile cannot supply release dependency identity', (t) => {
	const { root, tag } = checkout(t, { lockfile: false });
	write(root, '.git/info/exclude', 'website/package-lock.json\n');
	write(root, 'website/package-lock.json', '{"lockfileVersion":3}\n');
	assert.equal(git(root, 'status', '--porcelain'), '');
	assert.throws(() => verifySnapshotSource(root, tag), Error);
});

test('a source archive cannot borrow a containing repository release identity', (t) => {
	const { root, tag } = checkout(t);
	const archive = path.join(root, 'out', 'source-archive');
	write(archive, 'vcpkg.json', '{"name":"labrador","version-string":"0.3.0"}\n');
	write(archive, 'engine/core/example.h', '#pragma once\n');
	write(archive, 'website/package-lock.json', '{"lockfileVersion":3}\n');
	write(archive, 'website/src/content/docs/docs/index.mdx', '# Archived documentation\n');
	assert.equal(git(archive, 'rev-parse', 'HEAD'), git(root, 'rev-parse', 'HEAD'));
	assert.throws(() => verifySnapshotSource(archive, tag), Error);
});

test('the CLI prints only the verified source record and leaves the checkout untouched', (t) => {
	const { root, tag } = checkout(t, { cli: true });
	const before = git(root, 'status', '--porcelain');
	const result = spawnSync(process.execPath, ['scripts/snapshot-source.mjs', tag], {
		cwd: path.join(root, 'website'), encoding: 'utf8', env: COMMIT_ENV,
	});
	assert.equal(result.status, 0, result.stderr);
	assert.equal(result.stderr, '');
	assert.deepEqual(JSON.parse(result.stdout), verifySnapshotSource(root, tag));
	assert.equal(git(root, 'status', '--porcelain'), before);
});

test('the CLI refuses dirty source with failure status, stderr and no JSON record', (t) => {
	const { root, tag } = checkout(t, { cli: true });
	write(root, 'engine/core/example.h', '#pragma once\n// Dirty source\n');
	const before = git(root, 'status', '--porcelain');
	const result = spawnSync(process.execPath, ['scripts/snapshot-source.mjs', tag], {
		cwd: path.join(root, 'website'), encoding: 'utf8', env: COMMIT_ENV,
	});
	assert.equal(result.status, 1);
	assert.equal(result.stdout, '');
	assert.match(result.stderr, /dirty/i);
	assert.equal(git(root, 'status', '--porcelain'), before);
});

test('the CLI rejects missing and surplus arguments without emitting a record', (t) => {
	const { root, tag } = checkout(t, { cli: true });
	for (const args of [[], [tag, 'unexpected']]) {
		const result = spawnSync(process.execPath, ['scripts/snapshot-source.mjs', ...args], {
			cwd: path.join(root, 'website'), encoding: 'utf8', env: COMMIT_ENV,
		});
		assert.equal(result.status, 1);
		assert.equal(result.stdout, '');
		assert.match(result.stderr, /^Usage:/);
	}
	assert.equal(git(root, 'status', '--porcelain'), '');
});

test('archive commit environment overrides are refused for release verification', (t) => {
	const { root, tag } = checkout(t);
	const oldCommit = process.env.LABRADOR_COMMIT;
	const oldDate = process.env.LABRADOR_COMMIT_DATE;
	t.after(() => {
		if (oldCommit === undefined) delete process.env.LABRADOR_COMMIT;
		else process.env.LABRADOR_COMMIT = oldCommit;
		if (oldDate === undefined) delete process.env.LABRADOR_COMMIT_DATE;
		else process.env.LABRADOR_COMMIT_DATE = oldDate;
	});
	process.env.LABRADOR_COMMIT = 'f'.repeat(40);
	process.env.LABRADOR_COMMIT_DATE = '1999-01-01';
	assert.throws(() => verifySnapshotSource(root, tag), Error);
	delete process.env.LABRADOR_COMMIT;
	assert.throws(() => verifySnapshotSource(root, tag), Error,
		'the date-only archive override is also refused');
	process.env.LABRADOR_COMMIT = 'f'.repeat(40);
	delete process.env.LABRADOR_COMMIT_DATE;
	assert.throws(() => verifySnapshotSource(root, tag), Error,
		'the commit-only archive override is also refused');

	const archive = temporaryDirectory(t);
	write(archive, 'vcpkg.json', '{"name":"labrador","version-string":"0.3.0"}\n');
	write(archive, 'engine/core/example.h', '#pragma once\n');
	write(archive, 'website/package-lock.json', '{"lockfileVersion":3}\n');
	assert.throws(() => verifySnapshotSource(archive, tag), Error);
});
