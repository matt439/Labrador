// The published releases the development site lists on /versions/, read from
// website/published-versions.json when the page is built.
//
// Only development builds read the manifest. A release snapshot never lists
// other versions itself; its selector asks the development site at run time.
// So the file is read when the page asks for it, not when this module loads,
// and a snapshot built at a tag whose manifest says anything never reads it.

import { readFileSync } from 'node:fs';
import { absolutePath } from './repository';
import { assetUrl, compareTags, validateManifest } from '../../scripts/published-versions.mjs';

const MANIFEST = 'website/published-versions.json';

export interface Erratum {
	tag: string;
	date: string;
	title: string;
	href: string;
}

export interface PublishedRelease {
	tag: string;
	commit: string;
	sourceDate: string;
	path: string;
	asset: string;
	bytes: number;
	sha256: string;
	published: string;
	assetUrl: string;
	latest: boolean;
	// In the order the manifest appended them.
	errata: Erratum[];
}

interface Manifest {
	releases: Omit<PublishedRelease, 'assetUrl' | 'latest' | 'errata'>[];
	errata: Erratum[];
}

/** Every published release, newest first by version, whatever order they were published in. */
export function publishedReleases(): PublishedRelease[] {
	// Not through readRepositoryFile, which normalises line endings: the
	// manifest's exact text is part of what validation checks.
	const manifest = validateManifest(readFileSync(absolutePath(MANIFEST), 'utf8')) as Manifest;
	const releases = [...manifest.releases].sort((a, b) => compareTags(b.tag, a.tag));
	return releases.map((release, index) => ({
		...release,
		assetUrl: assetUrl(release),
		latest: index === 0,
		errata: manifest.errata.filter((erratum) => erratum.tag === release.tag),
	}));
}
