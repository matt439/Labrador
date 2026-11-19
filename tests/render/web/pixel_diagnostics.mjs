// Parse golden_image.cpp's successful comparisons and failure diagnostics.
export function pixel_diagnostics(lines) {
	const text = lines.join('\n');
	const frames = [];
	const pattern = /golden mismatch: ([^\r\n]+)\r?\n\s*(\d+) of (\d+) pixels differ by more than (\d+) \((\d+) differ at all\), worst channel by (\d+)\r?\n\s*first at \((\d+), (\d+)\)/g;
	for (const match of text.matchAll(pattern)) {
		const [beyond, pixels, allowance, differing, worst, x, y] = match.slice(2).map(Number);
		frames.push({ frame: match[1], pixels, differing, beyond, allowance, worst,
			first: {x, y} });
	}
	const passing_frames = [];
	const pass_pattern = /^golden pass: ([a-z0-9-]+), (\d+) pixels, (\d+) differing, allowance (\d+), worst channel (\d+)\r?$/gm;
	for (const match of text.matchAll(pass_pattern)) {
		const [pixels, differing, allowance, worst] = match.slice(2).map(Number);
		passing_frames.push({ frame: match[1], pixels, differing, beyond: 0, allowance, worst });
	}
	return {
		schema: 2,
		kind: 'golden-comparison-diagnostics',
		frames,
		unparsed_mismatches: (text.match(/golden mismatch:/g) || []).length - frames.length,
		passing_frames,
		unparsed_passes: (text.match(/^golden pass:/gm) || []).length - passing_frames.length,
		// Neither an empty list nor a parsed mismatch is a test-suite verdict.
		suite_verdict: 'use exit protocol and page complaints',
	};
}
