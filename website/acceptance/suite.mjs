// The acceptance checks for the documentation-version selector, the release
// index and the arrival notice (docs/website-versioning.md, "Release index
// and selector"), run in a real browser through W3C WebDriver against the
// two-release fixture that acceptance/fixture.mjs builds.
//
// Usage:
//   node acceptance/suite.mjs --target chrome|firefox|safari|ios
//     (--base http://host:port/ | --serve-dist <dist>)
//     [--only 1,2,5] [--driver <path>] [--browser <path>] [--udid <udid>]
//     [--report <file.json>]
//
// --base checks a fixture some other process serves; --serve-dist serves the
// dist folder itself with acceptance/server.mjs. Either way the suite puts two
// proxies of its own in front of the fixture: one answers 404 for
// /versions/index.json, as when the development site's index cannot be
// fetched, and one holds that request until a check releases it, so the
// loading state can be seen. Each check prints one line, PASS, FAIL or SKIP
// with its reason, and any failure makes the exit status 1. --report writes
// every result, with what each check measured. SIGINT and SIGTERM stop the
// browser, its driver and the servers before exiting.
//
// The suite does what a reader does: it clicks or taps the summary and the
// links and presses real keys. Scripts in the page only read state, such as
// the location, scroll widths, sizes and attributes, except as listed here:
// an element is scrolled into view before it is tapped, as a reader scrolls
// to it; a phone width a desktop browser window cannot reach is given to the
// page by an iframe of that width; the colour theme is chosen through the
// same localStorage entry the site's theme menu writes; a listener counts the
// selector's openings so that a check can wait for the list each opening
// builds instead of sleeping; and, where the driver keeps no console log, a
// listener records page errors, as the next paragraph says.
//
// Page errors are collected in two ways. Chrome's driver keeps the browser's
// console log, so every console error, uncaught exception and failed request
// fails the check that caused it, except a 404 the check itself asks for.
// Other drivers expose no console log, so there a listener added after each
// page loads records errors and unhandled rejections from then on; an error
// raised while a page is loading is not seen there.
//
// Not covered here: how a screen reader speaks the notice, which
// acceptance/screen-reader.mjs checks on a CI runner.
import http from 'node:http';
import https from 'node:https';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
	DEFAULT_WINDOW, TARGETS, pressReturnOnSimulatorKeyboard, restartSimulatorSafari, startBrowser, typeOnSimulatorKeyboard,
} from './browsers.mjs';
import { compareTags } from '../scripts/published-versions.mjs';
import { describeError } from '../scripts/snapshot-source.mjs';
import { FIXTURE_ONLY_HEADING, FIXTURE_ONLY_PATH, FIXTURE_TAGS } from './fixture.mjs';
import { startServer } from './server.mjs';
import { Keys, WebDriverError, pressKeys, tap, waitUntil } from './webdriver.mjs';

// What the fixture contains (acceptance/fixture.mjs): its releases, newest
// first, as the release index and the selector list them.
const RELEASES = [...FIXTURE_TAGS].sort((a, b) => compareTags(b, a));
const LATEST = RELEASES[0];
const OLDEST = RELEASES.at(-1);
const SHARED_PAGE = 'docs/guides/your-version/';
const SHARED_HEADING = '#what-was-checked';
const SHARED_TITLE = 'Which Labrador you have';
const FIXTURE_ONLY = FIXTURE_ONLY_PATH.slice(1);
const FIXTURE_ONLY_TITLE = 'Fixture-only guide';
const ERRATUM_TITLE = 'Fixture erratum';
const CANONICAL_ORIGIN = 'https://labradorengine.com';
const INDEX_PATH = '/versions/index.json';
const PHONE_WIDTHS = [320, 360, 430];
const PHONE_HEIGHT = 800;
const DESKTOP_WINDOW = { width: 1440, height: 900 };
const THEMES = ['light', 'dark'];
const MIN_TARGET_PX = 44;
const MIN_OUTLINE_PX = 2;
const MAX_TAB_PRESSES = 250;
// Where taps can be lost (retryLostInput in browsers.mjs): how many times a
// reader taps, and how long each tap but the last is given to take effect.
const LOST_TAP_ATTEMPTS = 3;
const LOST_TAP_WAIT_MS = 6_000;
// How long typed text is given to appear in a field before it is typed again.
const TYPED_TEXT_WAIT_MS = 3_000;

// The arrival notice, written out rather than taken from the site's own
// version-switch.mjs, which is the code under test.
const EXPECTED_NOTICE = {
	Development: `The page ${FIXTURE_ONLY_PATH} that you were reading in Development is not part of ${OLDEST}, `
		+ `so this is the ${OLDEST} documentation overview.`,
	[LATEST]: `The page ${FIXTURE_ONLY_PATH} that you were reading in ${LATEST} is not part of ${OLDEST}, `
		+ `so this is the ${OLDEST} documentation overview.`,
};

function versionPath(label) {
	return label === 'Development' ? '/' : `/versions/${label}/`;
}

class Skip extends Error {}

// ---------------------------------------------------------------------------
// Scripts run in the page.

const INSTALL_ERROR_LISTENER = `
	if (!window.__acceptanceErrors) {
		const errors = window.__acceptanceErrors = [];
		addEventListener('error', (event) => {
			const target = event.target && event.target !== window ? event.target : null;
			// Leaving a page cancels the prefetches Astro started on it, and
			// Firefox reports each as a failed load; no reader sees that.
			if (target && target.matches && target.matches('link[rel~="prefetch"]')) return;
			errors.push(target ? 'failed to load ' + (target.src || target.href || target.tagName) : 'error: ' + event.message);
		}, true);
		addEventListener('unhandledrejection', (event) => {
			errors.push('unhandled rejection: ' + String(event.reason && event.reason.message || event.reason));
		});
	}`;

const TAKE_ERRORS = `
	const errors = window.__acceptanceErrors || [];
	return errors.splice(0, errors.length);`;

// Counts the selector's openings. Added after the page's own toggle listener,
// it runs after that listener has rebuilt the list or started loading it, so
// a count above the one read before an opening means that opening's list is
// the one now in the page. Returns the count so far.
const COUNT_OPENINGS = `
	const details = document.querySelector('[data-version-select] details');
	if (!details) return -1;
	if (!details.__acceptanceOpenings) {
		details.__acceptanceOpenings = { count: 0 };
		details.addEventListener('toggle', () => { if (details.open) details.__acceptanceOpenings.count += 1; });
	}
	return details.__acceptanceOpenings.count;`;

const LOCATION = 'return { href: location.href, search: location.search, hash: location.hash, ready: document.readyState };';

// The selector's state as a reader would perceive it.
const SELECTOR_STATE = `
	const roots = document.querySelectorAll('[data-version-select]');
	const root = roots[0];
	if (!root) return null;
	const details = root.querySelector('details');
	const summary = root.querySelector('summary');
	const list = root.querySelector('.version-choices');
	const current = list.querySelector('.current');
	const status = list.querySelector('.status');
	const height = (element) => element.getBoundingClientRect().height;
	return {
		count: roots.length,
		openings: details.__acceptanceOpenings ? details.__acceptanceOpenings.count : 0,
		open: details.open,
		busy: list.getAttribute('aria-busy') === 'true',
		checking: status ? status.textContent.trim() : null,
		summaryText: summary.textContent.replace(/\\s+/g, ' ').trim(),
		summaryHeight: height(summary),
		current: current && {
			text: current.textContent.trim(),
			tag: current.tagName,
			ariaCurrent: current.getAttribute('aria-current'),
			insideLink: !!current.closest('a'),
		},
		items: [...list.children].map((li) => {
			const content = li.firstElementChild;
			return { tag: content ? content.tagName : null, text: li.innerText.replace(/\\s+/g, ' ').trim(), height: content ? height(content) : 0 };
		}),
		links: [...list.querySelectorAll('a')].map((a) => {
			const note = a.querySelector('.note');
			const style = note && getComputedStyle(note);
			return {
				label: (a.firstElementChild || a).textContent.trim().split(' (')[0],
				text: a.innerText.replace(/\\s+/g, ' ').trim(),
				href: a.href,
				rawHref: a.getAttribute('href'),
				note: note && {
					text: note.textContent.trim(),
					visible: note.getBoundingClientRect().height > 0 && style.visibility === 'visible' && style.display !== 'none',
				},
			};
		}),
	};`;

const VERSION_LINK = `
	const root = document.querySelector('[data-version-select]');
	return [...root.querySelectorAll('.version-choices a')]
		.find((a) => (a.firstElementChild || a).textContent.trim().split(' (')[0] === arguments[0]) || null;`;

const FOCUS = `
	const root = document.querySelector('[data-version-select]');
	const summary = root.querySelector('summary');
	const stops = [summary, ...root.querySelectorAll('.version-choices a')];
	const active = document.activeElement;
	const style = active ? getComputedStyle(active) : null;
	return {
		index: stops.indexOf(active),
		stops: stops.length,
		inside: root.contains(active),
		open: root.querySelector('details').open,
		tag: active ? active.tagName : null,
		text: active ? (active.textContent || '').replace(/\\s+/g, ' ').trim().slice(0, 60) : null,
		label: active && active.matches('.version-choices a') ? (active.firstElementChild || active).textContent.trim().split(' (')[0] : null,
		focusVisible: active ? active.matches(':focus-visible') : false,
		outlineWidth: style ? parseFloat(style.outlineWidth) || 0 : 0,
		devicePixelRatio,
		outlineStyle: style ? style.outlineStyle : null,
	};`;

const OVERFLOW = `
	const doc = document.documentElement;
	const pane = document.querySelector('#starlight__sidebar');
	return {
		innerWidth,
		clientWidth: doc.clientWidth,
		scrollWidth: Math.max(doc.scrollWidth, document.body.scrollWidth),
		pane: pane ? { clientWidth: pane.clientWidth, scrollWidth: pane.scrollWidth } : null,
		theme: doc.dataset.theme || null,
	};`;

// Where the selector sits among the sidebar's (or phone menu's) visible parts.
const SIDEBAR_ORDER = `
	const roots = document.querySelectorAll('[data-version-select]');
	const content = document.querySelector('#starlight__sidebar .sidebar-content');
	const root = roots[0];
	if (!content || !root) return { count: roots.length, inSidebar: false };
	// A link in a collapsed group still has a box in some browsers, so it is
	// shown only outside every closed <details> but that group's summary.
	const shown = (element) => {
		if (element.closest('details:not([open]) > :not(summary)')) return false;
		const r = element.getBoundingClientRect();
		return r.width > 0 && r.height > 0;
	};
	const origin = content.getBoundingClientRect().top;
	const top = (element) => element.getBoundingClientRect().top - origin;
	const others = [...content.querySelectorAll('a, summary, button, select, input')]
		.filter((element) => !root.contains(element)).filter(shown);
	const pages = others.filter((element) => element.matches('a[href]'));
	const below = others.filter((element) => top(element) > top(root));
	const describe = (element) => element.tagName + ' ' + (element.textContent || '').replace(/\\s+/g, ' ').trim().slice(0, 40);
	return {
		count: roots.length,
		inSidebar: content.contains(root),
		selectorTop: Math.round(top(root)),
		pages: pages.length,
		pagesAbove: pages.filter((element) => top(element) < top(root)).length,
		itemsBelow: below.length,
		firstBelow: below[0] ? describe(below[0]) + ' at y=' + Math.round(top(below[0])) : null,
	};`;

