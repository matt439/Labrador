// Reads a Labrador header into the declarations and comments a reference page
// is built from.
//
// THIS IS NOT A C++ PARSER. It is a line reader that leans on the shape
// CONVENTIONS fixes for every engine header: Allman braces, one statement per
// line or a statement continued across lines, and a `//` comment directly above
// the declaration it describes. A comment separated from the next declaration by a blank line
// describes nothing below it and is kept as a free-standing note. What the
// reader does not understand it reports by file and line rather than guessing
// (PHILOSOPHY T6), so a header that outgrows it fails the site build instead
// of publishing a wrong page.
//
// The alternatives, and why this was chosen over them, are in the reference's
// own introduction page (src/content/docs/docs/reference/index.mdx).

export type Access = 'public' | 'protected' | 'private';

export interface Declaration {
	// As written, dedented, with a body reduced to `;` unless it fitted on the
	// declaration's own line.
	text: string;
	// What a heading calls it: a function or variable name, `operator=`,
	// `~Scene`, an alias.
	name: string;
	line: number;
}

export interface CommentBlock {
	// One entry per comment line, with the `// ` removed; an empty entry is a
	// blank comment line, which separates paragraphs.
	lines: string[];
	line: number;
}

export type Item =
	| { kind: 'note'; comment: CommentBlock }
	| { kind: 'forward'; keyword: string; name: string; comment?: CommentBlock; line: number }
	| {
			kind: 'group';
			access: Access;
			declarations: Declaration[];
			comment?: CommentBlock;
	  }
	| TypeItem;

export interface TypeItem {
	kind: 'type';
	keyword: string;
	name: string;
	// The declaration line, e.g. `class Scene` or `struct View`.
	head: string;
	bases: string[];
	access: Access;
	comment?: CommentBlock;
	line: number;
	// The definition as written, for a nested type small enough to show whole.
	source: string;
	items: Item[];
}

export interface HeaderDocument {
	path: string;
	includes: string[];
	namespaces: string[];
	items: Item[];
}

interface Scope {
	kind: 'file' | 'namespace' | 'type' | 'enum';
	items: Item[];
	access: Access;
	type?: TypeItem;
	startIndex: number;
}

const TYPE_HEAD =
	/^(?:template\s*<[^]*>\s*)?(class|struct|union|enum\s+class|enum\s+struct|enum)\s+([A-Za-z_]\w*)(?:\s+final)?\s*(?::\s*([^]+))?$/;

class HeaderError extends Error {
	constructor(path: string, line: number, message: string) {
		super(`${path}:${line + 1}: ${message} (website/src/lib/cpp-header.ts reads engine headers line by line; see its opening comment)`);
	}
}

// Brace and parenthesis balance of one line of code, ignoring string and
// character literals and stopping at a line comment. Returns the code part
// of the line too, with any trailing comment removed.
function scan(line: string): { code: string; trailing?: string; parens: number; braces: number } {
	let parens = 0;
	let braces = 0;
	let quote: string | null = null;
	for (let index = 0; index < line.length; index++) {
		const character = line[index];
		if (quote) {
			if (character === '\\') {
				index++;
			} else if (character === quote) {
				quote = null;
			}
			continue;
		}
		if (character === '"' || character === "'") {
			quote = character;
		} else if (character === '/' && line[index + 1] === '/') {
			return {
				code: line.slice(0, index).trimEnd(),
				trailing: line.slice(index + 2).trim(),
				parens,
				braces,
			};
		} else if (character === '(') {
			parens++;
		} else if (character === ')') {
			parens--;
		} else if (character === '{') {
			braces++;
		} else if (character === '}') {
			braces--;
		}
	}
	return { code: line.trimEnd(), parens, braces };
}

function dedent(lines: string[]): string {
	const indents = lines
		.filter((line) => line.trim() !== '')
		.map((line) => line.match(/^[\t ]*/)![0].length);
	const common = Math.min(...indents);
	return lines.map((line) => line.slice(common)).join('\n');
}

