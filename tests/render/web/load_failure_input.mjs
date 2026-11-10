// A development page whose .wasm cannot be fetched, as
// MinimalLoadFailurePageTests: tests/render/web/run_page.mjs --input runs it
// against the minimal sample's built page in Chromium, with every request for
// a .wasm answered 404 over the DevTools protocol before it reaches the
// server.
//
// What it shows is that the page tells the player: the engine never runs, so
// Module.onError is never called, and the status line is replaced by the
// page's own listeners (samples/minimal/web/shell.html). It also holds that
// the page swallowed nothing, because Emscripten's "Aborted(" line still
// reaches the console, which is what fails every other page entry.
//
// The six development pages carry the same lines and this drives one of them,
// the template. It does not cover a loader script or a preloaded .data file
// that fails: no sample here has a .data file, and those two reach the page
// as an error event and a rejected promise rather than through Module.onAbort.

const FAILED = 'The game could not be loaded. Check your connection and reload the page.';
const WAIT_MS = 30000;

const state = { refused: 0, interception_errors: [], diagnostics: [] };

export const ready = "document.getElementById('status') !== null";

// What the refused download makes the browser and Emscripten's loader say.
// The run is not strict, so recognising these fails nothing either way; they
// are kept so that the drive below can require the loader's own report.
export function expected_complaint({ source, level, text }) {
	const expected = level === 'error' && (
		(source === 'browser' && /Failed to load resource/.test(text)) ||
		(source === 'console' &&
			/^(wasm streaming compile failed:|falling back to ArrayBuffer instantiation$|failed to asynchronously prepare wasm:|Aborted\()/.test(text)) ||
		(source === 'exception' && /Aborted\(/.test(text)));
	if (expected) { state.diagnostics.push({ source, text }); }
	return expected;
}

export async function prepare(page) {
	page.on((message) => {
		if (message.method !== 'Fetch.requestPaused') { return; }
		state.refused++;
		page.send('Fetch.fulfillRequest', {
			requestId: message.params.requestId,
			responseCode: 404,
			responseHeaders: [{ name: 'Content-Type', value: 'text/plain' }],
			body: Buffer.from('not found').toString('base64'),
		}).catch((error) => {
			state.interception_errors.push(error.stack ?? String(error));
		});
	});
	await page.send('Fetch.enable', { patterns: [{ urlPattern: '*.wasm*', requestStage: 'Request' }] });
}

function assert(condition, message) {
	if (!condition) { throw new Error(message); }
}

export default async function drive(page) {
	const aborted = () => state.diagnostics.some(({ source, text }) =>
		source === 'console' && text.startsWith('Aborted('));

	let deadline = Date.now() + WAIT_MS;
	let shown = await page.evaluate("document.getElementById('status').textContent");
	while (shown !== FAILED) {
		assert(state.interception_errors.length === 0, state.interception_errors.join('\n'));
		assert(Date.now() < deadline,
			`the status line read "${shown}" ${WAIT_MS / 1000} s after the page loaded, ` +
			`with ${state.refused} .wasm request(s) answered 404`);
		await page.wait(50);
		shown = await page.evaluate("document.getElementById('status').textContent");
	}
	assert(state.refused > 0, 'the failure line was shown, but no .wasm request was answered 404');
	assert(await page.evaluate("document.getElementById('status').className") === 'error',
		'the failure line is not styled as an error');
	console.log(`ok - a .wasm answered 404 replaces the loading line with "${FAILED}"`);

	// The console event for a line written before the status changed may
	// still be on its way to this process.
	deadline = Date.now() + WAIT_MS;
	while (!aborted()) {
		assert(Date.now() < deadline,
			'the page showed its failure line, but no "Aborted(" error reached the console; heard: ' +
			JSON.stringify(state.diagnostics));
		await page.wait(50);
	}
	console.log('ok - the loader\'s "Aborted(" error still reaches the console');
}
