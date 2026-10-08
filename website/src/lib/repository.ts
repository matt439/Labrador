// The checkout the site is built from: where it is, which revision it is at,
// and how to point a reader at a file in it.
//
// Every page that shows engine source, a design document or a header reads it
// from this checkout at build time, so the revision below is the revision of
// everything the site says. A build that cannot name its revision fails rather
// than publishing pages that cannot say what they describe (PHILOSOPHY T6).

import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync, statSync } from 'node:fs';
import path from 'node:path';

export const GITHUB_REPOSITORY = 'https://github.com/matt439/Labrador';

// Edits go to the branch, never to a commit: GitHub cannot open an editor on a
// revision that is not a branch head.
export const EDIT_BRANCH = 'master';

// npm runs scripts from the package directory, so the build always starts in
// website/. Resolved from the working directory rather than from this module's
// URL because Astro bundles this file before running it, and a bundled
// module's URL is wherever the bundle landed.
function findRepositoryRoot(): string {
	const root = path.resolve(process.cwd(), '..');
	if (!existsSync(path.join(root, 'engine')) || !existsSync(path.join(root, 'website'))) {
		throw new Error(
			`The website must be built from website/ inside a Labrador checkout; ` +
				`'${root}' has no engine/ and website/ beside each other.`
		);
	}
	return root;
}

export const REPOSITORY_ROOT = findRepositoryRoot();

export interface Revision {
	commit: string;
	short: string;
	// The committer date, as YYYY-MM-DD.
	date: string;
	// Whether the checkout differs from the commit, including authored pages
	// and the website's generator. A preview with local changes says so because
	// the commit alone no longer describes what the pages show.
	modified: boolean;
}

function git(...args: string[]): string {
	return execFileSync('git', args, { cwd: REPOSITORY_ROOT, encoding: 'utf8' }).trim();
}

function readRevision(): Revision {
	// A host that builds from an archive rather than a clone can supply the
	// commit itself.
	const supplied = process.env.LABRADOR_COMMIT;
	if (supplied) {
		return {
			commit: supplied,
			short: supplied.slice(0, 7),
			date: process.env.LABRADOR_COMMIT_DATE ?? 'unknown date',
			modified: false,
		};
	}

	let commit: string;
	try {
		commit = git('rev-parse', 'HEAD');
	} catch (error) {
		throw new Error(
			'Could not read the Labrador revision with `git rev-parse HEAD`. Build from a git ' +
				'checkout, or set LABRADOR_COMMIT (and optionally LABRADOR_COMMIT_DATE).\n' +
				String(error)
		);
	}

	return {
		commit,
		short: commit.slice(0, 7),
		date: git('show', '-s', '--format=%cs', commit),
		modified: git('status', '--porcelain').length > 0,
	};
}

export const REVISION: Revision = readRevision();

// Repository paths are written with forward slashes and from the root, exactly
// as an #include is written (CONVENTIONS, Files).
export function toRepositoryPath(absolute: string): string {
	return path.relative(REPOSITORY_ROOT, absolute).split(path.sep).join('/');
}

export function absolutePath(repositoryPath: string): string {
	return path.join(REPOSITORY_ROOT, ...repositoryPath.split('/'));
}

export function readRepositoryFile(repositoryPath: string): string {
	const file = absolutePath(repositoryPath);
	if (!existsSync(file)) {
		throw new Error(`The site cites '${repositoryPath}', and the checkout has no such file.`);
	}
	// Normalised to \n so that excerpts and line numbers do not depend on how
	// git checked the file out.
	return readFileSync(file, 'utf8').replace(/\r\n?/g, '\n');
}

export function isDirectory(repositoryPath: string): boolean {
	const file = absolutePath(repositoryPath);
	return existsSync(file) && statSync(file).isDirectory();
}

// A link to a file or folder as it was at the revision the site was built
// from, so that a page and the source it quotes can never disagree.
export function sourceUrl(repositoryPath: string, lines?: { start: number; end: number }): string {
	const kind = isDirectory(repositoryPath) ? 'tree' : 'blob';
	const anchor = !lines ? '' : lines.start === lines.end ? `#L${lines.start}` : `#L${lines.start}-L${lines.end}`;
	return `${GITHUB_REPOSITORY}/${kind}/${REVISION.commit}/${repositoryPath}${anchor}`;
}

export function editUrl(repositoryPath: string): string {
	return `${GITHUB_REPOSITORY}/edit/${EDIT_BRANCH}/${repositoryPath}`;
}

export function commitUrl(): string {
	return `${GITHUB_REPOSITORY}/commit/${REVISION.commit}`;
}