export function declarationName(text: string): string {
	const flat = text.replace(/\s+/g, ' ').trim();
	const alias = flat.match(/^using\s+([A-Za-z_]\w*)\s*=/);
	if (alias) {
		return alias[1];
	}
	const operator = flat.match(/\boperator\s*(\(\)|[^\s(]+)\s*\(/);
	if (operator) {
		return `operator${operator[1]}`;
	}
	// Skip a template parameter list, whose parentheses are not the call's.
	const body = flat.replace(/^template\s*<[^]*?>\s*/, '');
	const call = body.indexOf('(');
	const before = call < 0 ? body.replace(/\s*(=[^]*|\{[^]*|;)$/, '') : body.slice(0, call);
	const name = before.match(/(~?[A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*$/);
	return name ? name[1] : flat;
}

export function parseHeader(path: string, text: string): HeaderDocument {
	const lines = text.replace(/\r\n?/g, '\n').split('\n');
	const document: HeaderDocument = { path, includes: [], namespaces: [], items: [] };
	const stack: Scope[] = [{ kind: 'file', items: document.items, access: 'public', startIndex: 0 }];

	let pending: CommentBlock | undefined;
	let group: Extract<Item, { kind: 'group' }> | undefined;

	const scope = () => stack[stack.length - 1];
	const endGroup = () => {
		group = undefined;
	};
	const flushNote = () => {
		if (pending) {
			scope().items.push({ kind: 'note', comment: pending });
			pending = undefined;
		}
		endGroup();
	};

	let index = 0;
	while (index < lines.length) {
		const trimmed = lines[index].trim();

		if (trimmed === '') {
			flushNote();
			index++;
			continue;
		}

		if (trimmed.startsWith('//')) {
			endGroup();
			const content = trimmed.slice(2).replace(/^ /, '');
			if (pending) {
				pending.lines.push(content);
			} else {
				pending = { lines: [content], line: index };
			}
			index++;
			continue;
		}

		if (trimmed.startsWith('#')) {
			flushNote();
			const include = trimmed.match(/^#include\s+[<"]([^>"]+)[>"]/);
			if (include) {
				document.includes.push(include[1]);
			}
			index++;
			continue;
		}

		if (trimmed === '}' || trimmed === '};') {
			flushNote();
			const closing = stack.pop();
			if (!closing || closing.kind === 'file') {
				throw new HeaderError(path, index, 'a closing brace with no scope open');
			}
			if (closing.type) {
				closing.type.source = dedent(lines.slice(closing.startIndex, index + 1));
			}
			index++;
			continue;
		}

		const access = trimmed.match(/^(public|protected|private)\s*:$/);
		if (access) {
			flushNote();
			scope().access = access[1] as Access;
			index++;
			continue;
		}

		if (scope().kind === 'enum') {
			const enumerator = scan(lines[index]).code.trim().replace(/,$/, '');
			const name = enumerator.split(/[\s=,]/)[0];
			const declaration = { text: enumerator, name, line: index };
			if (pending || !group) {
				group = { kind: 'group', access: 'public', declarations: [], comment: pending };
				pending = undefined;
				scope().items.push(group);
			}
			group.declarations.push(declaration);
			index++;
			continue;
		}

		// A statement: read lines until it ends in a semicolon, ends in a body
		// that closed on its own line, or opens a body on the next line.
		const start = index;
		const statement: string[] = [];
		const trailing: string[] = [];
		let parens = 0;
		let braces = 0;
		let opensBody = false;
		for (;;) {
			if (index >= lines.length) {
				throw new HeaderError(path, start, 'a statement that never ends');
			}
			const scanned = scan(lines[index]);
			statement.push(scanned.code);
			if (scanned.trailing) {
				trailing.push(scanned.trailing);
			}
			parens += scanned.parens;
			braces += scanned.braces;
			index++;

			const code = statement.join('\n').trim();
			if (parens !== 0) {
				continue;
			}
			if (braces === 0 && (code.endsWith(';') || (code.endsWith('}') && code.includes('{')))) {
				break;
			}
			if (braces > 0) {
				// A braced initialiser continued on the next line -
				// `points_ = { a, b,` - is a value, not a body.
				if (/=\s*\{/.test(code)) {
					continue;
				}
				throw new HeaderError(path, start, 'a body opened on the declaration line; engine headers put braces on their own line');
			}
			const next = lines[index]?.trim();
			if (next === '{') {
				opensBody = true;
				index++;
				break;
			}
		}

		const text = dedent(statement);
		const head = text.replace(/\s+/g, ' ').trim();
		let comment = pending;
		pending = undefined;
		if (trailing.length > 0) {
			// A trailing comment on a declaration line is part of what the
			// declaration says.
			comment = comment
				? { line: comment.line, lines: [...comment.lines, '', ...trailing] }
				: { line: start, lines: trailing };
		}

		const namespace = head.match(/^namespace\s+([\w:]+)$/);
		if (namespace && opensBody) {
			endGroup();
			// `detail` holds internals a header must expose and a user must not
			// touch (CONVENTIONS, Namespaces), so it is read and not published.
			if (namespace[1] === 'detail') {
				stack.push({ kind: 'namespace', items: [], access: 'public', startIndex: start });
				continue;
			}
			if (comment) {
				scope().items.push({ kind: 'note', comment });
			}
			document.namespaces.push(namespace[1]);
			// A namespace adds no structure a reader needs: its contents are
			// listed as the file's.
			stack.push({ kind: 'namespace', items: scope().items, access: 'public', startIndex: start });
			continue;
		}

		const type = head.replace(/;$/, '').match(TYPE_HEAD);
		if (type && opensBody) {
			endGroup();
			const keyword = type[1].replace(/\s+/g, ' ');
			const item: TypeItem = {
				kind: 'type',
				keyword,
				name: type[2],
				head,
				bases: type[3] ? type[3].split(',').map((base) => base.trim()) : [],
				access: scope().access,
				comment,
				line: start,
				source: '',
				items: [],
			};
			scope().items.push(item);
			stack.push({
				kind: keyword.startsWith('enum') ? 'enum' : 'type',
				items: item.items,
				access: keyword === 'class' ? 'private' : 'public',
				type: item,
				startIndex: start,
			});
			continue;
		}
		if (type && !opensBody && head.endsWith(';') && !type[3]) {
			endGroup();
			scope().items.push({ kind: 'forward', keyword: type[1], name: type[2], comment, line: start });
			continue;
		}

		let declarationText = text;
		if (opensBody) {
			// A function defined in the header: the reader wants the signature,
			// not the body.
			let depth = 1;
			while (depth > 0) {
				if (index >= lines.length) {
					throw new HeaderError(path, start, 'a function body that never closes');
				}
				depth += scan(lines[index]).braces;
				index++;
			}
			declarationText = `${text};`;
		}

		const declaration: Declaration = { text: declarationText, name: declarationName(declarationText), line: start };
		if (comment || !group || group.access !== scope().access) {
			group = { kind: 'group', access: scope().access, declarations: [], comment };
			scope().items.push(group);
		}
		group.declarations.push(declaration);
	}

	if (stack.length !== 1) {
		throw new HeaderError(path, lines.length - 1, 'the file ends inside a scope');
	}
	if (pending) {
		document.items.push({ kind: 'note', comment: pending });
	}
	return document;
}
