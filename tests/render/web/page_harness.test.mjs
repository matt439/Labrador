// page_harness.mjs's shared pieces against fixtures: no browser, driver or
// network.
import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
import { SAMPLES, WEBDRIVER_KEYS, deadline_after, exited, key_actions, key_table,
	module_hook } from './page_harness.mjs';

// The root CMakeLists.txt's Chromium entries for the same pages, as written:
// each labrador_sample_page_test's name, target and quit keys, and the
// samples and names the Firefox entries are made from.
function cmake_sample_pages() {
	const text = readFileSync(fileURLToPath(new URL('../../../CMakeLists.txt', import.meta.url)), 'utf8');
	const chromium = new Map();
	for (const [, name, target, keys] of text.matchAll(/labrador_sample_page_test\(\s*(\w+)\s+(\w+)\s+"([^"]*)"/g)) {
		chromium.set(name, { target, keys });
	}
	const list = (variable) => {
		const found = new RegExp(`set\\(${variable}\\s+([^)]*)\\)`).exec(text);
		assert.ok(found, `CMakeLists.txt sets no ${variable}`);
		return found[1].trim().split(/\s+/);
	};
	return { chromium, samples: list('labrador_firefox_samples'), names: list('labrador_firefox_names') };
}

test('each sample\'s target and quit keys agree with its Chromium and Firefox ctest entries', () => {
	const { chromium, samples, names } = cmake_sample_pages();
	assert.deepEqual(samples, Object.keys(SAMPLES));
	assert.equal(names.length, samples.length);
	samples.forEach((sample, index) => {
		const entry = chromium.get(`${names[index]}PageTests`);
		assert.ok(entry, `CMakeLists.txt registers no ${names[index]}PageTests for ${sample}`);
		assert.equal(entry.target, SAMPLES[sample].target, sample);
		assert.equal(entry.keys, SAMPLES[sample].keys.join(','), sample);
	});
});

function hooked(marker = 'labradorFixtureHooked', extra = '') {
	const context = vm.createContext({});
	context.window = context;
	vm.runInContext(`const heard = { initialized: false, quit: false, errors: [], has_quit: false };
		window.heard = heard;
		${module_hook(marker, extra)}
		window.hooked_module = () => module;`, context);
	return context;
}

test('the Module hook hears each callback before the page\'s own, and keeps what it returns', () => {
	const context = hooked();
	const calls = [];
	const module = {
		onRuntimeInitialized(...args) { calls.push(['initialized', this, args, context.heard.initialized]); return 1; },
		onQuit(...args) { calls.push(['quit', this, args, context.heard.quit]); return 2; },
		onError(...args) { calls.push(['error', this, args]); return 3; },
	};
	context.Module = module;
	// The loader may assign the same Module again; it is not wrapped twice.
	context.Module = module;
	assert.equal(context.hooked_module(), module);
	assert.equal(module.onRuntimeInitialized('ready'), 1);
	assert.equal(module.onQuit('bye'), 2);
	assert.equal(module.onError('broken', 'more'), 3);
	assert.deepEqual(calls, [['initialized', module, ['ready'], true], ['quit', module, ['bye'], true],
		['error', module, ['broken', 'more']]]);
	const heard = JSON.parse(JSON.stringify(context.heard));
	assert.deepEqual(heard, { initialized: true, quit: true, errors: ['broken'], has_quit: true });
});

test('a page\'s onQuit that throws is still heard as a quit', () => {
	const context = hooked();
	const module = { onQuit() { throw new Error('page teardown failed'); } };
	context.Module = module;
	assert.throws(() => module.onQuit(), /page teardown failed/);
	assert.equal(context.heard.quit, true);
});

test('a Module without onQuit is recorded as such, and extra runs once per object', () => {
	const context = hooked('labradorFixtureHooked', 'value.labradorExtra = (value.labradorExtra ?? 0) + 1;');
	const module = {};
	context.Module = module;
	context.Module = module;
	assert.equal(context.heard.has_quit, false);
	assert.equal(module.onQuit, undefined);
	assert.equal(module.labradorExtra, 1);
	assert.equal(module.onRuntimeInitialized(), undefined);
	assert.equal(context.heard.initialized, true);
	context.Module = null;
	assert.equal(context.hooked_module(), null);
});

test('a key table is part of WebDriver\'s, and key actions press only its keys', () => {
	const table = key_table(['Escape', 'KeyS']);
	assert.deepEqual({ ...table }, { Escape: '\uE00C', KeyS: 's' });
	assert.throws(() => key_table(['F13']), /WebDriver has no key F13/);
	assert.equal(WEBDRIVER_KEYS.ArrowDown, '\uE015');
	const [source] = key_actions(['KeyS'], { table, hold_ms: 5 });
	assert.deepEqual(source.actions, [{ type: 'keyDown', value: 's' }, { type: 'pause', duration: 5 },
		{ type: 'keyUp', value: 's' }, { type: 'pause', duration: 5 }]);
	assert.throws(() => key_actions(['Enter'], { table }), /Unsupported sample key Enter/);
});

test('a deadline answers the time left up to its cap, then refuses', () => {
	const real_now = Date.now;
	let now = 1000;
	Date.now = () => now;
	try {
		const remaining = deadline_after(500, 'Fixture run', 200);
		assert.equal(remaining(), 200);
		now = 1400;
		assert.equal(remaining(), 100);
		now = 1500;
		assert.throws(remaining, /Fixture run deadline expired/);
	} finally {
		Date.now = real_now;
	}
});

test('a child that has already exited is not waited for', async () => {
	const gone = Object.assign(new EventEmitter(), { exitCode: 0, signalCode: null });
	assert.equal(await exited(gone, 60000), true);
	const running = Object.assign(new EventEmitter(), { exitCode: null, signalCode: null });
	const waiting = exited(running, 60000);
	running.emit('exit', 0, null);
	assert.equal(await waiting, true);
	assert.equal(running.listenerCount('exit'), 0);
	assert.equal(await exited(running, 1), false);
});
