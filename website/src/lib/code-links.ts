// Lexical API navigation, not C++ name resolution. Only published names are
// candidates; comments, literals and calls through an object are left alone.
import type { ExpressiveCodePlugin } from '@astrojs/starlight/expressive-code';
import { h } from '@astrojs/starlight/expressive-code/hast';
import { codeLink, freeFunctions, symbols } from './reference.ts';
import { codeReferences, type CodeReference } from './reference-mentions.ts';

export function codeLinks(code: string): CodeReference[] {
	return codeReferences(code, {
		codeLink,
		hasType: (name) => symbols().has(name),
		hasFunction: (name) => freeFunctions().has(name),
	});
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
