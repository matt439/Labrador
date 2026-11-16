// Exercise the engine's actual wasm query through a page-local setting fixture.
// Shared with WebWindowTests; this focused input also runs on its own page.
export async function check_motion_preference({ evaluate }) {
	const observed = JSON.parse(await evaluate(`(() => {
		const original = globalThis.matchMedia;
		const values = [];
		const queries = [];
		try {
			for (const reduced of [false, true, false]) {
				globalThis.matchMedia = (query) => {
					queries.push(query);
					return { matches: reduced };
				};
				values.push(Module._web_window_test_motion_preference());
			}
			globalThis.matchMedia = undefined;
			values.push(Module._web_window_test_motion_preference());
			globalThis.matchMedia = () => { throw new Error('setting unavailable'); };
			values.push(Module._web_window_test_motion_preference());
			return JSON.stringify({ values, queries });
		} finally {
			globalThis.matchMedia = original;
		}
	})()`));
	const expected = {
		values: [0, 1, 0, 0, 0],
		queries: Array(3).fill('(prefers-reduced-motion: reduce)'),
	};
	if (JSON.stringify(observed) !== JSON.stringify(expected)) {
		throw new Error(`motion preference: expected ${JSON.stringify(expected)}, received ${JSON.stringify(observed)}`);
	}
}

export default async function drive(page) {
	while (await page.evaluate('typeof Module === "object" && Module.labradorWindowReady === true') !== true) {
		await page.wait(20);
	}
	await check_motion_preference(page);
	console.log('ok - motion preference values, refresh and unavailable fallback');
	await page.evaluate('Module._web_window_test_close(), true');
}
