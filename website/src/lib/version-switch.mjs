// What the version selector and the arrival notice decide, kept free of the
// DOM so that node --test can check it. Every release snapshot freezes a copy
// of this logic, and each copy keeps reading the development site's current
// /versions/index.json, so the reader accepts schema 1 forever and ignores
// fields it does not know.

const TAG = /^v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$/;
const ROUTE = /^[a-z0-9._\/-]{0,200}$/i;

function isTag(id) {
	return typeof id === 'string' && TAG.test(id);
}

function tagParts(tag) {
	return tag.slice(1).split('.').map(BigInt);
}

/** Newest first. */
function byNewest(a, b) {
	const left = tagParts(a.id);
	const right = tagParts(b.id);
	for (let index = 0; index < 3; index += 1) {
		if (left[index] !== right[index]) return left[index] > right[index] ? -1 : 1;
	}
	return 0;
}

// A route is a page's path below its version's base: '' for the home page,
// 'docs/' for the overview. The same check guards the index's page lists and
// the route a reader brings in the query string.
function isRoute(route) {
	return typeof route === 'string' && ROUTE.test(route) && !route.startsWith('/')
		&& !route.includes('..') && !route.includes('//');
}

function readVersion(entry) {
	if (entry === null || typeof entry !== 'object') return undefined;
	const { id } = entry;
	if (id === 'development') {
		if (entry.path !== '/') return undefined;
	} else if (!isTag(id) || entry.path !== `/versions/${id}/`) {
		return undefined;
	}
	if (!Array.isArray(entry.pages)) return undefined;
	const fallback = id === 'development' ? 'Development' : id;
	const label = typeof entry.label === 'string' && entry.label.trim() && entry.label.length <= 64 ? entry.label : fallback;
	return {
		id,
		label,
		path: entry.path,
		commit: typeof entry.commit === 'string' ? entry.commit : undefined,
		published: typeof entry.published === 'string' ? entry.published : undefined,
		latest: false,
		pages: entry.pages.filter(isRoute),
	};
}

/**
 * The versions an index lists: Development first, then releases newest first,
 * with the newest marked latest. An unknown schema is refused; an entry whose
 * id or path is not one this reader can trust is dropped, as is a repeated id.
 */
export function parseVersionsIndex(json) {
	const index = typeof json === 'string' ? JSON.parse(json) : json;
	if (index === null || typeof index !== 'object' || index.schemaVersion !== 1) {
		throw new Error(`Expected a versions index with schemaVersion 1, found ${JSON.stringify(index?.schemaVersion)}.`);
	}
	if (!Array.isArray(index.versions)) throw new Error('Expected a versions index with a versions array.');
	const seen = new Set();
	const versions = [];
	for (const entry of index.versions) {
		const version = readVersion(entry);
		if (!version || seen.has(version.id)) continue;
		seen.add(version.id);
		versions.push(version);
	}
	const releases = versions.filter((version) => version.id !== 'development').sort(byNewest);
	if (releases.length) releases[0].latest = true;
	return [...versions.filter((version) => version.id === 'development'), ...releases];
}

/**
 * The route of a page below base, or null when the page is outside it.
 * 'docs/index.html' and 'docs' both name the route 'docs/'.
 */
export function stripBase(pathname, base) {
	if (typeof pathname !== 'string' || !pathname.startsWith('/')) return null;
	let path = pathname.endsWith('/index.html') ? pathname.slice(0, -'index.html'.length) : pathname;
	if (!path.endsWith('/')) path += '/';
	if (!path.startsWith(base)) return null;
	const route = path.slice(base.length);
	return isRoute(route) ? route : null;
}

/**
 * Where choosing version takes a reader on route: the same page and heading
 * when version has it, otherwise version's documentation overview, told
 * which page was missing and which version the reader came from. The shared
 * list at versions/ belongs to no version, so from it every version opens its
 * overview with nothing reported missing.
 */
export function targetFor(route, hash, fromId, version) {
	if (route === 'versions/') return { href: `${version.path}docs/`, missing: false };
	if (route !== null && version.pages.includes(route)) {
		return { href: version.path + route + (hash ?? ''), missing: false };
	}
	const query = `version-missing=${encodeURIComponent(route ?? '')}&from=${encodeURIComponent(fromId)}`;
	return { href: `${version.path}docs/?${query}`, missing: true };
}

/**
 * The sentence the overview shows a reader who arrived from another version's
 * page that this version lacks, or null when they did not. The query string is
 * anyone's to write, so only a plain route and a real version are repeated
 * back; anything else gets the general sentence.
 */
export function missingNotice(params, currentLabel) {
	if (!params.has('version-missing')) return null;
	const route = params.get('version-missing') ?? '';
	const from = params.get('from') ?? '';
	const general = `The page you were reading is not part of ${currentLabel}, so this is the ${currentLabel} documentation overview.`;
	if (route === '' || !isRoute(route) || !(from === 'development' || isTag(from))) return general;
	const fromLabel = from === 'development' ? 'Development' : from;
	return `The page /${route} that you were reading in ${fromLabel} is not part of ${currentLabel}, `
		+ `so this is the ${currentLabel} documentation overview.`;
}