const MENU_OPEN = `
	const pane = document.querySelector('#starlight__sidebar');
	return !!pane && pane.matches(':popover-open');`;

// True when the page shows a menu button and its menu is closed.
const MENU_HIDDEN = `
	const button = document.querySelector('button.sl-menu-button');
	const pane = document.querySelector('#starlight__sidebar');
	if (!button || !pane || pane.matches(':popover-open')) return false;
	const box = button.getBoundingClientRect();
	return box.width > 0 && box.height > 0 && getComputedStyle(button).visibility !== 'hidden';`;

const SCROLL_INTO_VIEW = 'arguments[0].scrollIntoView({ block: "center", inline: "nearest" });';

// Brings the element into view with the page scrolled to its top, so far as
// it can be: the page is scrolled only when the element lies below the first
// screen. Returns where the element is and how far the page scrolled.
const INTO_VIEW_FROM_TOP = `
	const element = arguments[0];
	window.scrollTo(0, 0);
	element.scrollIntoView({ block: 'nearest', inline: 'nearest' });
	const box = element.getBoundingClientRect();
	return {
		y: Math.round(box.top + box.height / 2 + scrollY),
		scrollY,
		height: innerHeight,
		text: (element.textContent || '').replace(/\\s+/g, ' ').trim().slice(0, 40),
	};`;

const NOTICE = `
	const notice = document.querySelector('[data-version-notice]');
	if (!notice) return null;
	const rect = notice.getBoundingClientRect();
	return { text: notice.textContent, role: notice.getAttribute('role'), height: rect.height };`;

const CANONICAL = `
	const link = document.querySelector('link[rel="canonical"]');
	return link ? link.getAttribute('href') : null;`;

// The site's theme menu keeps the reader's choice under this key.
const SET_THEME = 'localStorage.setItem("starlight-theme", arguments[0]);';

// How long after the load event the notice was filled, in milliseconds.
const SINCE_LOAD = `
	const navigation = performance.getEntriesByType('navigation')[0];
	return navigation && navigation.loadEventEnd > 0 ? performance.now() - navigation.loadEventEnd : null;`;

// Resolves once the load event has ended and the given time has passed since.
const AFTER_LOAD = `
	const done = arguments[arguments.length - 1];
	const wait = () => {
		const navigation = performance.getEntriesByType('navigation')[0];
		if (!navigation || navigation.loadEventEnd === 0) return requestAnimationFrame(wait);
		setTimeout(() => requestAnimationFrame(() => done(performance.now() - navigation.loadEventEnd)),
			Math.max(0, navigation.loadEventEnd + arguments[0] - performance.now()));
	};
	wait();`;

// ---------------------------------------------------------------------------
// The context each check runs in.

class Context {
	constructor({ session, target, flags, base, blockedBase, heldIndex, log }) {
		this.session = session;
		this.target = target;
		this.flags = flags;
		this.base = base;
		this.blockedBase = blockedBase;
		this.heldIndex = heldIndex;
		this.log = log;
		this.frame = null;
		this.chromeLog = target === 'chrome';
		this.touchInput = false;
		this.reset();
	}

	reset() {
		this.failures = [];
		this.details = [];
		this.pageErrors = [];
		this.allowed404 = [];
	}

	abs(urlPath, base = this.base) {
		return new URL(urlPath, base).href;
	}

	note(text) {
		this.details.push(text);
	}

	// A soft assertion: the check goes on and fails at the end.
	expect(condition, message) {
		if (!condition) this.failures.push(message);
		return Boolean(condition);
	}

	script(source, args = []) {
		return this.session.executeScript(source, args);
	}

	async takePageErrors() {
		try {
			const errors = await this.script(TAKE_ERRORS);
			if (Array.isArray(errors)) this.pageErrors.push(...errors);
		} catch {
			// The page is between documents; its listener went with it.
		}
	}

	async consoleErrors() {
		if (!this.chromeLog) return [];
		const entries = await this.session.send('POST', '/se/log', { type: 'browser' });
		return entries
			.filter((entry) => entry.level === 'SEVERE')
			.filter((entry) => !(entry.source === 'network' && this.allowed404.some((url) => entry.message.includes(url))))
			.map((entry) => `console (${entry.source}): ${entry.message}`);
	}

	async drainConsole() {
		if (this.chromeLog) await this.session.send('POST', '/se/log', { type: 'browser' });
	}

	// Waits until the page has loaded, at expectedHref when one is given or
	// anywhere but leaving when { leaving } names the address being left.
	async settle(expectedHref, { leaving, timeoutMs = 15_000 } = {}) {
		const where = await waitUntil(async () => {
			try {
				const location = await this.script(LOCATION);
				// A frame being replaced can answer null.
				if (!location || location.ready !== 'complete') return null;
				if (expectedHref !== undefined && location.href !== expectedHref) return null;
				if (leaving !== undefined && location.href === leaving) return null;
				return location;
			} catch (error) {
				if (error instanceof WebDriverError && error.code !== 'no such window') return null;
				// Safari answers "no such window" for a frame whose document is
				// being replaced; the frame is entered again and asked again.
				if (error instanceof WebDriverError && this.frame) {
					await this.enterFrame().catch(() => {});
					return null;
				}
				throw error;
			}
		}, { timeoutMs, message: expectedHref ? `the page at ${expectedHref}` : 'the page to load' }).catch(async (error) => {
			const location = await this.script(LOCATION).catch(() => null);
			throw new Error(`${error.message} The page is at ${location?.href ?? 'an unknown address'}.`);
		});
		await this.script(INSTALL_ERROR_LISTENER);
		return where;
	}

	// The phone-width frame that loadAtWidth made, when the session is in it.
	async enterFrame() {
		await this.session.send('POST', '/frame', { id: null });
		const element = await this.session.findElement(this.frame);
		await this.session.send('POST', '/frame', { id: element });
	}

	// On the iOS Simulator a navigation is sometimes never answered; the
	// driver's command times out with the page still where it was, and asking
	// once more loads it.
	async go(urlPath, { base } = {}) {
		await this.takePageErrors();
		const href = this.abs(urlPath, base);
		try {
			await this.session.navigate(href);
		} catch (error) {
			if (!(this.flags.retryLostInput && error instanceof WebDriverError && error.code === 'client timeout')) throw error;
			this.note(`the navigation to ${href} was not answered; it was asked for again`);
			await this.session.navigate(href);
		}
		return this.settle();
	}

	// Chooses a colour theme for this origin and loads the current page again
	// in it.
	async useTheme(theme) {
		await this.script(SET_THEME, [theme]);
		await this.takePageErrors();
		await this.session.refresh();
		await this.settle();
	}

	async selector() {
		return this.script(SELECTOR_STATE);
	}

	// A finger's tap on a phone, and at phone widths where the target has
	// touch. On the iOS Simulator safaridriver's element click does not open
	// Starlight's menu at all, and its touches land where they are asked to
	// only while the page is scrolled to its top (touchAtPagePoint in
	// browsers.mjs), so there the page goes back to its top first and an
	// element below the first screen cannot be tapped. Its touch generator is
	// slow enough that a held finger can become a long press, so its taps
	// lift without a pause.
	async activate(element) {
		if (this.flags.touchAtPagePoint) {
			const place = await this.script(INTO_VIEW_FROM_TOP, [element]);
			if (place.scrollY > 0) {
				throw new Error(`"${place.text}" is ${place.y} px down the page, below its first screen of ${place.height} px, `
					+ `where ${this.target}'s driver places no touch where it is asked to.`);
			}
			await tap(this.session, element, { pauseMs: 0 });
		} else if (this.flags.phone || this.touchInput) {
			await this.script(SCROLL_INTO_VIEW, [element]);
			await tap(this.session, element);
		} else {
			await this.session.click(element);
		}
	}

	// Taps `element` and waits until `done` returns something truthy, which is
	// returned, or false after the wait, which after the last tap is
	// lastWaitMs or waitUntil's default. Where taps can be lost, a tap after
	// which `done` is still false is repeated.
	async tapUntil(element, done, message, lastWaitMs = undefined) {
		const taps = this.flags.retryLostInput ? LOST_TAP_ATTEMPTS : 1;
		for (let tapAt = 1; ; tapAt += 1) {
			await this.activate(element);
			const result = await waitUntil(done, { timeoutMs: tapAt < taps ? LOST_TAP_WAIT_MS : lastWaitMs, message }).catch(() => false);
			if (result || tapAt >= taps) return result;
			this.note(`tap ${tapAt} did not bring ${message}; it was tapped again`);
		}
	}

	// Types into a field that has focus: on the target's keyboard where its
	// driver cannot type, otherwise through the driver.
	async type(element, text) {
		if (this.flags.simulatorKeyboard) await typeOnSimulatorKeyboard(text);
		else await this.session.sendKeys(element, text);
	}

	// Types into a field that has focus and waits until the field holds
	// something. The simulator can drop the first text sent to a box it is
	// still focusing; a reader whose text did not appear types it again, and
	// the second text is given waitUntil's default time to arrive.
	async typeAndConfirm(element, text, where) {
		await this.type(element, text);
		const typed = () => this.script('return arguments[0].value;', [element]);
		if (await waitUntil(typed, { timeoutMs: TYPED_TEXT_WAIT_MS, message: 'the typed text' }).catch(() => '')) return;
		this.note(`the first text sent to ${where} did not arrive; it was sent again`);
		await this.type(element, text);
		await waitUntil(typed, { message: `the text typed again into ${where}` });
	}

	// The number of openings so far; pass it to waitForList after opening.
	async countOpenings() {
		const count = await this.script(COUNT_OPENINGS);
		if (count < 0) throw new Error('The page has no version selector.');
		return count;
	}

	// At a phone's width the sidebar, and the selector in it, is the menu
	// behind the header's menu button: a reader opens that first.
	// A tap on the sticky header button of a page scrolled to a heading can
	// miss on iOS, so the reader's way is taken: back to the top, then tap,
	// and tap again if the menu has not opened.
	async revealSidebar() {
		if (!(await this.script(MENU_HIDDEN))) return;
		await this.script('window.scrollTo(0, 0);');
		for (let tapAt = 1; tapAt <= 3; tapAt += 1) {
			const menu = await this.session.findElement('button.sl-menu-button');
			await this.activate(menu);
			const opened = await waitUntil(() => this.script(MENU_OPEN), { timeoutMs: 5_000, message: 'the phone menu to open' }).catch(() => false);
			if (opened) return;
		}
		throw new Error('Three taps of the menu button did not open the phone menu.');
	}

	// Closes the phone menu, which covers the page, before a tap on the page.
	async hideSidebar() {
		if (!(await this.script(MENU_OPEN))) return;
		const menu = await this.session.findElement('button.sl-menu-button');
		await this.activate(menu);
		await waitUntil(async () => !(await this.script(MENU_OPEN)), { message: 'the phone menu to close' });
	}

