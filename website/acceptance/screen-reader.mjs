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
//   [--browser chrome|edge] [--driver <path>] [--binary <path>] [--listen <seconds>]
//
// The browser is a visible window, because NVDA reads only what is on the
// desktop, driven through WebDriver with real key actions: Tab to the
// selector's summary, Enter to open it, Tab to the v0.3.0 link and Enter to
// follow it. NVDA's spoken phrases are sampled for the listening time
// (20 seconds) after the last Enter, and every phrase is printed. NVDA
// may speak the notice as a live-region change or while reading the new
// page; a reader hears it either way, so either passes.
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { killDriver, startDriver } from './browsers.mjs';
import { startServer } from './server.mjs';
import { Keys, newSession, pressKeys } from './webdriver.mjs';

const START = 'docs/guides/fixture-only/';
const TARGET_LABEL = 'v0.3.0';
const DEFAULT_LISTEN_SECONDS = 20;
const SAMPLE_INTERVAL_MS = 250;

const BROWSERS = {
	chrome: { driver: 'chromedriver', folderVariable: 'CHROMEWEBDRIVER', browserName: 'chrome', options: 'goog:chromeOptions' },
	edge: { driver: 'msedgedriver', folderVariable: 'EDGEWEBDRIVER', browserName: 'MicrosoftEdge', options: 'ms:edgeOptions' },
};

function parseArguments(argv) {
	const options = { browser: 'chrome', listen: String(DEFAULT_LISTEN_SECONDS) };
	const valued = new Set(['--serve-dist', '--browser', '--driver', '--binary', '--listen']);
	for (let index = 0; index < argv.length; index += 2) {
		if (!valued.has(argv[index]) || argv[index + 1] === undefined) throw new Error(`Unexpected argument ${argv[index]}.`);
		options[argv[index].slice(2)] = argv[index + 1];
	}
	if (!options['serve-dist']) throw new Error('--serve-dist <dist> is required.');
	if (!BROWSERS[options.browser]) throw new Error('--browser must be chrome or edge.');
	options.listen = Number(options.listen);
	if (!(options.listen > 0)) throw new Error('--listen must be a number of seconds.');
	return options;
}

function driverPath(entry, explicit) {
	if (explicit) return explicit;
	const folder = process.env[entry.folderVariable];
	const candidate = folder && path.join(folder, `${entry.driver}.exe`);
	return candidate && existsSync(candidate) ? candidate : `${entry.driver}.exe`;
}

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

async function main() {
	if (!process.env.CI) {
		throw new Error('screen-reader.mjs runs only on a CI runner (CI is not set): it starts NVDA, which speaks aloud.');
	}
	const options = parseArguments(process.argv.slice(2));
	const entry = BROWSERS[options.browser];
	// Loaded here so that the rest of the acceptance code needs no Guidepup.
	const { nvda } = await import('@guidepup/guidepup');
	const server = await startServer({ root: options['serve-dist'], port: 0 });
	let driver;
	let session;
	let nvdaStarted = false;
	const heard = [];
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
			await new Promise((resolve) => setTimeout(resolve, 100));
		}
		let link = null;
		for (let tabs = 0; tabs < 10 && !(link ?? '').startsWith(TARGET_LABEL); tabs += 1) {
			await pressKeys(session, Keys.Tab);
			link = (await focus()).link;
		}
		if (!(link ?? '').startsWith(TARGET_LABEL)) throw new Error(`Tab never reached the ${TARGET_LABEL} link.`);
		process.stdout.write(`focused the link "${link}"; following it\n`);
		await nvda.clearSpokenPhraseLog();
		await pressKeys(session, Keys.Enter);

		const until = Date.now() + options.listen * 1000;
		let last = '';
		while (Date.now() < until) {
			const phrase = await nvda.lastSpokenPhrase();
			if (phrase && phrase !== last) {
				heard.push(phrase);
				last = phrase;
			}
			await new Promise((resolve) => setTimeout(resolve, SAMPLE_INTERVAL_MS));
		}
		const log = await nvda.spokenPhraseLog();
		process.stdout.write(`location after the switch: ${await session.url()}\n`);
		process.stdout.write(`sampled phrases (${heard.length}):\n${heard.map((phrase) => `  ${JSON.stringify(phrase)}`).join('\n')}\n`);
		process.stdout.write(`spoken phrase log (${log.length}):\n${log.map((phrase) => `  ${JSON.stringify(phrase)}`).join('\n')}\n`);
		if (isNotice([...log, ...heard])) {
			process.stdout.write('PASS NVDA spoke the arrival notice naming the page.\n');
		} else {
			process.stdout.write('FAIL NVDA did not speak the arrival notice naming the page.\n');
			process.exitCode = 1;
		}
	} finally {
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
