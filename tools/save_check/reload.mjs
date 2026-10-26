// run_page.mjs --input: prove that C++ reads an IDBFS save after a page reload.
import assert from 'node:assert/strict';

export default async function check({ evaluate, send, wait }) {
    const until = async (expression) => {
        for (let attempt = 0; attempt < 250; ++attempt) {
            try {
                if (await evaluate(expression) === true) return;
            } catch (error) {
                if (!/Execution context was destroyed|Cannot find context|Inspected target navigated/.test(error.message)) {
                    throw error;
                }
            }
            await wait(20);
        }
        throw new Error(`Timed out waiting for ${expression}`);
    };

    await until('typeof Module !== "undefined" && Module.settingsReady === true');
    assert.deepEqual(await evaluate('Module.loadedSettings'), {});
    assert.equal(await evaluate('Module._save_volume(37)'), 0);
    await evaluate('Module.flushSave().then(() => true)');
    await send('Page.reload', { ignoreCache: true });
    await until('typeof Module !== "undefined" && Module.settingsReady === true && Module.loadedSettings.volume === 37');
    assert.deepEqual(await evaluate('Module.loadedSettings'), { version: 1, volume: 37 });
}