	// Opens the selector by clicking or tapping its summary and waits until
	// the version list is shown, or with { staticOnly } until loading ends.
	// On the iOS Simulator a tap is sometimes lost; a reader whose tap did not
	// open the selector taps it again, but never taps an open one, which
	// would close it.
	async openSelector({ staticOnly = false } = {}) {
		const before = await this.countOpenings();
		await this.revealSidebar();
		const summary = await this.session.findElement('[data-version-select] summary');
		const taps = this.flags.retryLostInput ? LOST_TAP_ATTEMPTS : 1;
		for (let tapAt = 1; ; tapAt += 1) {
			await this.activate(summary);
			try {
				return await this.waitForList({ staticOnly, after: before, timeoutMs: tapAt < taps ? LOST_TAP_WAIT_MS : undefined });
			} catch (error) {
				const state = await this.selector().catch(() => null);
				if (tapAt >= taps || !state || state.open) throw error;
				this.note(`tap ${tapAt} on the selector's summary did not open it; it was tapped again`);
			}
		}
	}

	// Waits for the list that the opening after `after` built: the page builds
	// it again in the toggle event, which replaces any link found before it.
	async waitForList({ staticOnly = false, after, timeoutMs }) {
		const state = await waitUntil(async () => {
			const current = await this.selector();
			if (!current?.open || current.openings <= after || current.busy || current.checking) return null;
			if (!staticOnly && !current.current) return null;
			return current;
		}, { timeoutMs, message: staticOnly ? 'the selector to finish loading' : 'the selector to list the versions' });
		this.expect(state.count === 1, `the page has ${state.count} version selectors, not one.`);
		return state;
	}

	async versionLink(label) {
		const link = await this.script(VERSION_LINK, [label]);
		if (!link) throw new Error(`The selector has no link to ${label}.`);
		return link;
	}

	// Follows a link the way a reader does and waits for the page it opens.
	// Where taps can be lost, a tap after which the page is still where it
	// was is repeated, as a reader would.
	async follow(element, expectedHref, { leaving } = {}) {
		await this.takePageErrors();
		if (!this.flags.retryLostInput) {
			await this.activate(element);
			return this.settle(expectedHref, { leaving });
		}
		const from = (await this.script(LOCATION))?.href;
		for (let tapAt = 1; ; tapAt += 1) {
			await this.activate(element);
			try {
				return await this.settle(expectedHref, { leaving, timeoutMs: tapAt < LOST_TAP_ATTEMPTS ? LOST_TAP_WAIT_MS : undefined });
			} catch (error) {
				const now = await this.script(LOCATION).catch(() => null);
				if (tapAt >= LOST_TAP_ATTEMPTS || !now || now.href !== from) throw error;
				this.note(`tap ${tapAt} on a link left the page at ${from}; it was tapped again`);
			}
		}
	}

	async switchTo(label, expectedHref) {
		const state = await this.openSelector();
		const link = await this.versionLink(label);
		return { state, location: await this.follow(link, expectedHref) };
	}

	// Presses the target's Tab, or with { backward } Shift with it, until the
	// selector's summary has focus.
	async tabToSummary({ backward = false } = {}) {
		const tab = this.flags.tabKey;
		const key = backward ? [Keys.Shift, ...(Array.isArray(tab) ? tab : [tab])] : tab;
		for (let presses = 1; presses <= MAX_TAB_PRESSES; presses += 1) {
			await pressKeys(this.session, key);
			const focus = await this.script(FOCUS);
			if (focus.index === 0) return { presses, focus, key: this.tabName({ backward }) };
		}
		throw new Error(`${MAX_TAB_PRESSES} presses of ${this.tabName({ backward })} never reached the selector's summary.`);
	}

	tabName({ backward = false } = {}) {
		return `${backward ? 'Shift-' : ''}${Array.isArray(this.flags.tabKey) ? 'Option-Tab' : 'Tab'}`;
	}
}

async function fetchText(url) {
	const response = await fetch(url, { redirect: 'manual', signal: AbortSignal.timeout(15_000) });
	return { status: response.status, text: await response.text() };
}

function canonicalOf(html) {
	const match = /<link\b[^>]*\brel="canonical"[^>]*>/.exec(html);
	const href = match && /\bhref="([^"]*)"/.exec(match[0]);
	return href ? href[1] : null;
}

// ---------------------------------------------------------------------------
// Viewports. A phone width is the device's own on a phone. A desktop browser
// window cannot be made that narrow (headless Chrome and Firefox stop near
// 500 CSS pixels), so there the page is loaded into an iframe exactly that
// wide, whose document lays out, scrolls and matches media queries at that
// width. The frame is no taller than the host's viewport, so that every
// point of it can be clicked.

async function topFrame(context) {
	context.frame = null;
	await context.session.send('POST', '/frame', { id: null });
}

// Loads a page in the window itself with the theme chosen before it loads:
// a reload to apply the theme would lose the arrival notice, which shows only
// on the first load.
async function loadInTheme(context, urlPath, theme, landsAt = urlPath) {
	await topFrame(context);
	await context.go('/404.html');
	await context.script(SET_THEME, [theme]);
	await context.go(urlPath);
	await context.settle(context.abs(landsAt));
}

// landsAt names the address the page settles on when it rewrites its own
// (the arrival notice drops its query); it defaults to urlPath. A null width
// is the window's own.
async function loadAtWidth(context, width, urlPath, theme, landsAt = urlPath) {
	if (width === null) {
		await loadInTheme(context, urlPath, theme, landsAt);
		return;
	}
	await context.takePageErrors();
	await topFrame(context);
	// The host is a page of the same site, because Chrome refuses to load a
	// loopback page into a frame of about:blank. The frame covers the host,
	// which is left as it loaded, so its own scripts find what they expect.
	await context.session.navigate(context.abs('/404.html'));
	await context.settle();
	await context.script(SET_THEME, [theme]);
	await context.script(`
		const frame = document.createElement('iframe');
		frame.id = 'acceptance-viewport';
		frame.style.cssText = 'position:fixed;top:0;left:0;z-index:2147483647;display:block;border:0;background:white;'
			+ 'width:' + arguments[0] + 'px;height:' + Math.min(arguments[1], innerHeight) + 'px';
		frame.src = arguments[2];
		document.body.append(frame);`, [width, PHONE_HEIGHT, context.abs(urlPath)]);
	context.frame = '#acceptance-viewport';
	await context.enterFrame();
	await context.settle(context.abs(landsAt));
}

async function restoreWindow(context) {
	await topFrame(context).catch(() => {});
	if (context.flags.setWindowRect) await context.session.setWindowRect({ ...DEFAULT_WINDOW });
}

// Puts the theme back to following the system, on the origin checked.
async function restoreTheme(context) {
	await topFrame(context).catch(() => {});
	await context.go('/404.html');
	await context.script(SET_THEME, ['']);
}

function checkTheme(context, overflow, theme, where) {
	context.expect(overflow.theme === theme, `${where}: the page is in the ${overflow.theme} theme, not ${theme}.`);
}

function checkOverflow(context, overflow, where) {
	context.expect(overflow.scrollWidth <= overflow.clientWidth,
		`${where}: the page scrolls sideways (scrollWidth ${overflow.scrollWidth} > clientWidth ${overflow.clientWidth}).`);
	if (overflow.pane && overflow.pane.clientWidth > 0) {
		context.expect(overflow.pane.scrollWidth <= overflow.pane.clientWidth,
			`${where}: the sidebar scrolls sideways (scrollWidth ${overflow.pane.scrollWidth} > clientWidth ${overflow.pane.clientWidth}).`);
	}
}

function checkTargets(context, state, where) {
	context.expect(state.summaryHeight >= MIN_TARGET_PX,
		`${where}: the selector's summary is ${state.summaryHeight.toFixed(1)} px tall, below ${MIN_TARGET_PX}.`);
	for (const item of state.items) {
		context.expect(item.height >= MIN_TARGET_PX,
			`${where}: the version item "${item.text}" is ${item.height.toFixed(1)} px tall, below ${MIN_TARGET_PX}.`);
	}
}

// Once per page, in the sidebar (which is also the phone menu), after every
// page link and with nothing below it.
function checkSelectorPlacement(context, order, where, place) {
	context.expect(order.count === 1, `${where}: the page has ${order.count} version selectors, not one.`);
	if (!context.expect(order.inSidebar, `${where}: the selector is not inside the ${place}.`)) return;
	context.expect(order.pages > 0 && order.pagesAbove === order.pages,
		`${where}: the selector at y=${order.selectorTop} does not follow the ${place}'s pages: ${order.pagesAbove} of ${order.pages} page links are above it.`);
	context.expect(order.itemsBelow === 0,
		`${where}: the selector at y=${order.selectorTop} is not last in the ${place}: ${order.itemsBelow} items follow it, the first being ${order.firstBelow}.`);
}

// ---------------------------------------------------------------------------
// The checks.

async function sharedPage(context) {
	const route = SHARED_PAGE;
	await context.go(`/${route}${SHARED_HEADING}`);
	const title = await context.session.title();
	const order = [LATEST, OLDEST, 'Development', OLDEST, LATEST, 'Development'];
	for (const label of order) {
		const expected = context.abs(`${versionPath(label)}${route}${SHARED_HEADING}`);
		const { location } = await context.switchTo(label, expected);
		const landed = await context.script(`
			return { title: document.title, heading: !!document.getElementById(arguments[0]),
				current: (document.querySelector('[data-version-select] summary strong') || {}).textContent };`, [SHARED_HEADING.slice(1)]);
		context.expect(landed.title === title, `${label}: the title is "${landed.title}", not "${title}".`);
		context.expect(landed.heading, `${label}: the page has no heading ${SHARED_HEADING}.`);
		context.expect(landed.current === label, `${label}: the selector names "${landed.current}".`);
		context.note(`switched to ${label}: ${location.href}`);
	}
}

// The accessible name the driver computes, or null when the driver has no
// such command.
async function accessibleName(context, element) {
	try {
		return await context.session.send('GET', `${context.session.element(element)}/computedlabel`);
	} catch (error) {
		if (error instanceof WebDriverError && ['unknown command', 'unsupported operation', 'unknown method'].includes(error.code)) return null;
		throw error;
	}
}

