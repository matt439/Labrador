// Optional real-browser check of the built site's Mermaid wrapper. Run with
// tests/render/web/run_page.mjs --root website/dist --page <built page>
// --input website/acceptance/diagram-input.mjs. The architecture and tick pages
// exercise all three published diagrams; run each page separately.
//
// One response-only fixture adds a mathematical label and script-shaped label
// before the real page scripts execute. It exercises the bundled KaTeX and
// sanitizer paths, not an advisory's exploit prerequisites. Production files
// are unchanged. No external image or URL is requested by the fixture.
import assert from 'node:assert/strict';

export const ready = "document.readyState === 'complete'";
export const strict_complaints = true;
const state = { injected: 0, errors: [] };
const FIXTURE = `<pre class="mermaid" id="dependency-diagram-fixture">flowchart LR
  safe["&lt;script&gt;window.diagramUnsafe=true&lt;/script&gt;&lt;b&gt;Safe label&lt;/b&gt;"] --&gt; math["$$x^2 + y^2$$"]
</pre>`;

export async function prepare(page) {
	page.on((message) => {
		if (message.method === 'Runtime.exceptionThrown') {
			console.error(JSON.stringify(message.params.exceptionDetails));
		}
		if (message.method !== 'Fetch.requestPaused') return;
		void (async () => {
			const { requestId, resourceType } = message.params;
			if (resourceType !== 'Document') {
				await page.send('Fetch.continueRequest', { requestId });
				return;
			}
			const response = await page.send('Fetch.getResponseBody', { requestId });
			const html = response.base64Encoded ? Buffer.from(response.body, 'base64').toString('utf8') : response.body;
			assert.equal(html.split('</body>').length, 2, 'fixture requires one document body');
			state.injected++;
			await page.send('Fetch.fulfillRequest', {
				requestId, responseCode: 200,
				responseHeaders: [{ name: 'Content-Type', value: 'text/html; charset=utf-8' }],
				body: Buffer.from(html.replace('</body>', () => `${FIXTURE}</body>`)).toString('base64'),
			});
		})().catch((error) => state.errors.push(String(error.stack ?? error)));
	});
	await page.send('Fetch.enable', { patterns: [{ resourceType: 'Document', requestStage: 'Response' }] });
}

export async function rendered(page, count, { replacement = false } = {}) {
	const deadline = Date.now() + 20_000;
	while (Date.now() < deadline) {
		assert.deepEqual(state.errors, []);
		const result = await page.evaluate(`(() => {
			const diagrams = [...document.querySelectorAll('pre.mermaid')];
			return diagrams.length === ${count} && diagrams.every((node) =>
				node.dataset.processed === 'true' && node.querySelector('svg .node') !== null) &&
				document.querySelector('#dependency-diagram-fixture .katex') !== null &&
				(${!replacement} || document.querySelector('pre.mermaid svg') !== window.previousDiagram);
		})()`);
		if (result) return;
		await page.wait(50);
	}
	throw new Error(`expected ${count} rendered diagrams within 20 seconds`);
}

export default async function drive(page) {
	assert.equal(state.injected, 1, 'the real document plus one response-only fixture must load');
	const pathname = await page.evaluate('location.pathname');
	const count = pathname.endsWith('/docs/design/architecture/') || pathname.endsWith('/docs/design/architecture/index.html')
		? 3 : pathname.endsWith('/docs/concepts/tick/') || pathname.endsWith('/docs/concepts/tick/index.html') ? 2 : null;
	assert.notEqual(count, null, `unexpected diagram page ${pathname}`);
	await rendered(page, count);
	for (const theme of ['light', 'dark', 'light']) {
		// The same attribute observed by the real wrapper. Wait for a new SVG,
		// so an unchanged old diagram cannot satisfy the redraw check.
		await page.evaluate(`(() => {
			window.previousDiagram = document.querySelector('pre.mermaid svg');
			document.documentElement.dataset.theme = ${JSON.stringify(theme)};
		})()`);
		await rendered(page, count, { replacement: true });
		const fixture = await page.evaluate(`(() => {
			const fixture = document.getElementById('dependency-diagram-fixture');
			return { math: fixture.querySelector('.katex') !== null,
				text: [...fixture.querySelectorAll('.nodeLabel')].map((node) => node.textContent).join(' '),
				active: fixture.querySelector('script, [onerror], [onload], a[href^="javascript:"]')?.outerHTML ?? null,
				executed: window.diagramUnsafe === true };
		})()`);
		assert.equal(fixture.math && fixture.text.includes('Safe label') && fixture.active === null && !fixture.executed,
			true, `${theme}: math must render and the script-shaped label must remain inactive: ${JSON.stringify(fixture)}`);
	}
	assert.deepEqual(state.errors, []);
	console.log(`ok - ${count - 1} published diagrams and a math/sanitizer fixture rendered in light/dark/light`);
}
