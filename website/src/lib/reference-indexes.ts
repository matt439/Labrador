// Header discovery and the single owner of reference caches/invalidation.
// The supplied layout callback is lazy; collecting anchors never renders bodies.
import { readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import GithubSlugger from 'github-slugger';
import { parseHeader, type Item } from './cpp-header.ts';
import { REFERENCE_HEADERS, UNPUBLISHED_HEADERS, publishedPages, type ReferenceHeader } from './published.ts';
import { REPOSITORY_ROOT, absolutePath, readRepositoryFile, sourceUrl } from './repository.ts';
import { excerpt } from './excerpt.ts';
import { referenceMentions } from './reference-mentions.ts';
import type { Block, Explanation, ExplanationSource, Heading, Layout, ReferenceIndexes, SymbolTarget } from './reference-types.ts';

const BACKEND_FOLDERS = /^engine\/(?:render|audio|input)\/[^/]+\//;

function engineHeaders(): string[] {
	const headers: string[] = [];
	const walk = (directory: string) => {
		for (const entry of readdirSync(path.join(REPOSITORY_ROOT, directory))) {
			const child = `${directory}/${entry}`;
			if (statSync(path.join(REPOSITORY_ROOT, child)).isDirectory()) {
				walk(child);
			} else if (child.endsWith('.h') && !BACKEND_FOLDERS.test(child)) {
				headers.push(child);
			}
		}
	};
	walk('engine');
	return headers;
}

// Every public header - one directly in a module's folder - that published.ts
// neither publishes nor leaves out by name. The site build fails on any, so
// that a header added to the engine cannot quietly have no page.
function unlistedHeaders(): string[] {
	const listed = new Set([
		...REFERENCE_HEADERS.map((header) => header.source),
		...UNPUBLISHED_HEADERS.map(([source]) => source),
	]);
	return engineHeaders().filter((header) => /^engine\/[^/]+\/[^/]+\.h$/.test(header) && !listed.has(header));
}

const TYPE_DEFINITION =
	/^(\s*)(?:template\s*<.*>\s*)?(class|struct|union|enum\s+class|enum)\s+([A-Za-z_]\w*)(?:\s+final)?\s*(?::.*)?$/;

// The anchor the reference page gives a type: a type at namespace scope has a
// `## class Scene` heading, a nested one a `### View` heading (referencePage).
function typeAnchor(indent: string, keyword: string, name: string): string {
	const nested = indent.replace(/ {4}/g, '\t').length > 1;
	return new GithubSlugger().slug(nested ? name : `${keyword.replace(/\s+/g, ' ')} ${name}`);
}

const EXAMPLE_FOLDERS = ['samples', 'tests'];

export const EXPLANATION_FOLDERS = [
	'website/src/content/docs/docs/guides', 'website/src/content/docs/docs/concepts',
];

export function readableFile(repositoryPath: string): boolean {
	try {
		return statSync(absolutePath(repositoryPath)).isFile();
	} catch {
		return false;
	}
}

// The anchor each heading on a page gets: its text as GitHub slugs it,
// numbered when an earlier heading on the same page slugs the same, which is
// what the site's Markdown does.
function anchors(blocks: Block[]): [Heading, string][] {
	const slugger = new GithubSlugger();
	return blocks
		.filter((block): block is Heading => typeof block !== 'function')
		.map((heading) => [heading, slugger.slug(heading.text)]);
}

export function createReferenceIndexes(layout: (header: ReferenceHeader) => Layout): ReferenceIndexes {
	let symbolIndex: Map<string, SymbolTarget> | undefined;

	// Every type an engine header defines, by name, pointing at its reference page
	// if it has one and otherwise at its definition on GitHub. A name defined in
	// two headers is left out: a link that might land on the wrong type is worse
	// than no link.
	function symbols(): Map<string, SymbolTarget> {
		if (symbolIndex) {
			return symbolIndex;
		}
		const found = new Map<string, SymbolTarget[]>();
		const pages = publishedPages();
		for (const header of engineHeaders()) {
			const lines = readRepositoryFile(header).split('\n');
			lines.forEach((line, index) => {
				const match = line.match(TYPE_DEFINITION);
				if (!match) {
					return;
				}
				// A list of bases can continue across lines, each ending in a comma,
				// before the brace that makes this a definition.
				let next = index + 1;
				while (lines[next - 1].trimEnd().endsWith(',') && next < lines.length - 1) {
					next++;
				}
				if (lines[next]?.trim() !== '{') {
					return;
				}
				const [, indent, keyword, name] = match;
				const page = pages.get(header);
				const target = {
					header,
					url: page
						? `${page}#${typeAnchor(indent, keyword, name)}`
						: sourceUrl(header, { start: index + 1, end: index + 1 }),
				};
				found.set(name, [...(found.get(name) ?? []), target]);
			});
		}
		symbolIndex = new Map();
		for (const [name, targets] of found) {
			if (targets.length === 1) {
				symbolIndex.set(name, targets[0]);
			}
		}
		// Qualified spellings must name the namespace that actually declares the
		// type. In particular, other::Scene must not borrow labrador::Scene's
		// destination just because its final identifier is familiar.
		for (const header of REFERENCE_HEADERS) {
			const qualify = (items: Item[], owners: string[] = []) => {
				for (const item of items) {
					if (item.kind !== 'type' || item.access === 'private') {
						continue;
					}
					const target = symbolIndex!.get(item.name);
					if (target?.header === header.source) {
						const qualified = [item.namespace, ...owners, item.name].filter(Boolean).join('::');
						symbolIndex!.set(qualified, target);
					}
					qualify(item.items, [...owners, item.name]);
				}
			};
			qualify(parseHeader(header.source, readRepositoryFile(header.source)).items);
		}
		return symbolIndex;
	}

	// Read authored source before generating references, so the result cannot depend
	// on which page Astro happens to render first.
	function explanationSources(): string[] {
		const files: string[] = [];
		const walk = (directory: string) => {
			for (const entry of readdirSync(absolutePath(directory), { withFileTypes: true })) {
				const child = `${directory}/${entry.name}`;
				if (entry.isDirectory()) walk(child);
				else if (/\.mdx?$/.test(entry.name)) files.push(child);
			}
		};
		EXPLANATION_FOLDERS.forEach(walk);
		return files.sort();
	}

	function indexExplanations(
		pages: readonly ExplanationSource[], dependencies?: Set<string>
	): Map<string, Explanation[]> {
		const index = new Map<string, Map<string, Explanation>>();
		for (const page of pages) {
			const targets = referenceMentions(page.markdown, page.source, {
				codeLink,
				hasType: (name) => symbols().has(name),
				hasFunction: (name) => freeFunctions().has(name),
			}, (source, from, to) => {
				dependencies?.add(source);
				return excerpt(source, from, to).code;
			});
			for (const target of targets) {
				const links = index.get(target) ?? new Map<string, Explanation>();
				links.set(page.url, { title: page.title, url: page.url });
				index.set(target, links);
			}
		}
		return new Map([...index].map(([target, pages]) => [
			target, [...pages.values()].sort((a, b) => a.url < b.url ? -1 : a.url > b.url ? 1 : 0),
		] as const));
	}

	let explanationIndex: Map<string, Explanation[]> | undefined;
	let excerptDependencies: string[] = [];

	function explanationDependencies(): readonly string[] {
		return excerptDependencies;
	}

	function explanations(): Map<string, Explanation[]> {
		if (!explanationIndex) {
			const prefix = 'website/src/content/docs/';
			const pages = explanationSources().map((source): ExplanationSource => {
				const markdown = readRepositoryFile(source);
				const frontmatter = markdown.match(/^---\r?\n([^]*?)\r?\n---/);
				const title = frontmatter?.[1].match(/^title:\s*(.+)\s*$/m)?.[1].trim();
				if (!title) throw new Error(`Explanation page '${source}' has no title in its frontmatter.`);
				return {
					source, markdown, title: title.replace(/^(['"])(.*)\1$/, '$2'),
					url: `/${source.slice(prefix.length).replace(/\.mdx?$/, '').replace(/\/index$/, '')}/`,
				};
			});
			const dependencies = new Set<string>();
			explanationIndex = indexExplanations(pages, dependencies);
			excerptDependencies = [...dependencies].sort();
		}
		return explanationIndex;
	}

	let includerIndex: Map<string, string[]> | undefined;

	// Every file in the example folders that includes an engine header, by header,
	// in path order.
	function includers(): Map<string, string[]> {
		if (includerIndex) {
			return includerIndex;
		}
		const index = new Map<string, string[]>();
		const walk = (directory: string) => {
			for (const entry of readdirSync(path.join(REPOSITORY_ROOT, directory)).sort()) {
				const child = `${directory}/${entry}`;
				if (statSync(path.join(REPOSITORY_ROOT, child)).isDirectory()) {
					walk(child);
				} else if (/\.(?:h|cpp)$/.test(entry)) {
					for (const [, header] of readRepositoryFile(child).matchAll(/^#include "(engine\/[^"]+\.h)"/gm)) {
						index.set(header, [...new Set([...(index.get(header) ?? []), child])]);
					}
				}
			}
		};
		EXAMPLE_FOLDERS.forEach(walk);
		includerIndex = index;
		return index;
	}

	let tradeOffs: Map<string, string> | undefined;

	// T1-T12, each pointing at its heading on the Philosophy page.
	function tradeOffAnchors(): Map<string, string> {
		if (tradeOffs) {
			return tradeOffs;
		}
		tradeOffs = new Map();
		const slugger = new GithubSlugger();
		const page = publishedPages().get('docs/design/PHILOSOPHY.md')!;
		for (const line of readRepositoryFile('docs/design/PHILOSOPHY.md').split('\n')) {
			const heading = line.match(/^#{1,6}\s+(.*)$/);
			if (!heading) {
				continue;
			}
			const slug = slugger.slug(heading[1]);
			const tradeOff = heading[1].match(/^(T\d+)\./);
			if (tradeOff) {
				tradeOffs.set(tradeOff[1], `${page}#${slug}`);
			}
		}
		return tradeOffs;
	}

	let memberIndex: Map<string, string> | undefined;
	let functionIndex: Map<string, SymbolTarget> | undefined;

	// Free functions by qualified name and, when only one namespace supplies that
	// name, by bare name. Overloads on one header page share the first heading that
	// documents them. A family spread across pages has no unique destination and
	// is omitted in both forms rather than choosing an arbitrary header.
	function freeFunctions(): Map<string, SymbolTarget> {
		if (functionIndex) {
			return functionIndex;
		}
		const found = new Map<string, Map<string, SymbolTarget>>();
		const bareNames = new Map<string, Set<string>>();
		const pages = publishedPages();
		for (const header of REFERENCE_HEADERS) {
			for (const [heading, anchor] of anchors(layout(header).blocks)) {
				for (const qualified of heading.functions ?? []) {
					const targets = found.get(qualified) ?? new Map<string, SymbolTarget>();
					if (!targets.has(header.source)) {
						targets.set(header.source, { header: header.source, url: `${pages.get(header.source)}#${anchor}` });
					}
					found.set(qualified, targets);
					const bare = qualified.split('::').at(-1)!;
					const names = bareNames.get(bare) ?? new Set<string>();
					names.add(qualified);
					bareNames.set(bare, names);
				}
			}
		}
		functionIndex = new Map();
		for (const [qualified, targets] of found) {
			if (targets.size === 1) {
				functionIndex.set(qualified, targets.values().next().value!);
			}
		}
		for (const [bare, names] of bareNames) {
			if (names.size === 1) {
				const target = functionIndex.get(names.values().next().value!);
				if (target && !symbols().has(bare)) {
					functionIndex.set(bare, target);
				}
			}
		}
		return functionIndex;
	}

	// `Type::member` for every member and nested type that has a section of its
	// own, pointing at that section. A name documented in two sections, such as
	// overloads with different comments, points at the first.
	function memberAnchors(): Map<string, string> {
		if (memberIndex) {
			return memberIndex;
		}
		memberIndex = new Map();
		const index = symbols();
		const pages = publishedPages();
		for (const header of REFERENCE_HEADERS) {
			for (const [heading, anchor] of anchors(layout(header).blocks)) {
				// Only a type the symbol index places on this page: a name it left
				// out as ambiguous has no members to link either.
				if (!heading.owner || index.get(heading.owner)?.header !== header.source) {
					continue;
				}
				for (const name of heading.names ?? []) {
					const key = `${heading.owner}::${name}`;
					if (!memberIndex.has(key)) {
						memberIndex.set(key, `${pages.get(header.source)}#${anchor}`);
					}
				}
			}
		}
		return memberIndex;
	}

	// Where a name written as code in a page's prose links to: a header path, an
	// engine type, or `Type::member` - the member's own section where it has one,
	// otherwise its type's. A call is allowed, so `Scene::draw()` and
	// `Camera::frame(world_rectangle, viewport)` link as `Scene::draw` and
	// `Camera::frame` do. Anything else is not a name this site can place, and has
	// no link - except a header path the checkout does not have, which is a page
	// gone stale and fails the build naming it (PHILOSOPHY T6).
	function codeLink(code: string, page: string): string | undefined {
		if (/^engine\/[\w/]+\.h$/.test(code)) {
			if (!readableFile(code)) {
				throw new Error(`'${page}' names \`${code}\`, and the checkout has no such header.`);
			}
			return publishedPages().get(code) ?? sourceUrl(code);
		}
		const functionName = code.match(/^((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)(?:\([^]*\))?$/)?.[1];
		const functionTarget = functionName ? freeFunctions().get(functionName) : undefined;
		if (functionTarget) {
			return functionTarget.url;
		}
		const name = code.match(/^([A-Za-z_]\w*(?:::(?:~?[A-Za-z_]\w*))*)(?:\([^]*\))?$/)?.[1];
		if (!name) {
			return undefined;
		}
		const type = symbols().get(name);
		if (type) {
			return type.url;
		}
		const separator = name.lastIndexOf('::');
		const owner = name.slice(0, separator);
		const target = separator >= 0 ? symbols().get(owner) : undefined;
		if (!target) {
			return undefined;
		}
		const member = name.slice(separator + 2);
		return memberAnchors().get(`${owner.split('::').at(-1)}::${member}`) ?? target.url;
	}

	// The indexes are built once and kept. The dev server rebuilds the pages when
	// a header changes, which can move what the indexes hold, so it forgets them
	// first.
	function forgetIndexes(): void {
		symbolIndex = undefined;
		memberIndex = undefined;
		functionIndex = undefined;
		includerIndex = undefined;
		tradeOffs = undefined;
		explanationIndex = undefined;
		excerptDependencies = [];
	}
	return {
		unlistedHeaders, symbols, explanationSources, indexExplanations,
		explanationDependencies, explanations, includers, tradeOffAnchors,
		freeFunctions, memberAnchors, codeLink, forgetIndexes,
	};
}