async function absentPage(context) {
	// From the fixture-only guide's heading, the versions that have the page
	// keep the page and the heading; the one that lacks it does not.
	const heading = `#${FIXTURE_ONLY_HEADING}`;
	await context.go(`/${FIXTURE_ONLY}${heading}`);
	for (const label of [LATEST, 'Development']) {
		const { location } = await context.switchTo(label, context.abs(`${versionPath(label)}${FIXTURE_ONLY}${heading}`));
		const present = await context.script('return !!document.getElementById(arguments[0]);', [FIXTURE_ONLY_HEADING]);
		context.expect(present, `${label}: the page has no heading ${heading}.`);
		context.note(`from the heading, switched to ${label}: ${location.href}`);
	}
	const fromHeading = await context.openSelector();
	const oldestFromHeading = fromHeading.links.find((candidate) => candidate.label === OLDEST);
	context.expect(oldestFromHeading?.href.startsWith(`${context.abs(`${versionPath(OLDEST)}docs/`)}?`),
		`from the heading, the ${OLDEST} link goes to ${oldestFromHeading?.href}, not its overview.`);

	for (const from of ['Development', LATEST]) {
		const fromPath = `${versionPath(from)}${FIXTURE_ONLY}`;
		await context.go(fromPath);
		const state = await context.openSelector();
		const link = state.links.find((candidate) => candidate.label === OLDEST);
		if (!context.expect(link, `from ${from}: the selector offers no ${OLDEST}.`)) continue;
		const noteText = `This page is not in ${OLDEST}`;
		context.expect(link.note?.visible, `from ${from}: the ${OLDEST} link shows no visible absent-page note.`);
		context.expect(link.text.includes(noteText), `from ${from}: the ${OLDEST} link reads "${link.text}", without "${noteText}".`);
		const element = await context.versionLink(OLDEST);
		const name = await accessibleName(context, element);
		if (name === null) {
			context.note(`from ${from}: the driver cannot compute accessible names; the note is inside the link: ${link.text}`);
		} else {
			context.expect(typeof name === 'string' && name.includes(noteText),
				`from ${from}: the ${OLDEST} link's accessible name is ${JSON.stringify(name)}, without the note.`);
			context.note(`from ${from}: the ${OLDEST} link's accessible name is "${name}"`);
		}
		const overview = context.abs(`${versionPath(OLDEST)}docs/`);
		context.expect(link.href.startsWith(`${overview}?`), `from ${from}: the ${OLDEST} link goes to ${link.href}, not its overview.`);
		await context.follow(element, undefined, { leaving: context.abs(fromPath) });
		const landed = await waitUntil(async () => {
			const location = await context.script(LOCATION);
			return location.search === '' && location.href.startsWith(overview) ? location : null;
		}, { message: `the query to leave the address on ${overview}` }).catch((error) => {
			context.expect(false, `from ${from}: ${error.message}`);
			return null;
		});
		if (landed) context.expect(landed.href === overview, `from ${from}: landed on ${landed.href}, not ${overview}.`);
		const served = canonicalOf((await fetchText(overview)).text);
		const canonical = await context.script(CANONICAL);
		context.expect(canonical === served && served === `${CANONICAL_ORIGIN}${versionPath(OLDEST)}docs/`,
			`from ${from}: the canonical link is ${canonical}; the served page has ${served}.`);
		const expectedNotice = EXPECTED_NOTICE[from];
		const notice = await waitUntil(async () => {
			const current = await context.script(NOTICE);
			return current?.text ? current : null;
		}, { message: 'the arrival notice' }).catch(() => null);
		if (context.expect(notice, `from ${from}: no arrival notice appeared on ${OLDEST}'s overview.`)) {
			context.expect(notice.role === 'status', `from ${from}: the notice's role is ${notice.role}.`);
			context.expect(notice.height > 0, `from ${from}: the notice is not visible.`);
			context.expect(notice.text === expectedNotice, `from ${from}: the notice reads "${notice.text}", not "${expectedNotice}".`);
			context.note(`from ${from}: notice "${notice.text}"`);
		}
		// Back from the overview, which every version has.
		const back = context.abs(`${versionPath(from)}docs/`);
		const { location } = await context.switchTo(from, back);
		const after = await context.script(NOTICE);
		context.expect(!after?.text, `back in ${from}: a notice is shown: "${after?.text}".`);
		context.note(`from ${from}: back to ${location.href}`);
	}
}

async function noticeAnnounceable(context) {
	const arrival = `${versionPath(OLDEST)}docs/?version-missing=${encodeURIComponent(FIXTURE_ONLY)}&from=development`;
	const served = await fetchText(context.abs(arrival));
	context.expect(served.status === 200, `GET ${arrival} answered ${served.status}.`);
	const element = /<p\b[^>]*\bdata-version-notice\b[^>]*>([\s\S]*?)<\/p>/.exec(served.text);
	if (context.expect(element, 'the served overview has no [data-version-notice] element.')) {
		context.expect(/\brole="status"/.test(element[0]), `the served notice element is ${element[0].slice(0, 160)}, without role="status".`);
		context.expect(element[1] === '', `the served notice is not empty: "${element[1].slice(0, 160)}".`);
		context.note(`served: ${element[0].replace(/>[\s\S]*$/, '>')}, empty`);
	}
	await context.session.navigate(context.abs(arrival));
	const first = await context.script(NOTICE);
	context.note(`when the driver's navigation returned the notice read "${first?.text ?? ''}"`);
	await context.settle();
	const filled = await waitUntil(async () => {
		const current = await context.script(NOTICE);
		return current?.text ? current : null;
	}, { message: 'the notice to be filled' }).catch(() => null);
	const filledAfter = await context.script(SINCE_LOAD);
	const expected = EXPECTED_NOTICE.Development;
	if (context.expect(filled, 'the notice was never filled after load.')) {
		context.expect(filled.text === expected, `the notice reads "${filled.text}", not "${expected}".`);
		context.expect(filled.role === 'status', `the notice's role is ${filled.role}.`);
		context.note(`filled within ${Math.round(filledAfter ?? 0)} ms of the load event`);
	}
	const location = await context.script(LOCATION);
	context.expect(location.search === '', `the address still carries ${location.search}.`);
	// A reload, with the query gone, repeats nothing. The notice is filled a
	// fixed time after the load event, so twice the time it took above, and
	// at least a second, is past any fill this load could have scheduled.
	await context.takePageErrors();
	await context.session.refresh();
	const reloadedAt = await context.settle();
	context.expect(reloadedAt.search === '', `after the reload the address carries ${reloadedAt.search}.`);
	const waited = await context.session.executeAsyncScript(AFTER_LOAD, [Math.max(1_000, 2 * (filledAfter ?? 1_000))]);
	const reloaded = await context.script(NOTICE);
	context.expect(!reloaded?.text, `after a reload the notice reads "${reloaded?.text}".`);
	context.note(`after the reload the notice was still empty ${Math.round(waited)} ms after the load event`);
}

async function keyboard(context) {
	if (!context.flags.keyboard) throw new Skip(`${context.target} delivers no hardware key actions to the page`);
	const tab = context.flags.tabKey;
	await context.go(`/${SHARED_PAGE}`);
	const reached = await context.tabToSummary();
	context.note(`${context.tabName()} reached the summary after ${reached.presses} presses`);
	// A browser draws a line in whole device pixels and reports the width it
	// drew, so at a fractional device pixel ratio 2 CSS pixels become the
	// whole device pixels below them: 1.83px at 1.09. The comparison is in
	// device pixels.
	const outline = (focus, what) => {
		const drawn = Math.round(focus.outlineWidth * focus.devicePixelRatio * 100) / 100;
		const needed = Math.floor(MIN_OUTLINE_PX * focus.devicePixelRatio);
		context.expect(focus.outlineStyle !== 'none' && drawn >= needed,
			`${what}: the focus outline is ${focus.outlineWidth}px ${focus.outlineStyle} at device pixel ratio ${focus.devicePixelRatio}, `
			+ `${drawn} device pixels where ${MIN_OUTLINE_PX}px needs ${needed}.`);
		context.note(`${what}: focus outline ${focus.outlineWidth}px ${focus.outlineStyle} at device pixel ratio ${focus.devicePixelRatio}`);
	};
	outline(reached.focus, 'the summary');
	context.expect(reached.focus.focusVisible, `the summary reached by ${context.tabName()} does not match :focus-visible.`);

	let before = await context.countOpenings();
	await pressKeys(context.session, Keys.Space);
	const state = await context.waitForList({ after: before }).catch(() => null);
	context.expect(state, 'Space did not open the selector.');
	await pressKeys(context.session, Keys.Escape);
	let focus = await context.script(FOCUS);
	context.expect(!focus.open && focus.index === 0, `after Escape the list is ${focus.open ? 'open' : 'closed'} and focus is on ${focus.tag} "${focus.text}".`);

	before = await context.countOpenings();
	await pressKeys(context.session, Keys.Enter);
	const opened = await context.waitForList({ after: before }).catch(() => null);
	if (!context.expect(opened, 'Enter did not open the selector.')) return;
	context.expect(opened.current?.text.startsWith('Development') && opened.current.ariaCurrent === 'page'
		&& opened.current.tag === 'SPAN' && !opened.current.insideLink,
	`the current version is ${JSON.stringify(opened.current)}, not plain text marked aria-current="page".`);
	context.expect(!opened.links.some((link) => link.label === 'Development'), 'the current version is also a link.');

	const stops = (await context.script(FOCUS)).stops;
	const down = [];
	for (let press = 0; press < stops; press += 1) {
		await pressKeys(context.session, Keys.ArrowDown);
		down.push(await context.script(FOCUS));
	}
	const expectedDown = [...Array(stops - 1).keys()].map((index) => index + 1).concat([stops - 1]);
	context.expect(down.map((entry) => entry.index).join() === expectedDown.join(),
		`ArrowDown moved focus through stops ${down.map((entry) => entry.index).join()}, expected ${expectedDown.join()}.`);
	outline(down[0], `the link "${down[0].text}"`);
	const up = [];
	for (let press = 0; press < stops; press += 1) {
		await pressKeys(context.session, Keys.ArrowUp);
		up.push((await context.script(FOCUS)).index);
	}
	const expectedUp = [...Array(stops - 1).keys()].map((index) => stops - 2 - index).concat([0]);
	context.expect(up.join() === expectedUp.join(), `ArrowUp moved focus through stops ${up.join()}, expected ${expectedUp.join()}.`);
	context.note(`${stops} stops; ArrowDown ${down.map((entry) => entry.index).join()}; ArrowUp ${up.join()}`);

	await pressKeys(context.session, Keys.Escape);
	focus = await context.script(FOCUS);
	context.expect(!focus.open && focus.index === 0, `after Escape from a link the list is ${focus.open ? 'open' : 'closed'} and focus is on ${focus.tag} "${focus.text}".`);

	before = await context.countOpenings();
	await pressKeys(context.session, Keys.Enter);
	if (!context.expect(await context.waitForList({ after: before }).catch(() => null), 'Enter did not reopen the selector.')) return;
	let left = null;
	for (let press = 0; press <= stops && !left; press += 1) {
		await pressKeys(context.session, tab);
		focus = await context.script(FOCUS);
		if (!focus.inside) left = focus;
		else context.expect(focus.open, `${context.tabName()} within the list closed it at ${focus.tag} "${focus.text}".`);
	}
	if (context.expect(left, `${context.tabName()} never left the selector.`)) {
		context.expect(!left.open, `focus left for ${left.tag} "${left.text}" but the list stayed open.`);
	}
}

