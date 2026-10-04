// The site's two additions to the Markdown pipeline.
//
// Both are Sätteri mdast plugins, which is what Astro's default Markdown
// processor takes; Starlight registers its own transforms beside them.

import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { defineMdastPlugin, type MdastPluginEntry } from 'satteri';
import { DESIGN_DOCUMENTS, publishedPages } from './published';
import { REPOSITORY_ROOT, absolutePath, sourceUrl, toRepositoryPath } from './repository';

// Where the site's own pages live, from the repository root. A document read
// from elsewhere in the checkout is rendered *as if* it were here - at the path
// its page has - so that Starlight's heading anchors and asides apply to it the
// same as to a page written for the site. The link plugin undoes that mapping
// to find the file the links were actually written relative to.
export const SITE_CONTENT = 'website/src/content/docs';

export function siteContentFileURL(slug: string): URL {
	return pathToFileURL(path.join(REPOSITORY_ROOT, ...SITE_CONTENT.split('/'), ...slug.split('/')) + '.mdx');
}

function sourceOf(fileURL: URL): string | undefined {
	const repositoryPath = toRepositoryPath(fileURLToPath(fileURL));
	if (!repositoryPath.startsWith(`${SITE_CONTENT}/`)) {
		return repositoryPath;
	}
	const slug = repositoryPath.slice(SITE_CONTENT.length + 1).replace(/\.mdx?$/, '');
	return DESIGN_DOCUMENTS.find((document) => document.slug === slug)?.source;
}

// A scheme, a protocol-relative or absolute path, or an anchor on this page.
const NOT_RELATIVE = /^(?:[a-z][a-z0-9+.-]*:|\/|#)/i;

// A relative link, written for a reader of the repository, resolved for a
// reader of the site: to the target's page if the site publishes it, otherwise
// to the file on GitHub at the revision this build describes. A link to a file
// that does not exist fails the build naming both ends (PHILOSOPHY T6) - the
// check that browsing the repository on GitHub never runs.
function resolveRelative(url: string, fromSource: string): string {
	const hash = url.indexOf('#');
	const target = hash < 0 ? url : url.slice(0, hash);
	const anchor = hash < 0 ? '' : url.slice(hash);

	const repositoryPath = path.posix
		.normalize(path.posix.join(path.posix.dirname(fromSource), decodeURI(target)))
		.replace(/\/$/, '');

	if (repositoryPath.startsWith('..')) {
		throw new Error(`'${fromSource}' links to '${url}', which is outside the repository.`);
	}
	if (!existsSync(absolutePath(repositoryPath))) {
		throw new Error(`'${fromSource}' links to '${url}', and the checkout has no '${repositoryPath}'.`);
	}

	const page = publishedPages().get(repositoryPath);
	return (page ?? sourceUrl(repositoryPath)) + anchor;
}

// Applies only to documents that come from outside website/. A page written for
// the site links with site paths, and starlight-links-validator checks those.
export const checkoutLinks: MdastPluginEntry = ({ fileURL }) => {
	const source = fileURL ? sourceOf(fileURL) : undefined;
	if (!source || source.startsWith('website/')) {
		return undefined;
	}

	return defineMdastPlugin({
		name: 'labrador-checkout-links',
		link(node, ctx) {
			if (!NOT_RELATIVE.test(node.url)) {
				ctx.setProperty(node, 'url', resolveRelative(node.url, source));
			}
		},
		definition(node, ctx) {
			if (!NOT_RELATIVE.test(node.url)) {
				ctx.setProperty(node, 'url', resolveRelative(node.url, source));
			}
		},
		image(node) {
			if (!NOT_RELATIVE.test(node.url)) {
				throw new Error(
					`'${source}' has a relative image, '${node.url}', and the site does not copy ` +
						`images out of the checkout yet. Add that before publishing this document.`
				);
			}
		},
	});
};

const ESCAPES: Record<string, string> = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' };

// A fenced `mermaid` block becomes the element the diagram script looks for,
// instead of a highlighted code block. ARCHITECTURE.md draws its two diagrams
// this way because GitHub renders them, so the site has to as well.
export const mermaidBlocks = defineMdastPlugin({
	name: 'labrador-mermaid',
	code(node, ctx) {
		if (node.lang !== 'mermaid') {
			return;
		}
		const escaped = node.value.replace(/[&<>"]/g, (character) => ESCAPES[character]);
		ctx.replaceNode(node, { type: 'html', value: `<pre class="mermaid">${escaped}</pre>` });
	},
});
