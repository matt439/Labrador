// Starts a WebDriver driver process for one acceptance target and opens a
// session in it. Targets:
//   chrome   chromedriver, headless Chrome
//   firefox  geckodriver, headless Firefox
//   safari   safaridriver, desktop Safari (macOS; `sudo safaridriver --enable` once)
//   ios      safaridriver, Safari in a booted iOS Simulator
// A driver is found from an explicit path, then CHROMEWEBDRIVER or
// GECKOWEBDRIVER (the folders GitHub's runners set), then PATH. A browser
// binary may be named by LABRADOR_ACCEPTANCE_CHROME or
// LABRADOR_ACCEPTANCE_FIREFOX; each must match its driver's major version.
// The ios target uses LABRADOR_IOS_UDID, or else the first available iPhone
// of the newest installed iOS runtime, booting it and waiting for its boot to
// finish; a simulator is left booted afterwards.
// stop() deletes the session and then ends the driver process started here
// together with any browser it started, which survives the driver otherwise.
import { execFile, spawn, spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { promisify } from 'node:util';
import { DEFAULT_SESSION_TIMEOUT_MS, Keys, newSession, status } from './webdriver.mjs';

const execFileAsync = promisify(execFile);

// What each target can be asked to do. The suite skips a check whose need is
// absent and prints the reason.
//   keyboard       key actions reach the page as hardware keys
//   setWindowRect  the window can be resized to a given viewport; headless
//                  Chrome and Firefox clamp the width to at least 500 CSS
//                  pixels, so a narrower viewport cannot be reached this way
//   touch          pointer actions with pointerType "touch" are delivered;
//                  the suite taps rather than clicks at phone widths
//   phone          the page runs on a phone's browser and screen
// startBrowser adds tabKey, the key or chord that moves focus to the next
// link: Tab, except in a Safari whose "Press Tab to highlight each item on a
// webpage" setting is off (its default), where Tab skips links and Option-Tab
// is what reaches them.
export const TARGETS = Object.freeze({
	chrome: Object.freeze({ keyboard: true, setWindowRect: true, touch: true, phone: false }),
	firefox: Object.freeze({ keyboard: true, setWindowRect: true, touch: true, phone: false }),
	safari: Object.freeze({ keyboard: true, setWindowRect: true, touch: false, phone: false }),
	ios: Object.freeze({ keyboard: false, setWindowRect: false, touch: true, phone: true }),
});

export const DEFAULT_WINDOW = Object.freeze({ width: 1280, height: 800 });
export const DRIVER_READY_TIMEOUT_MS = 20_000;
export const SIMULATOR_BOOT_TIMEOUT_MS = 300_000;
const DRIVER_START_ATTEMPTS = 3;
const OUTPUT_LIMIT = 8_192;

const DRIVERS = Object.freeze({
	chrome: { name: 'chromedriver', folderVariable: 'CHROMEWEBDRIVER' },
	firefox: { name: 'geckodriver', folderVariable: 'GECKOWEBDRIVER' },
	safari: { name: 'safaridriver' },
	ios: { name: 'safaridriver' },
});

function executableName(name, platform) {
	return platform === 'win32' ? `${name}.exe` : name;
}

// The driver to spawn: an explicit path, the runner's folder variable when it
// holds the driver, or the bare name for PATH lookup.
export function resolveDriver(target, { driver, env = process.env, platform = process.platform } = {}) {
	const entry = DRIVERS[target];
	if (!entry) throw new Error(`Unknown target "${target}"; expected one of ${Object.keys(TARGETS).join(', ')}.`);
	if (driver) return driver;
	const folder = entry.folderVariable ? env[entry.folderVariable] : undefined;
	if (folder) {
		const candidate = path.join(folder, executableName(entry.name, platform));
		if (existsSync(candidate)) return candidate;
	}
	return executableName(entry.name, platform);
}

export function driverArguments(target, port) {
	switch (target) {
		case 'chrome':
			return [`--port=${port}`];
		case 'firefox':
			// A BiDi socket on a chosen port lets two geckodrivers run side by side.
			return ['--port', String(port), '--host', '127.0.0.1', '--websocket-port', '0'];
		case 'safari':
		case 'ios':
			return ['-p', String(port)];
		default:
			throw new Error(`Unknown target "${target}".`);
	}
}

// The alwaysMatch capabilities for a target. `binary` is a browser executable;
// `udid` is the iOS Simulator device.
export function targetCapabilities(target, {
	binary, udid, window = DEFAULT_WINDOW, env = process.env, platform = process.platform,
} = {}) {
	switch (target) {
		case 'chrome': {
			const args = ['--headless=new', `--window-size=${window.width},${window.height}`];
			// A Linux CI container has no user namespace for Chrome's sandbox;
			// everywhere else the sandbox stays on.
			if (platform === 'linux' && env.CI) args.push('--no-sandbox');
			const options = { args };
			const chosen = binary ?? env.LABRADOR_ACCEPTANCE_CHROME;
			if (chosen) options.binary = chosen;
			return { browserName: 'chrome', 'goog:chromeOptions': options };
		}
		case 'firefox': {
			const options = { args: ['-headless'] };
			const chosen = binary ?? env.LABRADOR_ACCEPTANCE_FIREFOX;
			if (chosen) options.binary = chosen;
			return { browserName: 'firefox', 'moz:firefoxOptions': options };
		}
		case 'safari':
			return { browserName: 'safari' };
		case 'ios':
			if (!udid) throw new Error('The ios target needs a simulator UDID.');
			return { browserName: 'safari', platformName: 'iOS', 'safari:useSimulator': true, 'safari:deviceUDID': udid };
		default:
			throw new Error(`Unknown target "${target}".`);
	}
}

function runtimeVersion(identifier) {
	const match = /SimRuntime\.iOS-(\d+)-(\d+)(?:-(\d+))?$/.exec(identifier);
	return match ? match.slice(1).map((part) => Number(part ?? 0)) : null;
}

function compareVersions(left, right) {
	for (let index = 0; index < Math.max(left.length, right.length); index += 1) {
		const difference = (left[index] ?? 0) - (right[index] ?? 0);
		if (difference !== 0) return difference;
	}
	return 0;
}

// From `xcrun simctl list devices available --json`, the newest iOS runtime's
// first available iPhone, or null.
export function chooseIosDevice(list) {
	const runtimes = Object.entries(list?.devices ?? {})
		.map(([identifier, devices]) => ({ identifier, version: runtimeVersion(identifier), devices }))
		.filter((runtime) => runtime.version !== null && Array.isArray(runtime.devices))
		.sort((left, right) => compareVersions(right.version, left.version));
	for (const runtime of runtimes) {
		const device = runtime.devices.find((candidate) =>
			candidate.isAvailable !== false && typeof candidate.name === 'string' && candidate.name.startsWith('iPhone'));
		if (device) return { udid: device.udid, name: device.name, state: device.state, runtime: runtime.identifier };
	}
	return null;
}

async function simctl(args, timeoutMs) {
	const { stdout } = await execFileAsync('xcrun', ['simctl', ...args], { timeout: timeoutMs, maxBuffer: 16 * 1024 * 1024 });
	return stdout;
}

function findDevice(list, udid) {
	for (const [runtime, devices] of Object.entries(list?.devices ?? {})) {
		const device = (devices ?? []).find((candidate) => candidate.udid === udid);
		if (device) return { udid, name: device.name, state: device.state, runtime };
	}
	return null;
}

// Chooses and boots the simulator, returning { udid, name, runtime }.
export async function prepareSimulator({ udid = process.env.LABRADOR_IOS_UDID, log = () => {} } = {}) {
	const list = JSON.parse(await simctl(['list', 'devices', 'available', '--json'], 60_000));
	const device = udid ? findDevice(list, udid) : chooseIosDevice(list);
	if (!device) {
		throw new Error(udid ? `No available simulator has UDID ${udid}.` : 'No available iPhone simulator was found.');
	}
	log(`simulator ${device.name} (${device.udid}, ${device.runtime}) is ${device.state}`);
	if (device.state !== 'Booted') await simctl(['boot', device.udid], 120_000);
	await simctl(['bootstatus', device.udid, '-b'], SIMULATOR_BOOT_TIMEOUT_MS);
	return device;
}

// Whether desktop Safari's Tab moves to links. LABRADOR_SAFARI_TAB_TO_LINKS
// (1 or 0) overrides what Safari's preferences say; a preference that cannot
// be read is taken as Safari's default, off.
export async function safariTabsToLinks({ env = process.env, read = readDefault } = {}) {
	if (env.LABRADOR_SAFARI_TAB_TO_LINKS === '1') return true;
	if (env.LABRADOR_SAFARI_TAB_TO_LINKS === '0') return false;
	// Sandboxed Safari keeps its preferences in its container.
	const domains = [
		path.posix.join(env.HOME ?? '', 'Library/Containers/com.apple.Safari/Data/Library/Preferences/com.apple.Safari'),
		'com.apple.Safari',
	];
	const keys = ['WebKitTabToLinksPreferenceKey', 'com.apple.Safari.ContentPageGroupIdentifier.WebKit2TabsToLinks'];
	for (const domain of domains) {
		for (const key of keys) {
			const value = await read(domain, key);
			if (value !== null) return value === '1' || value === 'true';
		}
	}
	return false;
}

async function readDefault(domain, key) {
	try {
		const { stdout } = await execFileAsync('defaults', ['read', domain, key], { timeout: 10_000 });
		return stdout.trim();
	} catch {
		return null;
	}
}

export function freePort() {
	return new Promise((resolve, reject) => {
		const server = net.createServer();
		server.unref();
		server.once('error', reject);
		server.listen(0, '127.0.0.1', () => {
			const { port } = server.address();
			server.close(() => resolve(port));
		});
	});
}

function tail(text) {
	return text.length > OUTPUT_LIMIT ? text.slice(-OUTPUT_LIMIT) : text;
}

function waitForExit(child, timeoutMs) {
	if (child.exitCode !== null || child.signalCode !== null) return Promise.resolve(true);
	return new Promise((resolve) => {
		const timer = setTimeout(() => resolve(false), timeoutMs);
		child.once('exit', () => {
			clearTimeout(timer);
			resolve(true);
		});
	});
}

// Ends the driver and every process it started, found from the driver's own
// process ID only. On Windows a killed driver leaves its browser running, so
// the whole tree is ended; elsewhere the driver leads its own process group.
export async function killDriver(child, { log = () => {} } = {}) {
	if (child.exitCode === null && child.signalCode === null && child.pid !== undefined) {
		if (process.platform === 'win32') {
			spawnSync('taskkill', ['/PID', String(child.pid), '/T', '/F'], { stdio: 'ignore', windowsHide: true, timeout: 30_000 });
		} else {
			try {
				process.kill(-child.pid, 'SIGTERM');
			} catch {
				child.kill();
			}
		}
	}
	if (!(await waitForExit(child, 10_000))) {
		child.kill('SIGKILL');
		if (!(await waitForExit(child, 5_000))) log(`driver process ${child.pid} did not exit after kill`);
	}
}

// Spawns the driver and waits until GET /status reports it ready. Returns
// { url, child, output() }; on failure the process is killed. `target` picks
// the command line; msedgedriver takes chromedriver's.
export async function startDriver(executable, target, { log = () => {}, readyTimeoutMs = DRIVER_READY_TIMEOUT_MS } = {}) {
	let lastError;
	for (let attempt = 1; attempt <= DRIVER_START_ATTEMPTS; attempt += 1) {
		const port = await freePort();
		const url = `http://127.0.0.1:${port}`;
		const child = spawn(executable, driverArguments(target, port), {
			stdio: ['ignore', 'pipe', 'pipe'], windowsHide: true, detached: process.platform !== 'win32',
		});
		let output = '';
		const collect = (chunk) => {
			output = tail(output + chunk.toString());
		};
		child.stdout.on('data', collect);
		child.stderr.on('data', collect);
		let spawnError;
		child.once('error', (error) => {
			spawnError = error;
		});
		const deadline = Date.now() + readyTimeoutMs;
		while (Date.now() < deadline) {
			if (spawnError) {
				throw new Error(`Could not start ${executable}: ${spawnError.message}`, { cause: spawnError });
			}
			if (child.exitCode !== null || child.signalCode !== null) break;
			try {
				const value = await status(url);
				if (value?.ready === true) {
					log(`${path.basename(executable)} ready at ${url}`);
					return { url, child, output: () => output };
				}
			} catch {
				// Not listening yet.
			}
			await new Promise((resolve) => setTimeout(resolve, 100));
		}
		if (spawnError) throw new Error(`Could not start ${executable}: ${spawnError.message}`, { cause: spawnError });
		const exited = child.exitCode !== null || child.signalCode !== null;
		await killDriver(child, { log });
		lastError = new Error(exited
			? `${executable} exited (${child.exitCode ?? child.signalCode}) before it was ready:\n${output}`
			: `${executable} was not ready within ${readyTimeoutMs} ms:\n${output}`);
		// Only an early exit can be a port taken between choosing and binding it.
		if (!exited) break;
		log(`attempt ${attempt}: ${lastError.message}`);
	}
	throw lastError;
}

// Returns { target, flags, session, driverUrl, device, stop() }. `device` is
// the simulator for the ios target.
export async function startBrowser(target, {
	driver, binary, udid, window = DEFAULT_WINDOW, log = () => {},
	readyTimeoutMs = DRIVER_READY_TIMEOUT_MS, sessionTimeoutMs = DEFAULT_SESSION_TIMEOUT_MS, commandTimeoutMs,
} = {}) {
	const flags = TARGETS[target];
	if (!flags) throw new Error(`Unknown target "${target}"; expected one of ${Object.keys(TARGETS).join(', ')}.`);
	const device = target === 'ios' ? await prepareSimulator({ udid, log }) : null;
	const capabilities = targetCapabilities(target, { binary, udid: device?.udid, window });
	const executable = resolveDriver(target, { driver });
	const tabKey = target === 'safari' && !(await safariTabsToLinks()) ? [Keys.Alt, Keys.Tab] : Keys.Tab;
	const started = await startDriver(executable, target, { log, readyTimeoutMs });
	const stopDriver = () => killDriver(started.child, { log });
	let session;
	try {
		session = await newSession(started.url, capabilities, { timeoutMs: sessionTimeoutMs, commandTimeoutMs });
		if (flags.setWindowRect) await session.setWindowRect({ width: window.width, height: window.height });
	} catch (error) {
		if (session) await session.deleteSession().catch(() => {});
		await stopDriver();
		const output = started.output().trim();
		throw output ? new Error(`${error.message}\n${path.basename(executable)} output:\n${output}`, { cause: error }) : error;
	}
	let stopped = false;
	return {
		target,
		flags: Object.freeze({ ...flags, tabKey }),
		session,
		driverUrl: started.url,
		device,
		async stop() {
			if (stopped) return;
			stopped = true;
			try {
				await session.deleteSession();
			} catch (error) {
				log(`deleting the session failed: ${error.message}`);
			}
			await stopDriver();
		},
	};
}