// Qualification's "switched through all three versions in both directions
// with Tab, Enter and the arrow keys alone": no click anywhere.
async function keyboardSwitching(context) {
	if (!context.flags.keyboard) throw new Skip(`${context.target} delivers no hardware key actions to the page`);
	const route = SHARED_PAGE;
	await context.go(`/${route}${SHARED_HEADING}`);
	const order = [LATEST, OLDEST, 'Development', OLDEST, LATEST, 'Development'];
	// A page opened at a heading starts keyboard focus at that heading, after
	// the sidebar, so a reader goes back to the selector with Shift-Tab.
	// Going forward instead leaves the page for the browser's own controls,
	// where a window without focus ignores Enter on the summary.
	for (const label of order) {
		const { presses, key } = await context.tabToSummary({ backward: true });
		const before = await context.countOpenings();
		await pressKeys(context.session, Keys.Enter);
		const state = await context.waitForList({ after: before });
		let focus = null;
		for (let press = 0; press < state.links.length && focus?.label !== label; press += 1) {
			await pressKeys(context.session, Keys.ArrowDown);
			focus = await context.script(FOCUS);
		}
		if (focus?.label !== label) throw new Error(`ArrowDown never reached the ${label} link; focus is on ${focus?.tag} "${focus?.text}".`);
		const expected = context.abs(`${versionPath(label)}${route}${SHARED_HEADING}`);
		await context.takePageErrors();
		await pressKeys(context.session, Keys.Enter);
		const location = await context.settle(expected);
		const landed = await context.script(`
			return { heading: !!document.getElementById(arguments[0]),
				current: (document.querySelector('[data-version-select] summary strong') || {}).textContent };`, [SHARED_HEADING.slice(1)]);
		context.expect(landed.heading, `${label}: the page has no heading ${SHARED_HEADING}.`);
		context.expect(landed.current === label, `${label}: the selector names "${landed.current}".`);
		context.note(`${presses} × ${key}, Enter, ArrowDown to ${label}, Enter: ${location.href}`);
	}
}

async function phoneLayout(context) {
	const viewports = [];
	if (context.flags.phone) {
		viewports.push({ width: null, label: 'the device width' });
	} else {
		for (const width of PHONE_WIDTHS) viewports.push({ width, label: `${width} px` });
	}
	const pages = [
		{ path: `/${FIXTURE_ONLY}`, name: 'Development', choose: LATEST, lands: `${versionPath(LATEST)}${FIXTURE_ONLY}` },
		{ path: `${versionPath(OLDEST)}${SHARED_PAGE}`, name: OLDEST, choose: 'Development', lands: `/${SHARED_PAGE}` },
	];
	// A finger, where the target has one, rather than a mouse.
	context.touchInput = context.flags.touch;
	try {
		for (const theme of THEMES) {
			for (const viewport of viewports) {
				for (const page of pages) {
					const where = `${viewport.label}, ${theme}, ${page.name} ${page.path}`;
					await loadAtWidth(context, viewport.width, page.path, theme);
					let overflow = await context.script(OVERFLOW);
					if (viewport.width !== null) {
						context.expect(overflow.innerWidth === viewport.width, `${where}: the viewport is ${overflow.innerWidth} px wide.`);
					} else {
						viewport.label = `the device width (${overflow.innerWidth} px)`;
					}
					checkTheme(context, overflow, theme, where);
					checkOverflow(context, overflow, where);
					const menu = await context.session.findElement('button.sl-menu-button');
					const opened = await context.tapUntil(menu, () => context.script(MENU_OPEN), 'the phone menu to open');
					if (!context.expect(opened, `${where}: the menu button did not open the menu.`)) continue;
					checkSelectorPlacement(context, await context.script(SIDEBAR_ORDER), where, 'phone menu');
					const state = await context.openSelector();
					checkTargets(context, state, where);
					overflow = await context.script(OVERFLOW);
					checkOverflow(context, overflow, `${where}, selector open`);
					const link = await context.versionLink(page.choose);
					const location = await context.follow(link, context.abs(page.lands));
					const smallest = Math.min(state.summaryHeight, ...state.items.map((item) => item.height));
					context.note(`${where}: page ${overflow.scrollWidth}/${overflow.clientWidth} px wide with the selector open; `
						+ `smallest target ${smallest.toFixed(1)} px; ${context.touchInput || context.flags.phone ? 'tapped' : 'clicked'} ${page.choose}, landed on ${location.href}`);
				}
			}
		}
	} finally {
		context.touchInput = false;
		await restoreTheme(context);
	}
}

async function desktopLayout(context) {
	if (context.flags.phone) throw new Skip(`${context.target} is a phone; it has no desktop width`);
	if (!context.flags.setWindowRect) throw new Skip(`${context.target} cannot resize its window`);
	await context.session.setWindowRect({ ...DESKTOP_WINDOW });
	try {
		for (const theme of THEMES) {
			for (const page of [`/${FIXTURE_ONLY}`, `${versionPath(OLDEST)}${SHARED_PAGE}`]) {
				const where = `${page}, ${theme}`;
				await context.go(page);
				await context.useTheme(theme);
				let overflow = await context.script(OVERFLOW);
				context.note(`${where}: innerWidth ${overflow.innerWidth}, clientWidth ${overflow.clientWidth}`);
				context.expect(overflow.innerWidth >= DESKTOP_WINDOW.width - 40, `${where}: the window reached only ${overflow.innerWidth} px.`);
				checkTheme(context, overflow, theme, where);
				checkOverflow(context, overflow, where);
				checkSelectorPlacement(context, await context.script(SIDEBAR_ORDER), where, 'sidebar');
				await context.openSelector();
				overflow = await context.script(OVERFLOW);
				checkOverflow(context, overflow, `${where}, selector open`);
			}
		}
	} finally {
		await restoreTheme(context);
	}
}

const RELEASE_CARDS = `
	const text = (element) => element ? element.textContent.replace(/\\s+/g, ' ').trim() : null;
	return [...document.querySelectorAll('.version-list > .version-card')].map((card) => {
		const paragraphs = [...card.querySelectorAll('.version-commit')];
		const commit = paragraphs.find((p) => /^(Commit|Built from)/.test(text(p)));
		const archive = paragraphs.find((p) => /^Archive/.test(text(p)));
		const codes = archive ? archive.querySelectorAll('code') : [];
		return {
			id: card.querySelector('h2').id,
			heading: text(card.querySelector('h2')),
			facts: text(card.querySelector('.version-facts')),
			badges: [...card.querySelectorAll('.version-badge')].map((badge) => ({ text: text(badge), visible: badge.getBoundingClientRect().height > 0 })),
			links: [...card.querySelectorAll('.version-links a')].map((a) => ({ text: text(a), href: a.getAttribute('href') })),
			commit: commit && commit.querySelector('a') && { text: text(commit.querySelector('a')), href: commit.querySelector('a').getAttribute('href') },
			archive: archive && { name: text(archive.querySelector('a')), href: archive.querySelector('a').getAttribute('href'), sha256: text(codes[codes.length - 1]) },
			errata: [...card.querySelectorAll('.version-errata li')].map((li) => ({ text: text(li), href: li.querySelector('a') && li.querySelector('a').getAttribute('href') })),
		};
	});`;

async function releaseIndex(context) {
	const page = context.abs('/versions/');
	const served = await fetchText(page);
	context.expect(served.status === 200, `GET /versions/ answered ${served.status}.`);
	const servedOrder = [...served.text.matchAll(/<h2 id="([^"]+)"/g)].map((match) => match[1]).filter((id) => id !== 'starlight__on-this-page');
	const expectedOrder = ['development', ...RELEASES];
	context.expect(servedOrder.join() === expectedOrder.join(), `the served list's headings are ${servedOrder.join()}, expected ${expectedOrder.join()}.`);
	const index = JSON.parse((await fetchText(context.abs(INDEX_PATH))).text);
	const entries = new Map(index.versions.map((version) => [version.label, version]));
	await context.go('/versions/');
	const cards = await context.script(RELEASE_CARDS);
	context.expect(cards.map((card) => card.heading).join() === ['Development', ...RELEASES].join(),
		`the page lists ${cards.map((card) => card.heading).join()}, expected Development, ${RELEASES.join(', ')}.`);
	for (const card of cards) {
		const entry = entries.get(card.heading);
		if (!context.expect(entry, `${card.heading}: /versions/index.json has no such version.`)) continue;
		const where = card.heading;
		const fullCommit = /^[0-9a-f]{40}$/;
		context.expect(card.commit && card.commit.text === entry.commit && fullCommit.test(card.commit.text)
			&& card.commit.href.endsWith(`/commit/${entry.commit}`),
		`${where}: the commit is ${JSON.stringify(card.commit)}, expected the full ${entry.commit} linking to it.`);
		const docs = `${versionPath(card.heading)}docs/`;
		if (card.heading === 'Development') {
			context.expect(card.links.length === 1 && card.links[0].text === 'Documentation for Development' && card.links[0].href === docs,
				`${where}: links ${JSON.stringify(card.links)}, expected only "Documentation for Development" to ${docs}.`);
			context.expect(card.badges.length === 0 && !card.archive, `${where}: shows a badge or an archive.`);
			context.note(`Development: built from ${card.commit?.text}`);
			continue;
		}
		const tag = card.heading;
		context.expect(entry.latest ? card.badges.length === 1 && card.badges[0].text === 'Latest release' && card.badges[0].visible : card.badges.length === 0,
			`${where}: badges ${JSON.stringify(card.badges)}${entry.latest ? ', expected a visible "Latest release"' : ', expected none'}.`);
		context.expect(card.heading !== LATEST || entry.latest, `${LATEST} is not the latest release in /versions/index.json.`);
		context.expect(card.facts.includes(`Published ${entry.published};`) && /source of \d{4}-\d{2}-\d{2}\./.test(card.facts),
			`${where}: the dates read "${card.facts}", expected "Published ${entry.published}; source of <date>."`);
		const expectedLinks = [
			{ text: `Documentation for ${tag}`, href: docs },
			{ text: `Home page of ${tag}`, href: entry.path },
			{ text: `Source at ${tag}`, suffix: `/tree/${tag}` },
		];
		context.expect(card.links.length === expectedLinks.length
			&& expectedLinks.every((expected, at) => card.links[at].text === expected.text
				&& (expected.href ? card.links[at].href === expected.href : card.links[at].href.endsWith(expected.suffix))),
		`${where}: links ${JSON.stringify(card.links)}, expected ${JSON.stringify(expectedLinks)}.`);
		const asset = `labrador-docs-${tag}.tar.gz`;
		context.expect(card.archive && card.archive.name === asset && card.archive.href.endsWith(`/releases/download/${tag}/${asset}`)
			&& card.archive.sha256 === entry.sha256 && /^[0-9a-f]{64}$/.test(card.archive.sha256),
		`${where}: the archive is ${JSON.stringify(card.archive)}, expected ${asset} with SHA-256 ${entry.sha256}.`);
		const errata = card.heading === OLDEST ? 1 : 0;
		context.expect(card.errata.length === errata, `${where}: ${card.errata.length} errata, expected ${errata}.`);
		if (card.heading === OLDEST && card.errata[0]) {
			context.expect(card.errata[0].href === FIXTURE_ONLY_PATH, `${OLDEST}'s erratum links to ${card.errata[0].href}.`);
			context.expect(/^\d{4}-\d{2}-\d{2}: /.test(card.errata[0].text) && card.errata[0].text.includes(ERRATUM_TITLE),
				`${OLDEST}'s erratum reads "${card.errata[0].text}", expected a date and "${ERRATUM_TITLE}".`);
			context.note(`${OLDEST} erratum: ${card.errata[0].text}`);
		}
		context.note(`${tag}: ${card.facts} commit ${card.commit?.text}; ${card.archive?.name} ${card.archive?.sha256}`);
	}
	for (const label of ['Development', ...RELEASES]) {
		const docs = `${versionPath(label)}docs/`;
		const { status } = await fetchText(context.abs(docs));
		context.expect(status === 200, `GET ${docs} answered ${status}.`);
	}
	// The selector on the list, which belongs to no version, opens each
	// release's overview without reporting anything missing.
	const state = await context.openSelector();
	for (const label of RELEASES) {
		const link = state.links.find((candidate) => candidate.label === label);
		context.expect(link && link.rawHref === `${versionPath(label)}docs/` && !link.note,
			`the selector on /versions/ offers ${label} as ${JSON.stringify(link)}.`);
	}
	const docsLink = await context.script(`
		return [...document.querySelectorAll('.version-card')].find((card) => card.querySelector('h2').id === arguments[0])
			.querySelector('.version-links a');`, [OLDEST]);
	await context.hideSidebar();
	// Where the driver cannot touch below the first screen (touchAtPagePoint
	// in browsers.mjs, a WebKit bug), a link down there is not followed: its
	// address and target were checked above, and the tap waits for WebKit's
	// fix. Remove this when the driver touches where it is asked to.
	if (context.flags.touchAtPagePoint) {
		const place = await context.script(INTO_VIEW_FROM_TOP, [docsLink]);
		if (place.scrollY > 0) {
			context.note(`SKIPPED following ${OLDEST}'s documentation link: it is ${place.y} px down the page, below the first screen of `
				+ `${place.height} px, where ${context.target}'s driver places no touch where it is asked to (WebKit bug)`);
			return;
		}
	}
	const location = await context.follow(docsLink, context.abs(`${versionPath(OLDEST)}docs/`));
	context.note(`followed ${OLDEST}'s documentation link to ${location.href}`);
}

