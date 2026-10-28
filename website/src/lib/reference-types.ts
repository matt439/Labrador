// Shared shapes keep the index, prose and page modules independent.
import type { CommentBlock } from './cpp-header.ts';
import type { ReferenceHeader, ReferenceModule } from './published.ts';

export interface SymbolTarget {
	url: string;
	header: string;
}

export interface ExplanationSource {
	source: string;
	title: string;
	url: string;
	markdown: string;
}

export interface Explanation {
	title: string;
	url: string;
}

export interface Heading {
	depth: 2 | 3;
	text: string;
	owner?: string;
	names?: string[];
	functions?: string[];
}

// Bodies stay deferred: indexes inspect headings without rendering prose that
// needs those indexes. The facade supplies the lazy layout callback.
export type Block = Heading | (() => string);

export interface Layout {
	blocks: Block[];
	description: string;
}

export interface ReferencePage {
	header: ReferenceHeader;
	markdown: string;
	description: string;
}

export interface ReferenceIndexes {
	unlistedHeaders(): string[];
	symbols(): Map<string, SymbolTarget>;
	explanationSources(): string[];
	indexExplanations(pages: readonly ExplanationSource[], dependencies?: Set<string>): Map<string, Explanation[]>;
	explanationDependencies(): readonly string[];
	explanations(): Map<string, Explanation[]>;
	includers(): Map<string, string[]>;
	tradeOffAnchors(): Map<string, string>;
	freeFunctions(): Map<string, SymbolTarget>;
	memberAnchors(): Map<string, string>;
	codeLink(code: string, page: string): string | undefined;
	forgetIndexes(): void;
}

export interface ReferenceProse {
	commentHtml(comment: CommentBlock | undefined): string;
	noteHtml(comment: CommentBlock): string;
}

export interface ReferencePages {
	layout(header: ReferenceHeader): Layout;
	referencePage(header: ReferenceHeader): ReferencePage;
	referencePages(): ReferencePage[];
	moduleReferencePage(module: ReferenceModule): string;
}
