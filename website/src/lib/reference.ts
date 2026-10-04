// Turns a parsed header into the Markdown of its reference page.
//
// The comment text is published as the header states it, every word: the
// reference is a view of the header, not an edit of it. What the page adds is
// structure - a heading per declaration group, the signatures as code, the
// public and protected members only - and links: a type the engine defines,
// a header path, and a trade-off number each become a link to where they are
// defined.

import { readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import GithubSlugger from 'github-slugger';
import { parseHeader, type CommentBlock, type Item, type TypeItem } from './cpp-header';
import { REFERENCE_HEADERS, UNPUBLISHED_HEADERS, publishedPages, type ReferenceHeader } from './published';
import { REPOSITORY_ROOT, REVISION, absolutePath, commitUrl, readRepositoryFile, sourceUrl } from './repository';

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
			if (!match || lines[index + 1]?.trim() !== '{') {
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
	return symbolIndex;
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

// A qualified member - `GameObject::draw`, `Camera::calculate_view_rectangle()`
// - or a header path is unambiguous anywhere in prose. A bare type name is
// linked when it is written as code, or when it is a compound PascalCase name
// that cannot be an ordinary English word at the start of a sentence: "Scene"
// and "State" are both.
function linkSymbols(escaped: string, inCode: boolean): string {
	const index = symbols();
	const pages = publishedPages();

	return escaped.replace(
		/\b(engine\/[\w/]+\.(?:h|md))\b|\b([A-Z][A-Za-z0-9]*)(::~?[a-z_]\w*(?:\(\))?)?|\b(T(?:1[0-2]|[1-9]))\b/g,
		(match, header: string | undefined, type: string | undefined, member: string | undefined, tradeOff: string | undefined) => {
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
			const target = type ? index.get(type) : undefined;
			if (!target) {
				return match;
			}
			const compound = /^[A-Z][a-z0-9]+[A-Z]/.test(type!);
			if (member || inCode || compound) {
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

// A comment block as HTML: paragraphs split at blank comment lines, `- `
// lines as a list, and lines indented past the paragraph as code. Written as
// HTML rather than Markdown because comment prose is not Markdown - a
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
			blocks.push(`<pre class="ref-comment-code"><code>${escapeHtml(paragraph.map((line) => line.replace(/^(?: {4}|\t)/, '')).join('\n'))}</code></pre>`);
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

// ------------------------------------------------------------------ the page

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
			out.push(`<div class="ref-note">${commentHtml(item.comment)}</div>`);
		} else if (item.kind === 'group' && item.comment) {
			const names = item.declarations.map((declaration) => escapeHtml(declaration.text)).join(', ');
			described.push(`<dt><code>${names}</code></dt><dd>${commentHtml(item.comment)}</dd>`);
		}
	}
	if (described.length > 0) {
		out.unshift(`<dl class="ref-enumerators">${described.join('')}</dl>`);
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
function nestedType(item: TypeItem, qualifier: string): string {
	const out: string[] = [`### ${item.name}`];
	out.push(fence(definitionWithoutComments(item)));
	out.push(commentHtml(item.comment));
	out.push(`<p class="ref-qualified">Declared as <code>${escapeHtml(qualifier)}::${item.name}</code>.</p>`);
	return out.filter(Boolean).join('\n\n');
}

function members(type: TypeItem): string {
	const out: string[] = [];
	for (const item of type.items.filter(visible)) {
		if (item.kind === 'note') {
			out.push(`<div class="ref-note">${commentHtml(item.comment)}</div>`);
		} else if (item.kind === 'type') {
			out.push(nestedType(item, type.name));
		} else if (item.kind === 'group') {
			const names = [...new Set(item.declarations.map((declaration) => declaration.name))];
			const label = item.access === 'protected' ? ' (protected)' : '';
			out.push(`### ${names.join(' · ')}${label}`);
			out.push(fence(item.declarations.map((declaration) => signature(declaration.text)).join('\n')));
			out.push(commentHtml(item.comment));
		}
	}
	return out.filter(Boolean).join('\n\n');
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

export interface ReferencePage {
	header: ReferenceHeader;
	markdown: string;
	description: string;
}

export function referencePage(header: ReferenceHeader): ReferencePage {
	const document = parseHeader(header.source, readRepositoryFile(header.source));
	const types = document.items.filter((item): item is TypeItem => item.kind === 'type');
	const own = new Set(types.map((type) => type.name));
	const namespace = document.namespaces.join('::');

	const out: string[] = [];
	out.push(
		`<p class="ref-source">Generated from <a href="${sourceUrl(header.source)}"><code>${escapeHtml(header.source)}</code></a> ` +
			`at <a href="${commitUrl()}"><code>${REVISION.short}</code></a>. The text under each declaration is the ` +
			`header's own comment, word for word. <a href="/docs/reference/">About the reference</a> says how these ` +
			`pages are made.</p>`
	);
	out.push(
		`<p class="ref-include"><code>#include "${escapeHtml(header.source)}"</code>` +
			(namespace ? ` · namespace <code>${escapeHtml(namespace)}</code>` : '') +
			`</p>`
	);

	for (const item of document.items) {
		if (item.kind === 'note') {
			out.push(`<div class="ref-note">${commentHtml(item.comment)}</div>`);
		}
		if (item.kind === 'type') {
			out.push(`## ${typeHeading(item)}`);
			if (item.keyword.startsWith('enum')) {
				out.push(fence(definitionWithoutComments(item)));
				out.push(commentHtml(item.comment));
				out.push(enumerators(item));
			} else {
				out.push(fence(typeSignature(item)));
				out.push(commentHtml(item.comment));
				out.push(members(item));
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
			out.push(`## ${names.join(' · ')}`);
			out.push(fence(declarations.map((declaration) => signature(declaration.text)).join('\n')));
			out.push(commentHtml(item.comment));
		}
	}

	const used = typesUsed(document, own);
	if (used.length > 0) {
		const index = symbols();
		out.push('## Related types');
		out.push(
			`<p>Named in the public declarations above: ${used
				.map((name) => link(index.get(name)!.url, `<code>${name}</code>`))
				.join(', ')}.</p>`
		);
	}

	const primary = types[0];
	const description = primary
		? `${typeHeading(primary)}, from ${header.source}.`
		: `${header.source}.`;
	return { header, markdown: out.filter(Boolean).join('\n\n'), description };
}

export function referencePages(): ReferencePage[] {
	return REFERENCE_HEADERS.map(referencePage);
}
