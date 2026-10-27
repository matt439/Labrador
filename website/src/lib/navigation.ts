// The site's top-level sections, in the order the header shows them.
import { sitePath, stripSiteBase } from './site-path.ts';

export const NAVIGATION = [
	{ label: 'Why Labrador?', href: sitePath('/why/') },
	{ label: 'Docs', href: sitePath('/docs/') },
	{ label: 'Examples', href: sitePath('/examples/') },
];

export function isCurrent(pathname: string, href: string): boolean {
	const current = stripSiteBase(pathname);
	const section = stripSiteBase(href);
	return current === section || current.startsWith(section);
}
