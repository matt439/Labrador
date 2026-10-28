// Reference generators and resolvers used by the collection and Markdown
// plugins. Indexes own invalidation; prose and pages consume their lazy getters.
import type { CommentBlock } from './cpp-header.ts';
import type { ReferenceHeader, ReferenceModule } from './published.ts';
import { createReferenceIndexes } from './reference-indexes.ts';
import { createReferencePages } from './reference-pages.ts';
import { createReferenceProse } from './reference-prose.ts';
import type { Explanation, ExplanationSource, Layout, ReferencePage, SymbolTarget } from './reference-types.ts';

export { EXPLANATION_FOLDERS } from './reference-indexes.ts';
export type { Explanation, ExplanationSource, ReferencePage } from './reference-types.ts';

// Creating the services performs no index build. When an index later asks for
// layout, all services exist and the returned prose bodies remain deferred.
function layout(header: ReferenceHeader): Layout { return pages.layout(header); }
const indexes = createReferenceIndexes(layout);
const prose = createReferenceProse(indexes);
const pages = createReferencePages(indexes, prose);

export function unlistedHeaders(): string[] { return indexes.unlistedHeaders(); }
export function symbols(): Map<string, SymbolTarget> { return indexes.symbols(); }
export function explanationSources(): string[] { return indexes.explanationSources(); }
export function indexExplanations(
	pages: readonly ExplanationSource[], dependencies?: Set<string>
): Map<string, Explanation[]> { return indexes.indexExplanations(pages, dependencies); }
export function explanationDependencies(): readonly string[] { return indexes.explanationDependencies(); }
export function commentHtml(comment: CommentBlock | undefined): string { return prose.commentHtml(comment); }
export function freeFunctions(): Map<string, SymbolTarget> { return indexes.freeFunctions(); }
export function codeLink(code: string, page: string): string | undefined { return indexes.codeLink(code, page); }
export function forgetIndexes(): void { indexes.forgetIndexes(); }
export function referencePage(header: ReferenceHeader): ReferencePage { return pages.referencePage(header); }
export function referencePages(): ReferencePage[] { return pages.referencePages(); }
export function moduleReferencePage(module: ReferenceModule): string { return pages.moduleReferencePage(module); }
