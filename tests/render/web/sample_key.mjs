// Keep each input level observable by the page's fixed-step loop. A Node
// timer can expire while the renderer delivers no frames, so wall time alone
// does not ensure that a press or release reaches Keyboard::poll().
const INTERVAL = `new Promise((done) => {
	const started = performance.now();
	let frames = 0;
	const step = (timestamp) => {
		if (++frames >= 2 && timestamp - started >= 150) { done(true); }
		else { requestAnimationFrame(step); }
	};
	requestAnimationFrame(step);
})`;

export async function press_sample_key(page, event) {
	await page.send('Input.dispatchKeyEvent', { type: 'rawKeyDown', ...event });
	await page.evaluate(INTERVAL);
	await page.send('Input.dispatchKeyEvent', { type: 'keyUp', ...event });
	await page.evaluate(INTERVAL);
}
