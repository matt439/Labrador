// Declaration layout and page assembly. Only body evaluation formats comments
// or discovers guide return links; anchor collection reads the headings alone.
import { parseHeader, type Declaration, type Item, type TypeItem } from './cpp-header.ts';
import { REFERENCE_AUDIENCES, REFERENCE_HEADERS, validateReferenceAudiences, type ReferenceHeader, type ReferenceModule } from './published.ts';
import { REVISION, commitUrl, readRepositoryFile, sourceUrl } from './repository.ts';
import { escapeHtml, fence, link } from './reference-prose.ts';
import { sitePath } from './site-path.ts';
import type { Block, Layout, ReferenceIndexes, ReferencePage, ReferencePages, ReferenceProse } from './reference-types.ts';

export function createReferencePages(indexes: ReferenceIndexes, prose: ReferenceProse): ReferencePages {
	const { symbols, includers, explanations } = indexes;
	const { commentHtml, noteHtml } = prose;

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
				const constants = names.length > 1 && item.declarations.every((declaration) =>
					/^static\s+(?:const|constexpr)\s/.test(declaration.text));
				if (constants) {
					blocks.push({ depth: 3, text: `Constants${label}`, owner: type.name, names });
					blocks.push(() => commentHtml(item.comment));
					blocks.push(() => item.declarations.map((declaration) =>
						`- \`${signature(declaration.text)}\``).join('\n'));
					continue;
				}
				// Fields have separate link targets even when they share one comment.
				// Parentheses after an initialiser belong to its value, not a method.
				const fields = item.declarations.every((declaration) =>
					!declaration.name.startsWith('operator') &&
					(!declaration.text.includes('(') || /^[^()]*[={]/.test(declaration.text)));
				const groups = fields ? item.declarations.map((declaration) => [declaration]) : [item.declarations];
				for (const declarations of groups) {
					const groupNames = [...new Set(declarations.map((declaration) => declaration.name))];
					blocks.push({ depth: 3, text: `${groupNames.join(' · ')}${label}`, owner: type.name, names: groupNames });
					blocks.push(() => fence(declarations.map((declaration) => signature(declaration.text)).join('\n')));
				}
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

	function layout(header: ReferenceHeader): Layout {
		validateReferenceAudiences([header]);
		const document = parseHeader(header.source, readRepositoryFile(header.source));
		const types = document.items.filter((item): item is TypeItem => item.kind === 'type');
		const own = new Set(types.map((type) => type.name));
		const namespace = document.namespaces.join('::');

		const blocks: Block[] = [];
		blocks.push(
			() =>
				`<p class="ref-source">Generated from <a href="${sourceUrl(header.source)}"><code>${escapeHtml(header.source)}</code></a> ` +
				`at <a href="${commitUrl()}"><code>${REVISION.short}</code></a>. The text under each declaration is the ` +
				`header's own comment, word for word. <a href="${sitePath('/docs/reference/')}">About the reference</a> says how these ` +
				`pages are made.</p>`
		);
		blocks.push(
			() => `<p><a href="${sitePath(`/docs/reference/${header.module}/`)}">Browse the ${header.module} module</a></p>`
		);
		blocks.push(() => `<p class="ref-audience"><strong>Audience: ${header.audience}.</strong>` +
			(header.source === 'engine/render/resource_factory.h'
				? ' Game code can use <code>white_texture_name</code>; the factories are renderer implementation helpers.'
				: '') + '</p>');
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
		blocks.push({ depth: 2, text: 'Explained in' });
		blocks.push(() => explainedIn(header));

		const primary = types[0];
		const description = primary
			? `${typeHeading(primary)}, from ${header.source}.`
			: `${header.source}.`;
		return { blocks, description };
	}

	function explainedIn(header: ReferenceHeader): string {
		const pages = explanations().get(`/${header.slug}/`) ?? [];
		return pages.length > 0
			? `<ul>${pages.map((page) => `<li>${link(page.url, escapeHtml(page.title))}</li>`).join('')}</ul>`
			: '<p>No guide or concept page links to this header yet.</p>';
	}

	function referencePage(header: ReferenceHeader): ReferencePage {
		const { blocks, description } = layout(header);
		const markdown = blocks
			.map((block) => (typeof block === 'function' ? block() : `${'#'.repeat(block.depth)} ${block.text}`))
			.filter(Boolean)
			.join('\n\n');
		return { header, markdown, description };
	}

	function referencePages(): ReferencePage[] {
		return REFERENCE_HEADERS.map(referencePage);
	}

	// Module lists use the same inventory as the sidebar and the header pages, so
	// adding a published header also makes it discoverable from its module.
	function moduleReferencePage(module: ReferenceModule): string {
		validateReferenceAudiences();
		const headers = REFERENCE_HEADERS.filter((header) => header.module === module.name);
		const groups = REFERENCE_AUDIENCES.flatMap((audience) => {
			const rows = headers.filter((header) => header.audience === audience).map((header) => {
				const file = header.source.slice(header.source.lastIndexOf('/') + 1);
				return `| [${header.title}](/${header.slug}/) | \`${file}\` |`;
			});
			return rows.length === 0 ? [] : [
				`## ${audience === 'game code' ? 'Game code' : 'Renderer writers'}`,
				['| Reference page | Header |', '| --- | --- |', ...rows].join('\n'),
			];
		});
		return [
			module.summary,
			`The public headers in \`engine/${module.name}/\`. Choose a page for its declarations, ` +
				`contracts, related types, and links to the samples and tests that include it.`,
			...groups,
			'## Keep reading',
			'[Concepts](/docs/concepts/) explains how the engine fits together. ' +
				'[Guides](/docs/guides/) walks through tasks using code from the samples. ' +
				'[About the reference](/docs/reference/) lists every module and explains how these pages are generated.',
		].join('\n\n');
	}
	return { layout, referencePage, referencePages, moduleReferencePage };
}
