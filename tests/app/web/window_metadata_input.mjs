// Exercise the actual wasm constructor and title setter, without claiming
// ownership of the host page's document title or favicon elements.
export async function check_window_metadata({ evaluate }) {
	const observed = JSON.parse(await evaluate(`(() => {
		const metadata = () => ({
			title: document.title,
			icons: Array.from(document.querySelectorAll('link[rel~="icon"]'),
				(node) => node.outerHTML),
		});
		const before = metadata();
		const originalTitle = document.title;
		try {
			document.title = 'The host controls this title';
			const host = metadata();
			Module._web_window_test_set_title();
			return JSON.stringify({
				constructed: JSON.stringify(before) === JSON.stringify(Module.labradorWindowHostMetadata),
				setter: JSON.stringify(metadata()) === JSON.stringify(host),
			});
		} finally {
			document.title = originalTitle;
		}
	})()`));
	if (!observed.constructed || !observed.setter) {
		throw new Error(`native title/icon operation changed host page metadata: ${JSON.stringify(observed)}`);
	}
}

export default async function drive(page) {
	while (await page.evaluate('typeof Module === "object" && Module.labradorWindowReady === true') !== true) {
		await page.wait(20);
	}
	await check_window_metadata(page);
	console.log('ok - browser host retains title and favicon ownership');
	await page.evaluate('Module._web_window_test_close(), true');
}
