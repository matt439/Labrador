// Release source verification is separate from the development site's optional
// archive identity override. A supplied commit cannot establish provenance.
import { execFileSync } from 'node:child_process';
import { existsSync, realpathSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// These folders supply generated pages, authored pages, quotations, media and
// toolchain pins. Even ignored additions here can change what the site says.
const inputPaths = [
	'.github', 'docs/design', 'engine', 'samples', 'tests',
	'website/src', 'website/public', 'website/scripts', 'website/tests',
	// A different config extension or a local .env at the root can change the
	// build too. glob's single star excludes generated subdirectories.
	':(top,glob)website/*',
];

export function releaseVersion(tag) {
	const match = /^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$/.exec(tag);
	if (!match || match[0] !== tag) {
		throw new Error('Use a stable release tag of the form vMAJOR.MINOR.PATCH.');
	}
	if (match[1] === '0' && BigInt(match[2]) < 3n) {
		throw new Error('Documentation snapshots start at v0.3.0; earlier releases are not backfilled.');
	}
	return tag.slice(1);
}

function gitEnvironment() {
	// -C must select the supplied checkout even inside a caller's Git hook or
	// alternate-index shell. Repository routing variables would override it.
	const environment = { ...process.env };
	for (const key of Object.keys(environment)) {
		if (/^GIT_/i.test(key)) delete environment[key];
	}
	// Local replacement objects must not change the contents of a release hash.
	// Disabling optional locks keeps status from refreshing the caller's index.
	environment.GIT_NO_REPLACE_OBJECTS = '1';
	environment.GIT_OPTIONAL_LOCKS = '0';
	return environment;
}

/**
 * Verify a candidate's source without writing files, building or publishing.
 * The returned record identifies source only; it is not a published-release
 * index entry or evidence that the engine/site qualification has passed.
 */
export function verifySnapshotSource(root, tag) {
	const version = releaseVersion(tag);
	if (process.env.LABRADOR_COMMIT !== undefined || process.env.LABRADOR_COMMIT_DATE !== undefined) {
		throw new Error('Unset LABRADOR_COMMIT and LABRADOR_COMMIT_DATE: release source must be verified with Git.');
	}
	const checkout = realpathSync(root);
	if (!existsSync(path.join(checkout, 'engine')) || !existsSync(path.join(checkout, 'website'))) {
		throw new Error('Release source must contain engine/ and website/ in one Labrador checkout.');
	}
	const environment = gitEnvironment();
	function git(...args) {
		try {
			return execFileSync('git', ['-C', checkout, ...args], {
				encoding: 'utf8', env: environment, stdio: ['ignore', 'pipe', 'pipe'],
			}).trimEnd();
		} catch {
			throw new Error(`Cannot verify release source: git ${args[0]} failed. Use a Git checkout with the release tag available.`);
		}
	}
	if (realpathSync(git('rev-parse', '--show-toplevel')) !== checkout) {
		throw new Error('Release source must be its own Git checkout, not an archive inside another repository.');
	}
	const commit = git('rev-parse', '--verify', 'HEAD^{commit}');
	if (!/^(?:[0-9a-f]{40}|[0-9a-f]{64})$/.test(commit)) {
		throw new Error('Git did not return a full source commit.');
	}
	const tagRef = `refs/tags/${tag}`;
	if (git('cat-file', '-t', tagRef) !== 'tag') {
		throw new Error('A release snapshot requires an annotated release tag, as CONTRIBUTING.md specifies.');
	}
	const tagHeader = git('cat-file', '-p', tagRef).split('\n\n', 1)[0];
	if (!tagHeader.split('\n').includes(`tag ${tag}`)) {
		throw new Error(`The annotated release tag object is not named ${tag}.`);
	}
	if (git('rev-parse', '--verify', `${tagRef}^{commit}`) !== commit) {
		throw new Error(`Release tag ${tag} does not name the checked-out HEAD commit.`);
	}
	if (git('-c', 'core.fsmonitor=false', 'status', '--porcelain=v1', '--untracked-files=all', '--ignore-submodules=none')) {
		throw new Error('Release source is dirty: commit or remove staged, unstaged and untracked changes throughout the checkout.');
	}
	// Hidden index flags can make status claim a modified source file is clean.
	const flagged = git('ls-files', '-v', '-z').split('\0').filter((entry) => entry && !entry.startsWith('H '));
	if (flagged.length) {
		throw new Error('Release source has hidden index flags or unresolved entries; use a full checkout without assume-unchanged or skip-worktree.');
	}
	if (git('ls-files', '--others', '--ignored', '--exclude-standard', '-z', '--', ...inputPaths)) {
		throw new Error('Release source contains ignored files in website input folders; remove them before verifying.');
	}
	// A source symlink or gitlink can read bytes outside the named revision.
	const entries = git('ls-files', '--stage', '-z').split('\0');
	if (entries.some((entry) => /^(120000|160000) /.test(entry))) {
		throw new Error('Release website inputs must be ordinary tracked files, without symlinks or submodules.');
	}
	// Reading these from the verified commit also requires them to be tracked.
	const manifest = JSON.parse(git('show', `${commit}:vcpkg.json`));
	if (manifest['version-string'] !== version) {
		throw new Error(`Release tag ${tag} disagrees with vcpkg.json version-string.`);
	}
	git('cat-file', '-e', `${commit}:website/package-lock.json`);
	return {
		schemaVersion: 1,
		tag,
		commit,
		sourceDate: git('show', '-s', '--format=%cs', commit),
		snapshotPath: `/versions/${tag}/`,
	};
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		if (process.argv.length !== 3) {
			throw new Error('Usage: npm run snapshot:source -- vMAJOR.MINOR.PATCH');
		}
		const root = fileURLToPath(new URL('../../', import.meta.url));
		process.stdout.write(`${JSON.stringify(verifySnapshotSource(root, process.argv[2]), null, 2)}\n`);
	} catch (error) {
		process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
		process.exitCode = 1;
	}
}
