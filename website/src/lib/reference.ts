// Turns a parsed header into the Markdown of its reference page.
//
// The comment text is published as the header states it, every word: the
// reference is a view of the header, not an edit of it. What the page adds is
// structure - a heading per declaration group, the signatures as code, the
// public and protected members only - and links: a type the engine defines,
// a member of one, a header path, and a trade-off number each become a link to
// where they are defined. Last, it lists the samples and tests that include
// the header.
//
// The same index links the rest of the site: a name written as code in a
// guide reaches its section here through codeLink.

import { readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import GithubSlugger from 'github-slugger';
import { parseHeader, type CommentBlock, type Declaration, type Item, type TypeItem } from './cpp-header.ts';
import { REFERENCE_HEADERS, UNPUBLISHED_HEADERS, publishedPages, type ReferenceHeader, type ReferenceModule } from './published.ts';
import { REPOSITORY_ROOT, REVISION, absolutePath, commitUrl, readRepositoryFile, sourceUrl } from './repository.ts';

// ------------------------------------------------------------------ the index

interface SymbolTarget {
	url: string;
	header: string;
}

// Backend folders hold no public API, and nothing outside one may include from
// it (ARCHITECTURE, Modules), so their types are not link targets either.
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
export function unlistedHeaders(): string[] {
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

let symbolIndex: Map<string, SymbolTarget> | undefined;

// Every type an engine header defines, by name, pointing at its reference page
// if it has one and otherwise at its definition on GitHub. A name defined in
// two headers is left out: a link that might land on the wrong type is worse
// than no link.
export function symbols(): Map<string, SymbolTarget> {
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
	// type. In particular, mattmath::Scene must not borrow labrador::Scene's
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

// The folders whose files show the API in use: the two samples, and the tests
// that state its behaviour.
const EXAMPLE_FOLDERS = ['samples', 'tests'];

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

// ------------------------------------------------------------------ the prose

function escapeHtml(text: string): string {
	return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function link(url: string, html: string): string {
	return `<a href="${url}">${html}</a>`;
}

// A qualified member, function or header path is unambiguous in prose. Bare
// function names need code formatting or call parentheses; bare type names
// need code formatting or compound PascalCase, since "Scene" and "State" can
// both be ordinary words at the start of a sentence.
function linkSymbols(escaped: string, inCode: boolean): string {
	const index = symbols();
	const pages = publishedPages();

	return escaped.replace(
		/\b(engine\/[\w/]+\.(?:h|md))\b|\b(T(?:1[0-2]|[1-9]))\b|\b([A-Za-z_]\w*(?:::(?:~?[A-Za-z_]\w*))*)(\(\))?/g,
		(match, header: string | undefined, tradeOff: string | undefined, name: string | undefined, call: string | undefined, offset: number) => {
			if (header) {
				if (!readableFile(header)) {
					return match;
				}
				return link(pages.get(header) ?? sourceUrl(header), match);
			}
			if (tradeOff) {
				const anchor = tradeOffAnchors().get(tradeOff);
				return anchor ? link(anchor, match) : match;
			}
			// Member access and file extensions are not free-function names.
			if (/(?:\.|-&gt;|::)\s*$/.test(escaped.slice(0, offset)) || /^\.\w/.test(escaped.slice(offset + match.length))) {
				return match;
			}
			const fn = name ? freeFunctions().get(name) : undefined;
			if (fn && (inCode || call || name!.includes('::'))) {
				return link(fn.url, match);
			}
			const separator = name?.lastIndexOf('::') ?? -1;
			const type = name && (index.has(name) ? name : separator >= 0 ? name.slice(0, separator) : undefined);
			const member = name && type !== name ? name.slice(separator + 2) : undefined;
			const target = type ? index.get(type) : undefined;
			if (!target) {
				return match;
			}
			if (member) {
				return link(memberAnchors().get(`${type!.split('::').at(-1)}::${member}`) ?? target.url, match);
			}
			const compound = /^[A-Z][a-z0-9]+[A-Z]/.test(type!);
			if (inCode || compound || type!.includes('::')) {
				return link(target.url, match);
			}
			return match;
		}
	);
}

function readableFile(repositoryPath: string): boolean {
	try {
		return statSync(absolutePath(repositoryPath)).isFile();
	} catch {
		return false;
	}
}

// Backticked spans become code, then symbols are linked inside and outside
// them. Everything is escaped first: header prose talks about `<` and `*` and
// is not Markdown.
function inline(text: string): string {
	return text
		.split(/(`[^`]+`)/)
		.map((part) => {
			if (part.startsWith('`') && part.endsWith('`') && part.length > 2) {
				return `<code>${linkSymbols(escapeHtml(part.slice(1, -1)), true)}</code>`;
			}
			return linkSymbols(escapeHtml(part), false);
		})
		.join('');
}

// A comment block as HTML prose and fenced C++: paragraphs split at blank
// comment lines, `- ` lines as a list, and indented lines as code. Prose is
// HTML rather than Markdown because header prose is not Markdown - a
// `std::vector<Segment>` or a `const float*` in a sentence would be read as a
// tag or as emphasis.
export function commentHtml(comment: CommentBlock | undefined): string {
	if (!comment) {
		return '';
	}
	const paragraphs: string[][] = [[]];
	for (const line of comment.lines) {
		if (line.trim() === '') {
			paragraphs.push([]);
		} else {
			paragraphs[paragraphs.length - 1].push(line);
		}
	}

	const blocks: string[] = [];
	for (const paragraph of paragraphs.filter((lines) => lines.length > 0)) {
		if (paragraph.every((line) => /^(?: {4}|\t)/.test(line))) {
			blocks.push(fence(paragraph.map((line) => line.replace(/^(?: {4}|\t)/, '')).join('\n')));
			continue;
		}
		if (paragraph[0].startsWith('- ')) {
			const items: string[] = [];
			for (const line of paragraph) {
				if (line.startsWith('- ')) {
					items.push(line.slice(2));
				} else {
					items[items.length - 1] += ` ${line.trim()}`;
				}
			}
			blocks.push(`<ul>${items.map((item) => `<li>${inline(item)}</li>`).join('')}</ul>`);
			continue;
		}
		blocks.push(`<p>${inline(paragraph.map((line) => line.trim()).join(' '))}</p>`);
	}
	// One block per line, with a blank line between, so that Markdown sees
	// each as an HTML block of its own.
	return blocks.join('\n\n');
}

function noteHtml(comment: CommentBlock): string {
	// Blank lines let Markdown recognise fenced examples inside the wrapper.
	return `<div class="ref-note">\n\n${commentHtml(comment)}\n\n</div>`;
}

// ------------------------------------------------------------------ the page
//
// A page is laid out as a list of blocks: headings, which follow from the
// header's structure alone, and content, which is rendered only when the page
// is. Keeping them apart is what lets the anchor of every member on every page
// be known before any prose is rendered - and the prose links to those
// anchors.

interface Heading {
	depth: 2 | 3;
	text: string;
	// For `Type::member` links: the type the section belongs to, and the
	// members and nested types it documents.
	owner?: string;
	names?: string[];
	// Namespace-qualified free functions documented by this heading.
	functions?: string[];
}

type Block = Heading | (() => string);

function fence(code: string): string {
	return '```cpp\n' + code + '\n```';
}

function typeHeading(item: TypeItem): string {
	return `${item.keyword} ${item.name}`;
}

function typeSignature(item: TypeItem): string {
	return item.bases.length > 0 ? `${item.keyword} ${item.name} : ${item.bases.join(', ')}` : `${item.keyword} ${item.name}`;
}

// A declaration as a caller needs it: the signature, without an inline body or
// a constructor's initialiser list, which are implementation and name private
// members. `= default`, `= delete` and `= 0` stay, because they are contract.
function signature(text: string): string {
	let open = text.indexOf('(');
	if (open < 0) {
		return text;
	}
	// operator() spells its own parentheses before the parameter list.
	if (/operator\s*$/.test(text.slice(0, open)) && text.startsWith('()', open)) {
		open = text.indexOf('(', open + 2);
	}
	let depth = 0;
	let close = -1;
	for (let index = open; index >= 0 && index < text.length; index++) {
		if (text[index] === '(') {
			depth++;
		} else if (text[index] === ')' && --depth === 0) {
			close = index;
			break;
		}
	}
	if (close < 0) {
		return text;
	}
	const rest = text.slice(close + 1);
	const cut = rest.search(/(?<!:):(?!:)|\{/);
	return cut < 0 ? text : `${(text.slice(0, close + 1) + rest.slice(0, cut)).trimEnd()};`;
}

// The definition as written, without its comments: for an enumeration, the
// list of values is the clearest thing to show.
function definitionWithoutComments(item: TypeItem): string {
	return item.source
		.split('\n')
		.filter((line) => !line.trim().startsWith('//'))
		.map((line) => line.replace(/\s*\/\/.*$/, ''))
		.join('\n');
}

function enumerators(type: TypeItem): string {
	const out: string[] = [];
	const described: string[] = [];
	for (const item of type.items) {
		if (item.kind === 'note') {
			out.push(noteHtml(item.comment));
		} else if (item.kind === 'group' && item.comment) {
			const names = item.declarations.map((declaration) => escapeHtml(declaration.text)).join(', ');
			described.push(`<dt><code>${names}</code></dt>\n<dd>\n\n${commentHtml(item.comment)}\n\n</dd>`);
		}
	}
	if (described.length > 0) {
		out.unshift(`<dl class="ref-enumerators">\n${described.join('\n')}\n</dl>`);
	}
	return out.join('\n\n');
}

function visible(item: Item): boolean {
	if (item.kind === 'group' || item.kind === 'type') {
		return item.access !== 'private';
	}
	return item.kind === 'note';
}

// A nested type small enough to read whole is shown whole, private members
// excepted; that is clearer than a heading per field.
function nestedType(blocks: Block[], item: TypeItem, qualifier: string): void {
	blocks.push({ depth: 3, text: item.name, owner: qualifier, names: [item.name] });
	blocks.push(() => fence(definitionWithoutComments(item)));
	blocks.push(() => commentHtml(item.comment));
	blocks.push(() => `<p class="ref-qualified">Declared as <code>${escapeHtml(qualifier)}::${item.name}</code>.</p>`);
}

function members(blocks: Block[], type: TypeItem): void {
	for (const item of type.items.filter(visible)) {
		if (item.kind === 'note') {
			blocks.push(() => noteHtml(item.comment));
		} else if (item.kind === 'type') {
			nestedType(blocks, item, type.name);
		} else if (item.kind === 'group') {
			const names = [...new Set(item.declarations.map((declaration) => declaration.name))];
			const label = item.access === 'protected' ? ' (protected)' : '';
			blocks.push({ depth: 3, text: `${names.join(' · ')}${label}`, owner: type.name, names });
			blocks.push(() => fence(item.declarations.map((declaration) => signature(declaration.text)).join('\n')));
			blocks.push(() => commentHtml(item.comment));
		}
	}
}

function typesUsed(document: ReturnType<typeof parseHeader>, own: Set<string>): string[] {
	const index = symbols();
	const used = new Set<string>();
	const visit = (items: Item[]) => {
		for (const item of items) {
			if (item.kind === 'group' && item.access !== 'private') {
				for (const declaration of item.declarations) {
					for (const word of declaration.text.match(/\b[A-Z][A-Za-z0-9]*\b/g) ?? []) {
						if (index.has(word) && !own.has(word)) {
							used.add(word);
						}
					}
				}
			} else if (item.kind === 'type' && item.access !== 'private') {
				item.bases.forEach((base) => {
					const name = base.split(/\s+/).pop()!;
					if (index.has(name) && !own.has(name)) {
						used.add(name);
					}
				});
				visit(item.items);
			} else if (item.kind === 'forward' && index.has(item.name) && !own.has(item.name)) {
				used.add(item.name);
			}
		}
	};
	visit(document.items);
	return [...used].sort();
}

// A member template defined below its class - `void StateContext::push(...)`
// - is the definition of something the class's own section already lists.
// What tells it from a free function is that its name is qualified.
function outOfLineMember(text: string): boolean {
	const flat = text.replace(/\s+/g, ' ').replace(/^template\s*<[^]*?>\s*/, '');
	const call = flat.indexOf('(');
	return call >= 0 && /[A-Za-z_]\w*::~?[A-Za-z_]\w*\s*$/.test(flat.slice(0, call));
}

// Only named functions at namespace scope participate. Aliases, initialised
// values, assertions and out-of-line members can all contain parentheses too.
// Operators stay on their declaring page: their punctuation is not an
// identifier a prose or code-token link can resolve without parsing C++.
function freeFunctionName(declaration: Declaration): string | undefined {
	const flat = declaration.text.replace(/\s+/g, ' ').replace(/^template\s*<[^]*?>\s*/, '').trim();
	if (/^(?:using|typedef|static_assert)\b/.test(flat) || outOfLineMember(flat)) {
		return undefined;
	}
	const call = flat.indexOf('(');
	const before = call < 0 ? '' : flat.slice(0, call);
	if (/[=;{}]/.test(before) || !/^[A-Za-z_]\w*$/.test(declaration.name) ||
		!new RegExp(`\\S\\s+${declaration.name}\\s*$`).test(before)) {
		return undefined;
	}
	return [declaration.namespace, declaration.name].filter(Boolean).join('::');
}

// The samples and tests that include a header, grouped by folder. Only a
// direct include is found, and the conventions do not ask a file to include
// everything it uses, so this is where to start reading rather than every use.
function includedBy(source: string): string {
	const files = includers().get(source) ?? [];
	if (files.length === 0) {
		return '<p>No sample or test includes this header directly.</p>';
	}
	const folders = new Map<string, string[]>();
	for (const file of files) {
		const folder = file.slice(0, file.lastIndexOf('/') + 1);
		folders.set(folder, [...(folders.get(folder) ?? []), file.slice(folder.length)]);
	}
	const items = [...folders].map(
		([folder, names]) =>
			`<li><code>${escapeHtml(folder)}</code> ${names
				.map((name) => link(sourceUrl(folder + name), `<code>${escapeHtml(name)}</code>`))
				.join(', ')}</li>`
	);
	return (
		'<p>The files that include this header directly. A file can also reach it through another header.</p>\n\n' +
		`<ul class="ref-includers">${items.join('')}</ul>`
	);
}

interface Layout {
	blocks: Block[];
	description: string;
}

function layout(header: ReferenceHeader): Layout {
	const document = parseHeader(header.source, readRepositoryFile(header.source));
	const types = document.items.filter((item): item is TypeItem => item.kind === 'type');
	const own = new Set(types.map((type) => type.name));
	const namespace = document.namespaces.join('::');

	const blocks: Block[] = [];
	blocks.push(
		() =>
			`<p class="ref-source">Generated from <a href="${sourceUrl(header.source)}"><code>${escapeHtml(header.source)}</code></a> ` +
			`at <a href="${commitUrl()}"><code>${REVISION.short}</code></a>. The text under each declaration is the ` +
			`header's own comment, word for word. <a href="/docs/reference/">About the reference</a> says how these ` +
			`pages are made.</p>`
	);
	blocks.push(
		() => `<p><a href="/docs/reference/${header.module}/">Browse the ${header.module} module</a></p>`
	);
	blocks.push(
		() =>
			`<p class="ref-include"><code>#include "${escapeHtml(header.source)}"</code>` +
			(namespace ? ` · namespace <code>${escapeHtml(namespace)}</code>` : '') +
			`</p>`
	);

	for (const item of document.items) {
		if (item.kind === 'note') {
			blocks.push(() => noteHtml(item.comment));
		}
		if (item.kind === 'type') {
			blocks.push({ depth: 2, text: typeHeading(item) });
			if (item.keyword.startsWith('enum')) {
				blocks.push(() => fence(definitionWithoutComments(item)));
				blocks.push(() => commentHtml(item.comment));
				blocks.push(() => enumerators(item));
			} else {
				blocks.push(() => fence(typeSignature(item)));
				blocks.push(() => commentHtml(item.comment));
				members(blocks, item);
			}
		}
		if (item.kind === 'group') {
			// A static_assert guards the implementation; the comment beside the
			// thing it guards is where a caller reads the rule.
			const declarations = item.declarations.filter(
				(declaration) => !outOfLineMember(declaration.text) && !declaration.text.startsWith('static_assert')
			);
			if (declarations.length === 0) {
				continue;
			}
			const names = [...new Set(declarations.map((declaration) => declaration.name))];
			const functions = declarations.map(freeFunctionName).filter((name): name is string => !!name);
			blocks.push({ depth: 2, text: names.join(' · '), functions });
			blocks.push(() => fence(declarations.map((declaration) => signature(declaration.text)).join('\n')));
			blocks.push(() => commentHtml(item.comment));
		}
	}

	const used = typesUsed(document, own);
	if (used.length > 0) {
		const index = symbols();
		blocks.push({ depth: 2, text: 'Related types' });
		blocks.push(
			() =>
				`<p>Named in the public declarations above: ${used
					.map((name) => link(index.get(name)!.url, `<code>${name}</code>`))
					.join(', ')}.</p>`
		);
	}

	blocks.push({ depth: 2, text: 'In the samples and tests' });
	blocks.push(() => includedBy(header.source));

	const primary = types[0];
	const description = primary
		? `${typeHeading(primary)}, from ${header.source}.`
		: `${header.source}.`;
	return { blocks, description };
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

let memberIndex: Map<string, string> | undefined;
let functionIndex: Map<string, SymbolTarget> | undefined;

// Free functions by qualified name and, when only one namespace supplies that
// name, by bare name. Overloads on one header page share the first heading that
// documents them. A family spread across pages has no unique destination and
// is omitted in both forms rather than choosing an arbitrary header.
export function freeFunctions(): Map<string, SymbolTarget> {
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
export function codeLink(code: string, page: string): string | undefined {
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
export function forgetIndexes(): void {
	symbolIndex = undefined;
	memberIndex = undefined;
	functionIndex = undefined;
	includerIndex = undefined;
	tradeOffs = undefined;
}

export interface ReferencePage {
	header: ReferenceHeader;
	markdown: string;
	description: string;
}

export function referencePage(header: ReferenceHeader): ReferencePage {
	const { blocks, description } = layout(header);
	const markdown = blocks
		.map((block) => (typeof block === 'function' ? block() : `${'#'.repeat(block.depth)} ${block.text}`))
		.filter(Boolean)
		.join('\n\n');
	return { header, markdown, description };
}

export function referencePages(): ReferencePage[] {
	return REFERENCE_HEADERS.map(referencePage);
}

// Module lists use the same inventory as the sidebar and the header pages, so
// adding a published header also makes it discoverable from its module.
export function moduleReferencePage(module: ReferenceModule): string {
	const headers = REFERENCE_HEADERS.filter((header) => header.module === module.name);
	const rows = headers.map((header) => {
		const file = header.source.slice(header.source.lastIndexOf('/') + 1);
		return `| [${header.title}](/${header.slug}/) | \`${file}\` |`;
	});
	return [
		module.summary,
		`The public headers in \`engine/${module.name}/\`. Choose a page for its declarations, ` +
			`contracts, related types, and links to the samples and tests that include it.`,
		'## Headers',
		['| Reference page | Header |', '| --- | --- |', ...rows].join('\n'),
		'## Keep reading',
		'[Concepts](/docs/concepts/) explains how the engine fits together. ' +
			'[Guides](/docs/guides/) walks through tasks using code from the samples. ' +
			'[About the reference](/docs/reference/) lists every module and explains how these pages are generated.',
	].join('\n\n');
}
