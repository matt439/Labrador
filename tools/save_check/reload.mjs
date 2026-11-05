// run_page.mjs --input: prove that C++ reads an IDBFS save after a page reload.
import assert from 'node:assert/strict';
import { wait_for } from '../../tests/render/web/page_wait.mjs';

// This driver owns completion across a reload. The original page's exit
// promise disappears with its execution context and must not end the run.
export const ready = 'typeof Module === "object" && Module.settingsReady === true';
export const strict_complaints = true;

export default async function check({ evaluate, send, wait }) {
	// A reload replaces the page while it is asked, so a navigation's
	// failure counts as not yet.
	const until = (expression) => wait_for(evaluate, expression,
		{ timeout_ms: 15000, navigating: true, wait, message: `Timed out waiting for ${expression}` });

	await until('typeof Module !== "undefined" && Module.settingsReady === true');
	assert.deepEqual(await evaluate('Module.loadedSettings'), {});
	assert.equal(await evaluate('Module._save_volume(37)'), 0);
	await evaluate('Module.flushSave().then(() => true)');
	await send('Page.reload', { ignoreCache: true });
	await until('typeof Module !== "undefined" && Module.settingsReady === true && Module.loadedSettings.volume === 37');
	assert.deepEqual(await evaluate('Module.loadedSettings'), { version: 1, volume: 37 });
}