// Opens the search box on the current page and returns its input, ready for
// typing.
async function openSearch(context) {
	const button = await context.session.findElement('site-search button[data-open-modal]');
	await waitUntil(async () => !(await context.session.elementProperty(button, 'disabled')), { message: 'search to be enabled' });
	const input = await context.tapUntil(button, async () => {
		const found = await context.session.findElements('site-search dialog[open] .pagefind-ui__search-input');
		return found[0] ?? null;
	}, 'the search box', 15_000);
	if (!input) throw new Error('The search button did not open the search box.');
	// On a phone a reader taps the box before typing into it, and taps again
	// if the box did not take focus: text typed then would go elsewhere.
	if (context.flags.phone) {
		const focused = await context.tapUntil(input, () => context.script('return document.activeElement === arguments[0];', [input]),
			'the search box to take focus', 5_000);
		if (!focused) throw new Error('Tapping the search box did not give it focus.');
	}
	return input;
}

// Searches a version's site through its search box and returns every result,
// loading more until all are shown.
async function search(context, label, query) {
	await context.go(`${versionPath(label)}docs/`);
	const input = await openSearch(context);
	await context.typeAndConfirm(input, query, `the search box on ${label}`);
	const read = () => context.script(`
		const dialog = document.querySelector('site-search dialog');
		const message = dialog.querySelector('.pagefind-ui__message');
		const more = dialog.querySelector('.pagefind-ui__button');
		const links = [...dialog.querySelectorAll('.pagefind-ui__result-link')];
		return {
			message: message ? message.textContent.trim() : '',
			typed: dialog.querySelector('.pagefind-ui__search-input')?.value ?? null,
			more: !!more,
			pages: dialog.querySelectorAll('li.pagefind-ui__result').length,
			links: links.map((a) => ({ href: a.href, text: a.textContent.trim() })),
		};`);
	let last = null;
	let results = await waitUntil(async () => {
		const found = await read();
		last = found;
		if (!found.message.includes(query)) return null;
		if (/^No results/.test(found.message)) return { ...found, count: 0 };
		const count = /^(\d+) results?/.exec(found.message);
		return count && found.links.length > 0 ? { ...found, count: Number(count[1]) } : null;
	}, { timeoutMs: 15_000, message: `search results for "${query}" on ${label}` }).catch((error) => {
		throw new Error(`${error.message} The box held ${JSON.stringify(last?.typed)} and the message read ${JSON.stringify(last?.message)}.`);
	});
	// Each result is a page. The box shows five at a time behind "Load more
	// results", which loses presses while a page of results is arriving, so
	// every result's address is read instead from the Pagefind module the box
	// itself loaded: the same index, so "only this version's pages" covers all
	// of them, not just the first five.
	const index = await context.session.executeAsyncScript(`
		const done = arguments[arguments.length - 1];
		(async () => {
			const pagefind = await import(arguments[0]);
			const found = await pagefind.search(arguments[1]);
			const pages = await Promise.all(found.results.map((result) => result.data()));
			return pages.map((page) => new URL(page.url, location.href).href);
		})().then((urls) => done({ urls }), (error) => done({ error: String(error) }));`,
	[context.abs(`${versionPath(label)}pagefind/pagefind.js`), query]);
	if (index.error) throw new Error(`Pagefind's module on ${label} could not search "${query}": ${index.error}`);
	results = { ...results, all: index.urls };
	await pressKeys(context.session, Keys.Escape).catch(() => {});
	return results;
}

async function searchPerVersion(context) {
	for (const label of [OLDEST, LATEST, 'Development']) {
		const prefix = context.abs(versionPath(label));
		const inVersion = (link) => link.href.startsWith(prefix)
			&& !(label === 'Development' && link.href.startsWith(context.abs('/versions/')));
		const describe = (results) => `"${results.message}"; ${results.links.map((link) => `"${link.text}" ${link.href}`).join(', ')}`;

		// A page every version has: something is found, and only this version's.
		const shared = await search(context, label, SHARED_TITLE);
		context.expect(shared.count > 0, `${label}: search for "${SHARED_TITLE}" found nothing.`);
		context.expect(shared.all.length === shared.count, `${label}: the index gave ${shared.all.length} results, the box said ${shared.count}.`);
		// Every result, not only those the box shows: it fills its rows as
		// each page's data arrives, so the first rows shown need not be the
		// best matches yet.
		context.expect(shared.all.some((href) => href.split('#')[0] === `${prefix}${SHARED_PAGE}`),
			`${label}: search for "${SHARED_TITLE}" did not find ${prefix}${SHARED_PAGE}.`);
		const outside = [...shared.links, ...shared.all.map((href) => ({ href }))].filter((link) => !inVersion(link));
		context.expect(outside.length === 0, `${label}: results outside the version: ${outside.map((link) => link.href).join(', ')}.`);
		context.note(`${label}, "${SHARED_TITLE}": ${describe(shared)}`);

		// The page only some versions have.
		const only = await search(context, label, FIXTURE_ONLY_TITLE);
		const page = `${prefix}${FIXTURE_ONLY}`;
		const hit = only.all.includes(page);
		const onlyOutside = [...only.links, ...only.all.map((href) => ({ href }))].filter((link) => !inVersion(link));
		context.expect(onlyOutside.length === 0, `${label}: results outside the version: ${onlyOutside.map((link) => link.href).join(', ')}.`);
		if (label === OLDEST) {
			context.expect(!hit && !only.links.some((link) => link.text.startsWith(FIXTURE_ONLY_TITLE)),
				`${label}: search found the fixture-only guide, which ${label} lacks.`);
		} else {
			context.expect(hit, `${label}: search for "${FIXTURE_ONLY_TITLE}" did not find ${page}.`);
		}
		context.note(`${label}, "${FIXTURE_ONLY_TITLE}": ${describe(only)}`);
	}
	// /versions/ is not searchable: its erratum's title, which appears on no
	// other page, finds nothing there.
	const erratum = await search(context, 'Development', ERRATUM_TITLE);
	const listed = erratum.links.filter((link) => link.href.startsWith(context.abs('/versions/')));
	context.expect(listed.length === 0, `search found the release list: ${listed.map((link) => link.href).join(', ')}.`);
	context.note(`Development, "${ERRATUM_TITLE}": "${erratum.message}"`);
}

// Pressing Return, or Go on a phone's keyboard, in the search box keeps the
// reader on the page they searched from, whether the box is empty or holds a
// query. The search form must not submit anywhere: iOS Safari followed the
// form's former action, javascript:void(0), to an empty page (SITE#4), so the
// action is also read. A phone's keyboard is the Simulator's hardware keyboard.
const SEARCH_FORM = `
	const form = document.querySelector('site-search .pagefind-ui__form');
	const dialog = document.querySelector('site-search dialog');
	return {
		href: location.href,
		heading: document.querySelector('main h1')?.textContent.trim() ?? null,
		action: form ? form.getAttribute('action') : null,
		method: form ? form.getAttribute('method') : null,
		open: Boolean(dialog && dialog.open),
		typed: dialog?.querySelector('.pagefind-ui__search-input')?.value ?? null,
	};`;
const SEARCH_SUBMIT_WATCH_MS = 3_000;

async function searchSubmission(context) {
	if (!context.flags.keyboard && !context.flags.simulatorKeyboard) {
		throw new Skip(`${context.target} delivers no Return key to the page`);
	}
	const page = `${versionPath('Development')}docs/`;
	for (const query of ['', SHARED_TITLE]) {
		const described = query ? `"${query}"` : 'an empty box';
		await context.go(page);
		const input = await openSearch(context);
		if (query) await context.typeAndConfirm(input, query, 'the search box');
		const before = await context.script(SEARCH_FORM);
		context.expect(before.action === null || !/^\s*javascript:/i.test(before.action),
			`the search form's action is ${JSON.stringify(before.action)}, which a phone's Go can follow to an empty page.`);
		if (context.flags.simulatorKeyboard) await pressReturnOnSimulatorKeyboard();
		else await pressKeys(context.session, Keys.Enter);
		// Nothing should happen, so the page is watched for a while instead.
		const deadline = Date.now() + SEARCH_SUBMIT_WATCH_MS;
		let after = before;
		while (Date.now() < deadline) {
			await new Promise((resolve) => setTimeout(resolve, 200));
			after = await context.script(SEARCH_FORM).catch((error) => ({ href: `unreadable (${error.message})` }));
			if (after.href !== before.href || after.heading !== before.heading) break;
		}
		context.expect(after.href === before.href, `Return in ${described} left ${before.href} for ${after.href}.`);
		context.expect(after.heading === before.heading && Boolean(before.heading),
			`Return in ${described} changed the page's heading from ${JSON.stringify(before.heading)} to ${JSON.stringify(after.heading)}.`);
		context.note(`${described}: form action ${JSON.stringify(before.action)}, method ${JSON.stringify(before.method)}; `
			+ `after Return still at ${after.href}, search ${after.open ? 'open' : 'closed'}, box ${JSON.stringify(after.typed)}`);
		await pressKeys(context.session, Keys.Escape).catch(() => {});
	}
}

