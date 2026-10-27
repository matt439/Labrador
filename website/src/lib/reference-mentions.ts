// The API links a source page will render, read before reference pages render.
// Prose and C++ use the same resolvers as their Markdown and highlighting plugins.
import { mdxToMdast, type MdastNode } from 'satteri';
import { sourceLanguage } from './excerpt.ts';

export interface CodeReference {
	start: number;
	end: number;
	url: string;
}

export interface ReferenceResolver {
	codeLink(code: string, page: string): string | undefined;
	hasType(name: string): boolean;
	hasFunction(name: string): boolean;
}

const TOKENS = /\/\/[^\n]*|\/\*[\s\S]*?(?:\*\/|$)|(?:u8|u|U|L)?R"([^\s()\\]{0,16})\([\s\S]*?\)\1"|(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*"|\b\d[\w'.]*|(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\\n])*'|(?:::)?[A-Za-z_]\w*(?:::(?:~?[A-Za-z_]\w*))*/g;

// Lexical links only: comments, literals, object calls and other namespaces stay plain.
export function codeReferences(code: string, resolver: ReferenceResolver): CodeReference[] {
	const links: CodeReference[] = [];
	const linked = new Set<string>();
	let context = '';
	let end = 0;
	for (const match of code.matchAll(TOKENS)) {
		const token = match[0];
		context += code.slice(end, match.index);
		end = match.index + token.length;
		if (token.startsWith('//') || token.startsWith('/*')) continue;
		const before = context.trimEnd();
		context += token;
		if (!/^(?:::)?[A-Za-z_]\w*(?:::(?:~?[A-Za-z_]\w*))*$/.test(token)) continue;
		const name = token.replace(/^::/, '');
		if (before.endsWith('.') || before.endsWith('->') || before.endsWith('::')) continue;
		const unqualified = name.replace(/^labrador::/, '');
		const type = unqualified.split('::')[0];
		const isType = resolver.hasType(type) && (!name.includes('::') || name === unqualified || /^labrador::/.test(name));
		const isFunction = resolver.hasFunction(name) && /^\s*(?:<[^;{}]*>)?\s*\(/.test(code.slice(match.index + token.length));
		if (!isType && !isFunction) continue;
		const url = resolver.codeLink(name, 'C++ code block');
		if (!url || linked.has(url)) continue;
		linked.add(url);
		links.push({ start: match.index, end: match.index + token.length, url });
	}
	return links;
}

const CPP = new Set(['cpp', 'c++', 'cxx', 'cc', 'h', 'hpp']);
const REFERENCE_PATH = /^\/docs\/reference\/[^/]+\/[^/]+\//;

export type ExcerptReader = (source: string, from?: string, to?: string) => string;

// Only links that the authored page renders count. An API name in an import,
// heading, comment, literal or external link label cannot create an explanation.
export function referenceMentions(
	markdown: string, page: string, resolver: ReferenceResolver, readExcerpt?: ExcerptReader
): Set<string> {
	const tree = mdxToMdast(markdown.replace(/^---\r?\n[^]*?\r?\n---\r?\n/, ''), { position: false });
	const targets = new Set<string>();
	const definitions = new Map<string, string>();
	const add = (url: string | undefined) => {
		if (url && REFERENCE_PATH.test(url)) targets.add(url.split('#')[0]);
	};
	const collectDefinitions = (node: MdastNode) => {
		if (node.type === 'definition') definitions.set(node.identifier.toLowerCase(), node.url);
		if ('children' in node) node.children.forEach(collectDefinitions);
	};
	collectDefinitions(tree);
	const code = (text: string) => codeReferences(text, resolver).forEach((reference) => add(reference.url));
	const visit = (node: MdastNode, inHeading = false, inLink = false) => {
		if (node.type === 'link') add(node.url);
		if (node.type === 'linkReference') add(definitions.get(node.identifier.toLowerCase()));
		if (node.type === 'inlineCode' && !inHeading && !inLink) add(resolver.codeLink(node.value, page));
		if (node.type === 'code' && CPP.has(node.lang ?? '')) code(node.value);
		let jsxLink = false;
		if (node.type === 'mdxJsxFlowElement' || node.type === 'mdxJsxTextElement') {
			const attributes = new Map(node.attributes.flatMap((attribute) =>
				attribute.type === 'mdxJsxAttribute' && typeof attribute.value === 'string'
					? [[attribute.name, attribute.value] as const] : []));
			jsxLink = /^a$|Link$/.test(node.name ?? '');
			add(attributes.get('href'));
			if (node.name === 'SourceFile' && readExcerpt && attributes.has('path')) {
				const source = attributes.get('path')!;
				const language = attributes.get('lang') ?? sourceLanguage(source);
				if (CPP.has(language)) code(readExcerpt(source, attributes.get('from'), attributes.get('to')));
			}
		}
		if ('children' in node) node.children.forEach((child) => visit(child,
			inHeading || node.type === 'heading',
			inLink || node.type === 'link' || node.type === 'linkReference' || jsxLink));
	};
	visit(tree);
	return targets;
}
