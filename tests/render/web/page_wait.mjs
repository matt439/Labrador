// Waiting on a page for a condition, for run_page.mjs's input drivers and the
// steps bundle_page.mjs shares with the WebDriver runners. A wait is bounded
// in milliseconds, not attempts, so the same step has the same allowance
// whichever runner's evaluations it pays for.

// What an evaluation fails with when the page it ran in was replaced by a
// navigation or a reload while it ran: DevTools' wordings for a destroyed
// execution context, a context it can no longer find, and a target that
// navigated.
export const NAVIGATION_ERROR =
	/Execution context was destroyed|Cannot find (?:default execution )?context|Inspected target navigated/;

// Evaluates `expression` until `done` holds for its value, every interval_ms,
// and answers that value; once timeout_ms has passed without it, throws
// `message`. `check`, run before each evaluation, may end the wait by
// throwing. With `navigating`, an evaluation that fails with
// NAVIGATION_ERROR counts as not yet; any other failure ends the wait.
export async function wait_for(evaluate, expression, { timeout_ms, wait, done = (value) => value === true,
	interval_ms = 20, navigating = false, check = () => {}, message = `waiting for ${expression}` } = {}) {
	if (!Number.isFinite(timeout_ms) || timeout_ms <= 0) throw new Error('wait_for needs a timeout in milliseconds.');
	const deadline = Date.now() + timeout_ms;
	for (;;) {
		check();
		try {
			const value = await evaluate(expression);
			if (done(value)) return value;
		} catch (error) {
			if (!navigating || !NAVIGATION_ERROR.test(error.message)) throw error;
		}
		if (Date.now() >= deadline) throw new Error(`${message} (after ${timeout_ms} ms)`);
		await wait(interval_ms);
	}
}
