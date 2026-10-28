// Header comments become escaped prose, examples and links. No caches live here.
import type { CommentBlock } from './cpp-header.ts';
import { publishedPages } from './published.ts';
import { sourceUrl } from './repository.ts';
import { readableFile } from './reference-indexes.ts';
import { sitePath } from './site-path.ts';
import type { ReferenceIndexes, ReferenceProse } from './reference-types.ts';

export function escapeHtml(text: string): string {
	return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

export function link(url: string, html: string): string {
	return `<a href="${sitePath(url)}">${html}</a>`;
}

export function fence(code: string): string {
	return '```cpp\n' + code + '\n```';
}

export function createReferenceProse(indexes: ReferenceIndexes): ReferenceProse {
	const { symbols, freeFunctions, memberAnchors, tradeOffAnchors } = indexes;

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
	function commentHtml(comment: CommentBlock | undefined): string {
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
	return { commentHtml, noteHtml };
}
