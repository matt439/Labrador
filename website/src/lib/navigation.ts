// The site's top-level sections, in the order the header shows them.
export const NAVIGATION = [
	{ label: 'Why Labrador?', href: '/why/' },
	{ label: 'Docs', href: '/docs/' },
	{ label: 'Examples', href: '/examples/' },
];

export function isCurrent(pathname: string, href: string): boolean {
	return pathname === href || pathname.startsWith(href);
}
