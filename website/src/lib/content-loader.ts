// The docs collection: the site's own pages, plus the pages it reads from the
// rest of the checkout.
//
// Starlight renders one collection, so both kinds of page are entries in it.
// Its own loader reads src/content/docs/ as usual; this one then adds the
// design documents and the reference pages, rendered through the same
// Markdown pipeline so they get the same headings, anchors, code blocks and
// search indexing as a page written for the site.

import type { Loader, LoaderContext } from 'astro/loaders';
import { docsLoader } from '@astrojs/starlight/loaders';
import { siteContentFileURL } from './markdown-plugins';
import { DESIGN_DOCUMENTS, REFERENCE_HEADERS, type PublishedDocument } from './published';
import { referencePage, unlistedHeaders } from './reference';
import { REVISION, absolutePath, commitUrl, editUrl, readRepositoryFile, sourceUrl } from './repository';

async function store(
	context: LoaderContext,
	id: string,
	markdown: string,
	data: Record<string, unknown>
): Promise<void> {
	const rendered = await context.renderMarkdown(markdown, { fileURL: siteContentFileURL(id) });
	// The path the page would have if it were written for the site. No file is
	// there; Starlight builds the sidebar tree from these paths, and the
	// Markdown above was rendered as if it lived at the same place. `.mdx`
	// because starlight-links-validator, placing an error in a `.md` page,
	// opens the file to measure its frontmatter - and there is no file.
	const filePath = `src/content/docs/${id}.mdx`;
	const parsed = await context.parseData({ id, data, filePath });
	context.store.set({
		id,
		data: parsed,
		body: markdown,
		rendered,
		filePath,
		digest: context.generateDigest(markdown),
	});
}

// A design document is published as written, under a note that says where it
// came from and how to read it. Its own title line is dropped because the page
// title replaces it.
async function loadDesignDocument(context: LoaderContext, document: PublishedDocument): Promise<void> {
	const source = readRepositoryFile(document.source);
	const body = source.replace(/^# .*\n+/, '');
	const markdown =
		`:::note[Read from the repository]\n` +
		`This page is [\`${document.source}\`](${sourceUrl(document.source)}) at ` +
		`[\`${REVISION.short}\`](${commitUrl()}), published as written. The design documents ` +
		`describe the engine Labrador is becoming, in the present tense, and say nothing about ` +
		`the current code; a trade-off is cited by its number, so "T3" is a complete argument.\n` +
		`:::\n\n` +
		body;

	await store(context, document.slug, markdown, {
		title: document.title,
		description: document.description,
		editUrl: editUrl(document.source),
		sidebar: { order: document.order },
	});
}

async function loadReference(context: LoaderContext, index: number): Promise<void> {
	const header = REFERENCE_HEADERS[index];
	const page = referencePage(header);
	await store(context, header.slug, page.markdown, {
		title: header.title,
		description: page.description,
		editUrl: editUrl(header.source),
		sidebar: { order: header.order },
	});
}

async function loadCheckout(context: LoaderContext): Promise<void> {
	const unlisted = unlistedHeaders();
	if (unlisted.length > 0) {
		throw new Error(
			`Public engine headers with no reference page and no reason given: ${unlisted.join(', ')}. ` +
				`Add each to REFERENCE_MODULES or UNPUBLISHED_HEADERS in website/src/lib/published.ts.`
		);
	}
	for (const document of DESIGN_DOCUMENTS) {
		await loadDesignDocument(context, document);
	}
	for (let index = 0; index < REFERENCE_HEADERS.length; index++) {
		await loadReference(context, index);
	}
}

export function labradorDocsLoader(): Loader {
	const site = docsLoader();
	return {
		name: 'labrador-docs',
		load: async (context) => {
			// First, because Starlight's loader deletes every entry it did not
			// load itself - which would include the ones added below.
			await site.load(context);
			await loadCheckout(context);

			// In dev, an edit to a published document or header reloads its page.
			// A header change can move what the others link to, so all of them
			// are rebuilt; there are few enough that it costs nothing.
			const watched = new Set<string>([
				...DESIGN_DOCUMENTS.map((document) => absolutePath(document.source)),
				...REFERENCE_HEADERS.map((header) => absolutePath(header.source)),
			]);
			context.watcher?.add([...watched]);
			context.watcher?.on('change', async (changed) => {
				if (watched.has(changed)) {
					context.logger.info(`Reloading pages read from the checkout (${changed})`);
					await loadCheckout(context);
				}
			});
		},
	};
}
