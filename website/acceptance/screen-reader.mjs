// Whether a screen reader speaks the arrival notice: NVDA, through
// @guidepup/guidepup, listening while Chrome (or Edge) switches from
// Development's fixture-only guide to v0.3.0, which lacks it. The overview
// that opens fills its empty role=status region after load
// (Banner.astro), and this passes when something NVDA says after the switch
// is that notice: it says the page is "not part of" the version and names
// the guide's path.
//
// This runs only on a Windows CI runner, never on a developer's machine:
// NVDA speaks aloud and takes over the keyboard. It refuses to start unless
// CI is set. The runner prepares NVDA first:
//   npm install --no-save @guidepup/guidepup @guidepup/setup
//   npx guidepup setup --ci
//   npx guidepup install nvda
//
// Usage: node acceptance/screen-reader.mjs --serve-dist <dist>
//   [--browser chrome|edge] [--driver <path>] [--binary <path>] [--screenshot <file>]
//
// The browser is a visible window, because NVDA reads only the foreground
// window. WebDriver's Tab and Enter key actions reach the selector's v0.3.0
// link. The Enter that follows it is pressed through NVDA, as a reader's own
// key press, once NVDA reports the browser as the foreground window.
//
// Guidepup records speech only while one of its own commands runs: it hears
// what the command makes NVDA say until NVDA has been quiet for a second, and
// drops anything said between commands. So the Enter is pressed through it
// with full capture, which hears NVDA read the new page as it opens, notice
// included. Everything heard is printed; --screenshot saves the final
// viewport.
import { existsSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { killDriver, startDriver } from './browsers.mjs';
import { startServer } from './server.mjs';
import { Keys, newSession, pressKeys } from './webdriver.mjs';

const START = 'docs/guides/fixture-only/';
const TARGET_LABEL = 'v0.3.0';
const FOREGROUND_ATTEMPTS = 10;

const BROWSERS = {
	chrome: { driver: 'chromedriver', folderVariable: 'CHROMEWEBDRIVER', browserName: 'chrome', options: 'goog:chromeOptions', application: 'chrome' },
	edge: { driver: 'msedgedriver', folderVariable: 'EDGEWEBDRIVER', browserName: 'MicrosoftEdge', options: 'ms:edgeOptions', application: 'edge' },
};

function parseArguments(argv) {
	const options = { browser: 'chrome' };
	const valued = new Set(['--serve-dist', '--browser', '--driver', '--binary', '--screenshot']);
	for (let index = 0; index < argv.length; index += 2) {
		if (!valued.has(argv[index]) || argv[index + 1] === undefined) throw new Error(`Unexpected argument ${argv[index]}.`);
		options[argv[index].slice(2)] = argv[index + 1];
	}
	if (!options['serve-dist']) throw new Error('--serve-dist <dist> is required.');
	if (!BROWSERS[options.browser]) throw new Error('--browser must be chrome or edge.');
	return options;
}

function driverPath(entry, explicit) {
	if (explicit) return explicit;
	const folder = process.env[entry.folderVariable];
	const candidate = folder && path.join(folder, `${entry.driver}.exe`);
	return candidate && existsSync(candidate) ? candidate : `${entry.driver}.exe`;
}

const delay = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

// What NVDA said, reduced to letters and digits, so that "/docs/" and
// "slash docs slash" compare alike whatever its punctuation level.
export function normalisePhrase(phrase) {
	return String(phrase).toLowerCase()
		.replace(/\b(slash|dash|hyphen|dot|point)\b/g, ' ')
		.replace(/[^a-z0-9]+/g, '');
}

export function isNotice(phrases) {
	const spoken = normalisePhrase(phrases.join(' '));
	return spoken.includes('notpartof') && spoken.includes('fixtureonly');
}

// Whether NVDA's report of the foreground window's title names the page or
// the browser.
export function namesBrowser(spoken, pageTitle, application) {
	const said = normalisePhrase(spoken);
	const title = normalisePhrase(pageTitle);
	return (title.length > 0 && said.includes(title)) || said.includes(application);
}

async function main() {
	if (!process.env.CI) {
		throw new Error('screen-reader.mjs runs only on a CI runner (CI is not set): it starts NVDA, which speaks aloud.');
	}
	const options = parseArguments(process.argv.slice(2));
	const entry = BROWSERS[options.browser];
	// Loaded here so that the rest of the acceptance code needs no Guidepup.
	const { nvda, NVDAKeyCodeCommands, WindowsKeyCodes, WindowsModifiers } = await import('@guidepup/guidepup');
	const switchApplication = { keyCode: [WindowsKeyCodes.Escape], modifiers: [WindowsModifiers.Alt] };
	const server = await startServer({ root: options['serve-dist'], port: 0 });
	let driver;
	let session;
	let nvdaStarted = false;
	try {
		await nvda.start();
		nvdaStarted = true;
		// msedgedriver takes chromedriver's command line. A driver that cannot
		// start throws here, and the finally below still stops NVDA.
		driver = await startDriver(driverPath(entry, options.driver), 'chrome', {
			log: (line) => process.stderr.write(`${line}
`),
		});
		const browserOptions = { args: ['--force-renderer-accessibility', '--start-maximized', '--no-first-run', '--no-default-browser-check'] };
		if (options.binary) browserOptions.binary = options.binary;
		session = await newSession(driver.url, { browserName: entry.browserName, [entry.options]: browserOptions });
		const start = new URL(START, server.url).href;
		await session.navigate(start);
		process.stdout.write(`opened ${start} in ${session.capabilities.browserName} ${session.capabilities.browserVersion}\n`);

		const focus = () => session.executeScript(`
			const root = document.querySelector('[data-version-select]');
			const active = document.activeElement;
			return {
				summary: active === root.querySelector('summary'),
				link: active && active.matches('.version-choices a') ? active.textContent.trim() : null,
			};`);
		let presses = 0;
		while (!(await focus()).summary) {
			presses += 1;
			if (presses > 250) throw new Error('Tab never reached the version selector.');
			await pressKeys(session, Keys.Tab);
		}
		// The page builds the list again in the toggle event that follows
		// opening, replacing any link found before it. A listener added now
		// runs after the page's own, so its flag means this opening's list is
		// built or loading.
		await session.executeScript(`
			const details = document.querySelector('[data-version-select] details');
			window.__acceptanceOpened = false;
			details.addEventListener('toggle', () => { if (details.open) window.__acceptanceOpened = true; }, { once: true });`);
		await pressKeys(session, Keys.Enter);
		const deadline = Date.now() + 10_000;
		for (;;) {
			const ready = await session.executeScript(`
				const list = document.querySelector('[data-version-select] .version-choices');
				return window.__acceptanceOpened === true && list.getAttribute('aria-busy') !== 'true' && !!list.querySelector('.current');`);
			if (ready) break;
			if (Date.now() > deadline) throw new Error('The selector never listed the versions.');
			await delay(100);
		}
		let link = null;
		for (let tabs = 0; tabs < 10 && !(link ?? '').startsWith(TARGET_LABEL); tabs += 1) {
			await pressKeys(session, Keys.Tab);
			link = (await focus()).link;
		}
		if (!(link ?? '').startsWith(TARGET_LABEL)) throw new Error(`Tab never reached the ${TARGET_LABEL} link.`);
		process.stdout.write(`focused the link "${link}"\n`);

		// NVDA's key presses go to the foreground window. Alt+Escape sends the
		// foreground window to the back, which brings the next one forward.
		const pageTitle = await session.title();
		let foreground = false;
		for (let attempt = 0; attempt < FOREGROUND_ATTEMPTS && !foreground; attempt += 1) {
			if (attempt > 0) {
				await nvda.perform(switchApplication, { capture: false });
				await delay(500);
			}
			await nvda.perform(NVDAKeyCodeCommands.reportTitle);
			const title = await nvda.lastSpokenPhrase();
			process.stdout.write(`NVDA reports the foreground window as ${JSON.stringify(title)}\n`);
			foreground = namesBrowser(title, pageTitle, entry.application);
		}
		if (!foreground) throw new Error(`NVDA never reported the browser ("${pageTitle}") as the foreground window.`);
		if ((await focus()).link !== link) throw new Error(`Focus left the ${TARGET_LABEL} link while the browser was brought forward.`);

		await nvda.clearSpokenPhraseLog();
		process.stdout.write('pressing Enter through NVDA\n');
		await nvda.press('Enter', { capture: true });
		const heard = await nvda.spokenPhraseLog();
		process.stdout.write(`location after the switch: ${await session.url()}\n`);
		process.stdout.write(`spoken after Enter (${heard.length}):\n${heard.map((phrase) => `  ${JSON.stringify(phrase)}`).join('\n')}\n`);
		if (isNotice(heard)) {
			process.stdout.write('PASS NVDA spoke the arrival notice naming the page.\n');
		} else {
			process.stdout.write('FAIL NVDA did not speak the arrival notice naming the page.\n');
			process.exitCode = 1;
		}
	} finally {
		if (session && options.screenshot) {
			await session.screenshot().then((png) => writeFileSync(options.screenshot, png)).catch(() => {});
		}
		if (session) await session.deleteSession().catch(() => {});
		if (driver) await killDriver(driver.child);
		if (nvdaStarted) await nvda.stop().catch(() => {});
		await server.close();
	}
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		await main();
	} catch (error) {
		process.stderr.write(`${error instanceof Error ? error.stack ?? error.message : String(error)}\n`);
		process.exitCode = 1;
	}
}
