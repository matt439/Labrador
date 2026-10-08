// Lexical API navigation, not C++ name resolution. Only published names are
// candidates; comments, literals and calls through an object are left alone.
import type { ExpressiveCodePlugin } from '@astrojs/starlight/expressive-code';
import { h } from '@astrojs/starlight/expressive-code/hast';
import { codeLink, freeFunctions, symbols } from './reference.ts';

interface CodeLink {
	start: number;
	end: number;
	url: string;
}

// Read whole tokens so another library's ns::Scene cannot become a link to
// Labrador's Scene. Raw strings and block comments can span multiple lines.
const TOKENS = /\/\/[^\n]*|\/\*[\s\S]*?(?:\*\/|$)|(?:u8|u|U|L)?R"([^\s()\\]{0,16})\([\s\S]*?\)\1"|(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*"|\b\d[\w'.]*|(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\\n])*'|(?:::)?[A-Za-z_]\w*(?:::(?:~?[A-Za-z_]\w*))*/g;

export function codeLinks(code: string): CodeLink[] {
	const links: CodeLink[] = [];
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
		const unqualified = name.replace(/^(?:labrador|mattmath)::/, '');
		const type = unqualified.split('::')[0];
		const isType = symbols().has(type) && (!name.includes('::') || name === unqualified || /^(?:labrador|mattmath)::/.test(name));
		const isFunction = freeFunctions().has(name) && /^\s*(?:<[^;{}]*>)?\s*\(/.test(code.slice(match.index + token.length));
		if (!isType && !isFunction) continue;
		const url = codeLink(name, 'C++ code block');
		if (!url || linked.has(url)) continue;
		linked.add(url);
		links.push({ start: match.index, end: match.index + token.length, url });
	}
	return links;
}

export const apiCodeLinks: ExpressiveCodePlugin = {
	name: 'labrador-api-links',
	hooks: {
		annotateCode({ codeBlock }) {
			if (!['cpp', 'c++', 'cxx', 'cc', 'h', 'hpp'].includes(codeBlock.language)) return;
			const links = codeLinks(codeBlock.code);
			let offset = 0;
			for (const line of codeBlock.getLines()) {
				for (const link of links.filter((link) => link.start >= offset && link.end <= offset + line.text.length)) {
					line.addAnnotation({
						name: 'api-link',
						inlineRange: { columnStart: link.start - offset, columnEnd: link.end - offset },
						renderPhase: 'latest',
						render: ({ nodesToTransform }) => nodesToTransform.map((node) =>
							h('a', { href: link.url, className: ['api-link'] }, node)),
					});
				}
				offset += line.text.length + 1;
			}
		},
	},
};
