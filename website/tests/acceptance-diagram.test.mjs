import assert from 'node:assert/strict';
import test from 'node:test';
import { rendered } from '../acceptance/diagram-input.mjs';

test('the diagram wait tolerates an old complete SVG until the async redraw replaces it', async () => {
	const previous = {};
	const next = {};
	let reads = 0;
	let waits = 0;
	const page = {
		async evaluate(expression) {
			// Run the actual DOM predicate. A fully rendered old diagram remains
			// visible for two replies while the wrapper awaits its dynamic import.
			const current = ++reads < 3 ? previous : next;
			const diagram = { dataset: { processed: 'true' }, querySelector: () => ({}) };
			const document = {
				querySelectorAll: () => [diagram, diagram],
				querySelector: (selector) => selector === 'pre.mermaid svg' ? current : {},
			};
			return Function('document', 'window', `return ${expression}`)(document, { previousDiagram: previous });
		},
		async wait() { waits++; },
	};
	await rendered(page, 2, { replacement: true });
	assert.equal(reads, 3);
	assert.equal(waits, 2);
});
