// A passage of a repository file, located by its text rather than by line
// numbers.
//
// A page quoting `samples/minimal/main.cpp` names the first and last lines it
// wants. If the file changes so that either line is gone or is no longer
// unique, the site build fails naming the file and the line, instead of
// quietly quoting whatever now sits at lines 18 to 40 (PHILOSOPHY T6).

import { readRepositoryFile } from './repository';

export interface Excerpt {
	code: string;
	// 1-based and inclusive, for linking to the passage on GitHub.
	start: number;
	end: number;
	whole: boolean;
}

function indentation(line: string): number {
	return line.match(/^\s*/)![0].length;
}

// The end of a passage that closes a brace is the brace at the start line's
// own depth: `to="}"` after `from="int main"` means the end of main, not the
// end of the first block inside it.
function find(lines: string[], text: string, from: number, path: string, role: string): number {
	const depth = text.startsWith('}') ? indentation(lines[from]) : Infinity;
	const matches: number[] = [];
	for (let index = from; index < lines.length; index++) {
		if (lines[index].trim().startsWith(text) && indentation(lines[index]) <= depth) {
			matches.push(index);
		}
	}
	if (matches.length === 0) {
		throw new Error(`An excerpt of '${path}' ${role} at a line starting '${text}', and there is none.`);
	}
	// The end of a passage is the first match after its start; only the start
	// has to be unique, because it is the one a reader cannot see.
	if (role === 'starts' && matches.length > 1) {
		throw new Error(
			`An excerpt of '${path}' starts at a line starting '${text}', and ${matches.length} lines do ` +
				`(${matches.map((index) => index + 1).join(', ')}). Quote more of the line.`
		);
	}
	return matches[0];
}

function dedent(lines: string[]): string {
	const indents = lines.filter((line) => line.trim() !== '').map((line) => line.match(/^\t*/)![0].length);
	const common = indents.length > 0 ? Math.min(...indents) : 0;
	return lines.map((line) => line.slice(common)).join('\n');
}

export function excerpt(path: string, from?: string, to?: string): Excerpt {
	const lines = readRepositoryFile(path).replace(/\n$/, '').split('\n');
	if (!from && !to) {
		return { code: lines.join('\n'), start: 1, end: lines.length, whole: true };
	}
	const start = from ? find(lines, from, 0, path, 'starts') : 0;
	const end = to ? find(lines, to, start, path, 'ends') : lines.length - 1;
	return { code: dedent(lines.slice(start, end + 1)), start: start + 1, end: end + 1, whole: false };
}