async function notFound(context) {
	const missing = `${versionPath(OLDEST)}nope/`;
	const href = context.abs(missing);
	const served = await fetchText(href);
	context.expect(served.status === 404, `GET ${missing} answered ${served.status}, not 404.`);
	context.expect(served.text.includes(`Documentation for Labrador ${OLDEST}.`), `the 404 body is not ${OLDEST}'s 404 page.`);
	context.allowed404.push(href);
	await context.go(missing);
	const seen = await context.script(`
		const revision = document.querySelector('.site-revision');
		const heading = document.querySelector('h1');
		return { title: document.title, revision: revision ? revision.innerText.trim() : null, heading: heading ? heading.textContent.trim() : null,
			canonical: (document.querySelector('link[rel="canonical"]') || {}).href || null };`);
	context.expect(seen.title.startsWith('404'), `the page's title is "${seen.title}".`);
	context.expect(seen.revision?.includes(`Documentation for Labrador ${OLDEST}.`), `the page's version line reads "${seen.revision}".`);
	context.expect(seen.canonical === `${CANONICAL_ORIGIN}${versionPath(OLDEST)}404/`, `the page's canonical is ${seen.canonical}.`);
	context.note(`shown: "${seen.heading}"; "${seen.revision}"`);
}

async function indexUnavailable(context) {
	const index = context.abs(INDEX_PATH, context.blockedBase);
	const { status } = await fetchText(index);
	context.expect(status === 404, `the blocking proxy answered ${status} for ${INDEX_PATH}.`);
	context.allowed404.push(index);
	await context.go(`/${SHARED_PAGE}`, { base: context.blockedBase });
	for (const attempt of ['first', 'second']) {
		const state = await context.openSelector({ staticOnly: true });
		context.expect(state.summaryText.includes('Development'), `${attempt} opening: the summary reads "${state.summaryText}".`);
		context.expect(state.links.length === 1 && state.links[0].rawHref === '/versions/' && state.links[0].text === 'All documentation versions',
			`${attempt} opening: the list is ${JSON.stringify(state.links)}, not the static link to /versions/.`);
		context.expect(state.items.length === 1, `${attempt} opening: the list has ${state.items.length} items.`);
		const summary = await context.session.findElement('[data-version-select] summary');
		await context.activate(summary);
		await waitUntil(async () => !(await context.selector()).open, { message: 'the selector to close' });
	}
	await context.openSelector({ staticOnly: true });
	const link = await context.session.findElement('[data-version-select] .version-choices a');
	await context.follow(link, context.abs('/versions/', context.blockedBase));
	context.note('the static link to /versions/ stayed and was followed');
}

// What a browser without JavaScript gets is the served markup itself.
async function staticMarkup(context) {
	const selectors = (html) => [...html.matchAll(/<div\b[^>]*\bdata-version-select\b[^>]*>/g)].length;
	const footerLink = (html) => /<p class="site-versions\b[^"]*"[^>]*><a href="\/versions\/" data-version-index\b/.test(html);
	for (const { path: urlPath, label } of [
		{ path: `/${SHARED_PAGE}`, label: 'Development' },
		{ path: `${versionPath(OLDEST)}${SHARED_PAGE}`, label: OLDEST },
		{ path: `${versionPath(LATEST)}${FIXTURE_ONLY}`, label: LATEST },
		{ path: '/versions/', label: 'Development' },
	]) {
		const { status, text } = await fetchText(context.abs(urlPath));
		if (!context.expect(status === 200, `GET ${urlPath} answered ${status}.`)) continue;
		context.expect(selectors(text) === 1, `${urlPath}: the served page has ${selectors(text)} version selectors, not one.`);
		const root = /<div\b[^>]*\bdata-version-select\b[^>]*>([\s\S]*?)<\/details>/.exec(text);
		if (context.expect(root, `${urlPath}: no <details> inside the selector.`)) {
			const summary = /<summary\b[^>]*>([\s\S]*?)<\/summary>/.exec(root[1]);
			const named = summary && /<strong\b[^>]*>([^<]*)<\/strong>/.exec(summary[1]);
			context.expect(named?.[1] === label, `${urlPath}: the served summary names ${named?.[1]}, not ${label}.`);
			const list = /<ul\b[^>]*class="version-choices\b[^"]*"[^>]*>([\s\S]*?)<\/ul>/.exec(root[1]);
			const items = list ? [...list[1].matchAll(/<li\b/g)].length : 0;
			const links = list ? [...list[1].matchAll(/<a\b[^>]*>/g)].map((match) => match[0]) : [];
			context.expect(items === 1 && links.length === 1 && /\bhref="\/versions\/"/.test(links[0]) && /\bdata-version-index\b/.test(links[0]),
				`${urlPath}: the served list has ${items} items and links ${JSON.stringify(links)}, not only the link to /versions/.`);
		}
		context.expect(footerLink(text), `${urlPath}: the footer has no link to /versions/.`);
	}
	// The splash pages have no sidebar, so no selector, but keep the footer link.
	for (const splash of ['/', `${versionPath(OLDEST)}`]) {
		const { status, text } = await fetchText(context.abs(splash));
		if (!context.expect(status === 200, `GET ${splash} answered ${status}.`)) continue;
		context.expect(!/\bid="starlight__sidebar"/.test(text), `${splash}: the splash page has a sidebar.`);
		context.expect(selectors(text) === 0, `${splash}: the splash page has a version selector.`);
		context.expect(footerLink(text), `${splash}: the footer has no link to /versions/.`);
	}
	// The list is not searchable; a documentation page is.
	const list = (await fetchText(context.abs('/versions/'))).text;
	const docs = (await fetchText(context.abs(`/${SHARED_PAGE}`))).text;
	context.expect(!/\bdata-pagefind-body\b/.test(list), '/versions/ is marked for the search index.');
	context.expect(/\bdata-pagefind-body\b/.test(docs), `/${SHARED_PAGE} is not marked for the search index, so the comparison proves nothing.`);
	// A browser shows the list's link as it was served.
	await context.go('/');
	const footer = await context.script(`
		const link = document.querySelector('.site-versions a[href="/versions/"]');
		return link ? { visible: link.getBoundingClientRect().height > 0, text: link.textContent.trim() } : null;`);
	context.expect(footer?.visible, `the home page's footer link to /versions/ is ${JSON.stringify(footer)}.`);
	context.note('served markup: one selector per page with only the /versions/ link; footer links on docs, list and splash pages');
}

// While the index is on its way the list says so and reports itself busy.
async function indexLoading(context) {
	await context.go(`/${SHARED_PAGE}`, { base: context.heldIndex.url });
	const before = await context.countOpenings();
	await context.revealSidebar();
	const summary = await context.session.findElement('[data-version-select] summary');
	await context.activate(summary);
	const loading = await waitUntil(async () => {
		const state = await context.selector();
		return state?.open && state.openings > before && state.checking ? state : null;
	}, { message: 'the list to show that it is checking versions' }).catch(() => null);
	if (context.expect(loading, 'the list never showed that it was checking versions.')) {
		context.expect(loading.busy, 'the list showing "Checking versions" is not aria-busy="true".');
		context.expect(loading.checking === 'Checking versions…', `the status reads "${loading.checking}".`);
		context.expect(loading.open, 'the summary does not report the list as expanded.');
		context.note(`while the index was held: "${loading.checking}", aria-busy ${loading.busy}`);
	}
	context.expect(context.heldIndex.held() > 0, 'no request for the index reached the proxy.');
	context.heldIndex.release();
	const state = await context.waitForList({ after: before });
	context.expect(!state.busy && !state.checking && state.links.length === RELEASES.length + 1,
		`after the index arrived the list is ${JSON.stringify({ busy: state.busy, checking: state.checking, links: state.links.length })}.`);
	context.note(`after release: ${state.items.map((item) => item.text).join(' | ')}`);
}

// axe-core's WCAG 2.x A and AA rules, run in the page itself. The scan covers
// every state the selector and notice add to a page: the open list with an
// absent page's note, the arrival notice, and the release index with the list
// open, each at a phone's width and at 1440 pixels in both themes. Rules
// beyond A and AA (axe's best practices) are left out: they judge Starlight's
// page structure rather than what versioning adds. The header names
// Starlight's search button by what it shows, "Search Ctrl K", so its label
// is in its name (label-content-name-mismatch). A published snapshot keeps the
// header it was built with, and v0.3.0's predates that, so on a frozen page
// the search button is left out of the scan; every page built from this
// checkout includes it.
const AXE_SOURCE = readFileSync(createRequire(import.meta.url).resolve('axe-core/axe.min.js'), 'utf8');
const AXE_TAGS = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];
const AXE_RUN = `
	const done = arguments[arguments.length - 1];
	window.axe.run(arguments[1] ? { exclude: [['site-search']] } : document, { runOnly: { type: 'tag', values: arguments[0] }, resultTypes: ['violations'] })
		.then((result) => done({
			version: window.axe.version,
			violations: result.violations.map((violation) => ({
				id: violation.id,
				impact: violation.impact,
				nodes: violation.nodes.slice(0, 5).map((node) => ({ target: node.target.join(' '), summary: node.failureSummary })),
			})),
		}), (error) => done({ error: String(error) }));`;

async function axeScan(context, where, { frozen = false } = {}) {
	await context.script(`${AXE_SOURCE}
return typeof window.axe;`);
	const result = await context.session.executeAsyncScript(AXE_RUN, [AXE_TAGS, frozen]);
	if (result.error) throw new Error(`${where}: axe failed: ${result.error}`);
	for (const violation of result.violations) {
		context.expect(false, `${where}: ${violation.id} (${violation.impact}) at ${violation.nodes.map((node) => `${node.target}: ${node.summary}`).join(' | ')}`);
	}
	context.note(`${where}: axe ${result.version}, ${result.violations.length} violations`);
}

async function accessibilityScan(context) {
	const viewports = [];
	if (context.flags.phone) viewports.push({ width: null, label: 'the device width' });
	else viewports.push({ width: 360, label: '360 px' });
	if (!context.flags.phone && context.flags.setWindowRect) viewports.push({ width: DESKTOP_WINDOW.width, label: '1440 px', desktop: true });
	try {
		for (const theme of THEMES) {
			for (const viewport of viewports) {
				const load = async (urlPath, landsAt) => {
					if (viewport.desktop) {
						await topFrame(context);
						await context.session.setWindowRect({ ...DESKTOP_WINDOW });
						await loadInTheme(context, urlPath, theme, landsAt);
					} else {
						await loadAtWidth(context, viewport.width, urlPath, theme, landsAt);
					}
					checkTheme(context, await context.script(OVERFLOW), theme, `${viewport.label}, ${urlPath}`);
				};
				const at = `${viewport.label}, ${theme}`;

				// The open list, with the absent page's note on v0.3.0.
				await load(`/${FIXTURE_ONLY}`);
				const open = await context.openSelector();
				const missing = open.links.find((link) => link.label === OLDEST);
				context.expect(missing?.note, `${at}: the ${OLDEST} link carries no note to scan.`);
				await axeScan(context, `${at}, ${FIXTURE_ONLY_PATH} with the list open`);

				// The arrival notice, reached through that link's address.
				await load(missing.rawHref, missing.rawHref.split('?')[0]);
				await waitUntil(async () => (await context.script(NOTICE))?.text, { message: 'the arrival notice' });
				await axeScan(context, `${at}, ${OLDEST}'s overview with the arrival notice`, { frozen: true });

				// The release index with the list open.
				await load('/versions/');
				await context.openSelector();
				await axeScan(context, `${at}, /versions/ with the list open`);
			}
		}
	} finally {
		await topFrame(context).catch(() => {});
		await restoreWindow(context);
		await restoreTheme(context);
	}
}

