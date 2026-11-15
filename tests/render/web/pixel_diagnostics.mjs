// Reuse golden_image.cpp diagnostics without pretending absent records passed.
// This parser is renderer-neutral; raw output remains the source of truth.
export function pixel_diagnostics(lines) {
	const text = lines.join('\n');
	const frames = [];
	const pattern = /golden mismatch: ([^\r\n]+)\r?\n\s*(\d+) of (\d+) pixels differ by more than (\d+) \((\d+) differ at all\), worst channel by (\d+)\r?\n\s*first at \((\d+), (\d+)\)/g;
	for (const match of text.matchAll(pattern)) {
		const [beyond, pixels, allowance, differing, worst, x, y] = match.slice(2).map(Number);
		frames.push({ frame: match[1], pixels, differing, beyond, allowance, worst,
			first: {x, y} });
	}
	return {
		schema: 1,
		kind: 'golden-failure-diagnostics',
		frames,
		unparsed_mismatches: (text.match(/golden mismatch:/g) || []).length - frames.length,
		passing_frame_details: 'not emitted by comparator',
		// Neither an empty list nor a parsed mismatch is a test-suite verdict.
		suite_verdict: 'use exit protocol and page complaints',
	};
}
