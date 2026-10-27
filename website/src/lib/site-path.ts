// Page indexes use paths relative to the site root. Rendering adds the chosen
// documentation version, while external URLs and page-local anchors stay intact.
import { releaseVersion } from '../../scripts/snapshot-source.mjs';

const tag = process.env.LABRADOR_DOCS_TAG;
if (tag !== undefined) releaseVersion(tag);

export const SITE_BASE = tag === undefined ? '/' : `/versions/${tag}/`;

export function sitePath(url: string): string {
	if (SITE_BASE === '/' || !url.startsWith('/') || url.startsWith('//')) return url;
	const pathname = url.split(/[?#]/, 1)[0];
	if (pathname === SITE_BASE.slice(0, -1) || url.startsWith(SITE_BASE)) return url;
	return SITE_BASE + url.slice(1);
}

export function stripSiteBase(pathname: string): string {
	if (SITE_BASE === '/') return pathname;
	if (pathname === SITE_BASE.slice(0, -1)) return '/';
	return pathname.startsWith(SITE_BASE) ? `/${pathname.slice(SITE_BASE.length)}` : pathname;
}
