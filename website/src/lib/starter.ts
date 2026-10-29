// The release whose starter ZIP Get Started sends a reader to download.
//
// tools/consumer_check/starter_zip.py makes the ZIP, and CONTRIBUTING.md's
// release procedure attaches it to every release from v0.3.0, the first that
// also has a documentation snapshot. So a release's own snapshot names its
// own tag, and the development site names the newest release whose snapshot
// is published. Before any is, there is no release known to carry a ZIP, and
// the page says how to tell whether the newest release has one.

import { RELEASE_SOURCE } from './repository';
import { publishedReleases } from './versions';

export const REPOSITORY_URL = 'https://github.com/matt439/Labrador';

export interface Starter {
	tag: string;
	asset: string;
	downloadUrl: string;
	releaseUrl: string;
}

export function starterAsset(tag: string): string {
	return `labrador-starter-${tag}.zip`;
}

/** The release to download a starter ZIP from, or undefined when none is known to carry one. */
export function starterRelease(): Starter | undefined {
	const tag = RELEASE_SOURCE ? RELEASE_SOURCE.tag : publishedReleases()[0]?.tag;
	if (tag === undefined) return undefined;
	const asset = starterAsset(tag);
	return {
		tag,
		asset,
		downloadUrl: `${REPOSITORY_URL}/releases/download/${tag}/${asset}`,
		releaseUrl: `${REPOSITORY_URL}/releases/tag/${tag}`,
	};
}