export const CHECKS = [
	{ id: 1, name: 'shared page and heading', run: sharedPage },
	{ id: 2, name: 'absent page', run: absentPage },
	{ id: 3, name: 'notice is announceable', run: noticeAnnounceable },
	{ id: 4, name: 'keyboard', run: keyboard },
	{ id: 5, name: 'phone layout', run: phoneLayout },
	{ id: 6, name: 'desktop layout', run: desktopLayout },
	{ id: 7, name: 'release index', run: releaseIndex },
	{ id: 8, name: 'search per version', run: searchPerVersion },
	{ id: 9, name: '404', run: notFound },
	{ id: 10, name: 'index unavailable', run: indexUnavailable },
	{ id: 11, name: 'keyboard switching', run: keyboardSwitching },
	{ id: 12, name: 'static markup', run: staticMarkup },
	{ id: 13, name: 'index loading', run: indexLoading },
	{ id: 14, name: 'accessibility scan', run: accessibilityScan },
	{ id: 15, name: 'search submission', run: searchSubmission },
];

// ---------------------------------------------------------------------------
// Serving.

// A proxy for the fixture at `upstream` that changes only INDEX_PATH: 'block'
// answers 404, as when the development site's index cannot be fetched;
// 'hold' keeps each request waiting until release(), after which requests
// pass. held() counts the requests held so far.
export async function startIndexProxy(upstream, mode) {
	const target = new URL(upstream);
	const client = target.protocol === 'https:' ? https : http;
	const waiting = [];
	let heldCount = 0;
	let released = false;
	const forward = (request, response) => {
		const outgoing = client.request({
			protocol: target.protocol, hostname: target.hostname, port: target.port, method: request.method, path: request.url,
			headers: { ...request.headers, host: target.host },
		}, (answer) => {
			response.writeHead(answer.statusCode, answer.headers);
			answer.pipe(response);
		});
		outgoing.on('error', () => response.destroy());
		if (request.method === 'GET' || request.method === 'HEAD') outgoing.end();
		else request.pipe(outgoing);
	};
	const server = http.createServer((request, response) => {
		if (request.url.split(/[?#]/)[0] === INDEX_PATH) {
			if (mode === 'block') {
				response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8', 'Cache-Control': 'no-store' });
				response.end('Withheld by the acceptance suite\n');
				return;
			}
			if (!released) {
				heldCount += 1;
				waiting.push(() => forward(request, response));
				return;
			}
		}
		forward(request, response);
	});
	const sockets = new Set();
	server.on('connection', (socket) => {
		sockets.add(socket);
		socket.on('close', () => sockets.delete(socket));
	});
	await new Promise((resolve, reject) => {
		server.once('error', reject);
		server.listen(0, '127.0.0.1', resolve);
	});
	let closed = false;
	return {
		url: `http://127.0.0.1:${server.address().port}/`,
		held: () => heldCount,
		release() {
			released = true;
			for (const send of waiting.splice(0)) send();
		},
		async close() {
			if (closed) return;
			closed = true;
			await new Promise((resolve) => {
				server.close(() => resolve());
				for (const socket of sockets) socket.destroy();
			});
		},
	};
}

// ---------------------------------------------------------------------------
// Running.

function parseArguments(argv) {
	const options = {};
	const valued = new Set(['--target', '--base', '--serve-dist', '--only', '--driver', '--browser', '--udid', '--report']);
	for (let index = 0; index < argv.length; index += 1) {
		const name = argv[index];
		if (!valued.has(name)) throw new Error(`Unknown argument ${name}.`);
		const value = argv[index + 1];
		if (value === undefined) throw new Error(`${name} needs a value.`);
		options[name.slice(2)] = value;
		index += 1;
	}
	if (!TARGETS[options.target]) throw new Error(`--target must be one of ${Object.keys(TARGETS).join(', ')}.`);
	if (Boolean(options.base) === Boolean(options['serve-dist'])) throw new Error('Give exactly one of --base and --serve-dist.');
	if (options.only) {
		options.only = options.only.split(',').map((part) => Number(part.trim()));
		const unknown = options.only.filter((id) => !CHECKS.some((check) => check.id === id));
		if (unknown.length) throw new Error(`--only names no check ${unknown.join(', ')}.`);
	}
	return options;
}

function seconds(ms) {
	return `${(ms / 1000).toFixed(1)} s`;
}

// Runs one check once, setting result.status (and result.reason for a skip)
// and leaving its failures in context.failures.
async function runCheck(check, context, result) {
	try {
		await restoreWindow(context);
		await context.drainConsole();
		await check.run(context);
		await context.takePageErrors();
		const errors = [...context.pageErrors, ...(await context.consoleErrors())];
		for (const error of errors) context.failures.push(`page error: ${error}`);
		result.status = context.failures.length ? 'fail' : 'pass';
	} catch (error) {
		if (error instanceof Skip) {
			result.status = 'skip';
			result.reason = error.message;
		} else {
			result.status = 'fail';
			context.failures.push(describeError(error));
		}
	}
}

// The cleanups of runs in progress, for a signal to call.
const running = new Set();

export async function stopAll() {
	await Promise.all([...running].map((cleanup) => cleanup()));
}

export async function runSuite(options, { write = (line) => process.stdout.write(`${line}\n`), log = () => {} } = {}) {
	const servers = [];
	let browser;
	let cleaned = false;
	const cleanup = async () => {
		if (cleaned) return;
		cleaned = true;
		if (browser) await browser.stop();
		for (const server of servers) await server.close();
	};
	running.add(cleanup);
	const results = [];
	const report = { target: options.target, startedAt: new Date().toISOString(), results };
	try {
		let base = options.base;
		if (options['serve-dist']) {
			const server = await startServer({ root: options['serve-dist'], port: 0 });
			servers.push(server);
			base = server.url;
		}
		base = new URL(base).href;
		report.base = base;
		const blocked = await startIndexProxy(base, 'block');
		servers.push(blocked);
		const heldIndex = await startIndexProxy(base, 'hold');
		servers.push(heldIndex);
		browser = await startBrowser(options.target, { driver: options.driver, binary: options.browser, udid: options.udid, log });
		if (cleaned) throw new Error('Stopped before the checks began.');
		const { session } = browser;
		report.browser = { name: session.capabilities.browserName, version: session.capabilities.browserVersion, platform: session.capabilities.platformName };
		report.consoleErrors = options.target === 'chrome'
			? "Chrome's console log through the driver"
			: 'not exposed by this driver; an in-page listener records errors raised after each load';
		write(`target ${options.target}: ${report.browser.name} ${report.browser.version} on ${report.browser.platform}; base ${base}`);
		write(`console errors: ${report.consoleErrors}`);
		const context = new Context({ session, target: options.target, flags: browser.flags, base, blockedBase: blocked.url, heldIndex, log });
		for (const check of CHECKS) {
			if (options.only && !options.only.includes(check.id)) continue;
			if (cleaned) break;
			const started = Date.now();
			const result = { id: check.id, name: check.name };
			const runs = context.flags.retryLostInput ? 2 : 1;
			for (let run = 1; run <= runs; run += 1) {
				const earlier = run > 1 ? { failures: context.failures, details: context.details } : null;
				context.reset();
				if (earlier) {
					context.note(`run 1 failed and the check was run again: ${earlier.failures.join(' | ')}`);
					for (const detail of earlier.details) context.note(`run 1: ${detail}`);
					// A failed run can leave the Simulator's Safari taking no
					// taps, so the second runs in a new session of a restarted
					// Safari.
					if (browser.device) {
						await browser.stop();
						await restartSimulatorSafari(browser.device.udid, log);
						browser = await startBrowser(options.target, { driver: options.driver, binary: options.browser, udid: browser.device.udid, log });
						context.session = browser.session;
						context.frame = null;
						context.note('the second run used a new session of a restarted Safari');
					}
				}
				await runCheck(check, context, result);
				result.runs = run;
				if (result.status !== 'fail' || cleaned) break;
			}
			// What the browser showed when a check failed, beside the report.
			if (result.status === 'fail' && options.report) {
				const shot = path.join(path.dirname(options.report), `fail-${check.id}.png`);
				mkdirSync(path.dirname(shot), { recursive: true });
				await context.session.screenshot().then((png) => {
					writeFileSync(shot, png);
					result.screenshot = path.basename(shot);
				}).catch(() => {});
			}
			result.failures = context.failures;
			result.details = context.details;
			result.durationMs = Date.now() - started;
			results.push(result);
			const again = result.runs > 1 ? ', on its second run' : '';
			const head = `${result.status.toUpperCase()} ${check.id} ${check.name} (${seconds(result.durationMs)}${again})`;
			if (result.status === 'skip') write(`${head}: ${result.reason}`);
			else if (result.status === 'fail') write(`${head}:\n${result.failures.map((failure) => `    - ${failure}`).join('\n')}`);
			else write(head);
		}
	} finally {
		await cleanup();
		running.delete(cleanup);
		report.finishedAt = new Date().toISOString();
		if (options.report) mkdirSync(path.dirname(path.resolve(options.report)), { recursive: true });
		if (options.report) writeFileSync(options.report, `${JSON.stringify(report, null, 2)}\n`);
	}
	const failed = results.filter((result) => result.status === 'fail').length;
	const skipped = results.filter((result) => result.status === 'skip').length;
	write(`${results.length - failed - skipped} passed, ${failed} failed, ${skipped} skipped`);
	return report;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	let options;
	try {
		options = parseArguments(process.argv.slice(2));
	} catch (error) {
		process.stderr.write(`${error.message}\nUsage: node acceptance/suite.mjs --target chrome|firefox|safari|ios `
			+ '(--base <url> | --serve-dist <dist>) [--only 1,2] [--driver <path>] [--browser <path>] [--udid <udid>] [--report <file>]\n');
		process.exit(2);
	}
	// A cancelled CI step or Ctrl+C still stops the driver and its browser.
	for (const [signal, code] of [['SIGINT', 130], ['SIGTERM', 143]]) {
		process.once(signal, () => {
			process.stderr.write(`${signal}: stopping the browser, its driver and the servers\n`);
			stopAll().finally(() => process.exit(code));
		});
	}
	try {
		const report = await runSuite(options, { log: (line) => process.stderr.write(`${line}\n`) });
		if (report.results.some((result) => result.status === 'fail')) process.exitCode = 1;
	} catch (error) {
		process.stderr.write(`${error instanceof Error ? error.stack ?? error.message : String(error)}\n`);
		process.exitCode = 1;
	}
}
