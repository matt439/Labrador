// Types for version-switch.mjs, which stays plain JavaScript so that
// node --test imports it without a build step.

export interface Version {
	id: string;
	label: string;
	path: string;
	commit?: string;
	published?: string;
	latest: boolean;
	pages: string[];
}

export function parseVersionsIndex(json: unknown): Version[];
export function stripBase(pathname: string, base: string): string | null;
export function targetFor(
	route: string | null,
	hash: string,
	fromId: string,
	version: Pick<Version, 'path' | 'pages'>
): { href: string; missing: boolean };
export function missingNotice(params: URLSearchParams, currentLabel: string): string | null;
