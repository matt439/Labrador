// What the site publishes from the rest of the checkout, and where.
//
// This is the explicit set the website plan asks for: a document is on the site
// because it is listed here, never because it happens to be under docs/. The
// historical reviews, the surveys, the performance evidence and the planning
// documents stay in the repository and are linked to there.
//
// A relative link inside any published document is resolved against this
// list: a target that is published becomes a link to its page, and anything
// else becomes a link to the file on GitHub at the built revision
// (remark-checkout-links.ts).

export interface PublishedDocument {
	// The file in the repository, from the root.
	source: string;
	// The site path, without leading or trailing slashes.
	slug: string;
	title: string;
	description: string;
	order: number;
}

export const DESIGN_DOCUMENTS: PublishedDocument[] = [
	{
		source: 'docs/design/PHILOSOPHY.md',
		slug: 'docs/design/philosophy',
		title: 'Philosophy',
		description:
			'The twelve trade-offs Labrador makes, the price of each, and the engine they describe.',
		order: 1,
	},
	{
		source: 'docs/design/ARCHITECTURE.md',
		slug: 'docs/design/architecture',
		title: 'Architecture',
		description: 'The targets, the tree, the module table, and where the engine/game boundary runs.',
		order: 2,
	},
	{
		source: 'docs/design/CONVENTIONS.md',
		slug: 'docs/design/conventions',
		title: 'Conventions',
		description: 'Naming, files, and what a comment is for.',
		order: 3,
	},
];

export interface ReferenceHeader {
	// The header, from the repository root.
	source: string;
	slug: string;
	// The sidebar label: the primary type the header declares.
	title: string;
	order: number;
}

// The prototype's slice of the public headers. Small on purpose: the plan asks
// for a candidate generator to be tried on a slice before the toolchain is
// chosen, and these three cover the shapes the rest of the engine uses - an
// interface the engine calls, an interface the game implements, and a concrete
// class with a template member, a nested type and grouped overloads.
export const REFERENCE_HEADERS: ReferenceHeader[] = [
	{ source: 'engine/core/state.h', slug: 'docs/reference/core/state', title: 'State', order: 1 },
	{
		source: 'engine/core/game_object.h',
		slug: 'docs/reference/core/game-object',
		title: 'GameObject',
		order: 2,
	},
	{ source: 'engine/scene/scene.h', slug: 'docs/reference/scene/scene', title: 'Scene', order: 3 },
];

// Every repository file that has a page of its own, keyed by repository path.
export function publishedPages(): Map<string, string> {
	const pages = new Map<string, string>();
	for (const document of DESIGN_DOCUMENTS) {
		pages.set(document.source, `/${document.slug}/`);
	}
	for (const header of REFERENCE_HEADERS) {
		pages.set(header.source, `/${header.slug}/`);
	}
	return pages;
}
