// Coordinator behavior against the production pre-js, without a browser or
// filesystem. IndexedDB persistence/reload is covered by recovery_reload.mjs.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import vm from 'node:vm';

const source = readFileSync(new URL('./recovery_persist.js', import.meta.url), 'utf8');

function fixture({ url = 'https://example.invalid/game.html', volume = 100, callbacks, navigationError = false } = {}) {
	const calls = [];
	const restores = [];
	const flushes = [];
	let pending = null;
	let inFlight = 0;
	let maximumInFlight = 0;
	const originalPreRun = callbacks ?? (() => calls.push('original preRun'));
	const location = { href: url };
	const Module = {
		preRun: originalPreRun,
		settingsReady: false,
		settingsState: { status: 'current', profile: 'main', volume, canSave: true, retryAvailable: false },
	};
	const prepare = (next, profile = Module.settingsState.profile) => {
		calls.push(`prepare ${next}`);
		if (pending !== null) {
			Module.saveOperation = { status: 'refused', message: 'Retry the pending save first.' };
			return 1;
		}
		pending = { volume: next, profile };
		Module.settingsState.profile = profile;
		Module.saveOperation = { status: 'prepared', message: 'Candidate prepared.' };
		return 0;
	};
	Module._prepare_volume = prepare;
	Module._retry_save = () => {
		calls.push('retry');
		Module.saveOperation = { status: pending === null ? 'refused' : 'prepared', message: 'Retry candidate.' };
		return pending === null ? 1 : 0;
	};
	Module._recover_settings = (previous) => {
		calls.push(`recover ${previous}`);
		return prepare(previous ? volume : 100, 'recovered');
	};
	Module._complete_save = (success) => {
		calls.push(`complete ${success}`);
		if (!success) {
			Module.settingsState.canSave = false;
			Module.settingsState.retryAvailable = true;
			Module.saveOperation = { status: 'failed', message: 'The acknowledged generation is unchanged.' };
			return 1;
		}
		assert.notEqual(pending, null, 'the coordinator must have a prepared candidate');
		Module.settingsState.volume = pending.volume;
		Module.settingsState.profile = pending.profile;
		Module.settingsState.canSave = true;
		Module.settingsState.retryAvailable = false;
		pending = null;
		Module.saveOperation = { status: 'saved', message: 'Storage acknowledged.' };
		return 0;
	};
	const context = {
		Module, URL, Error, Promise, location,
		history: {
			state: null,
			replaceState(_state, _title, next) {
				calls.push('update URL');
				if (navigationError) throw new Error('fixture navigation refusal');
				location.href = String(next);
			},
		},
		FS: {
			mkdir(path) { calls.push(`mkdir ${path}`); },
			mount(_type, _options, path) { calls.push(`mount ${path}`); },
			syncfs(populate, done) {
				if (populate) { calls.push('restore'); restores.push(done); return; }
				calls.push('flush');
				maximumInFlight = Math.max(maximumInFlight, ++inFlight);
				flushes.push((error = null) => { --inFlight; done(error); });
			},
		},
		IDBFS: {},
		addRunDependency() { calls.push('add dependency'); },
		removeRunDependency() { calls.push('ready'); Module.settingsReady = true; },
		abort(reason) { calls.push('abort'); throw new Error(reason); },
	};
	vm.runInNewContext(source, context, { filename: 'recovery_persist.js' });
	return {
		Module, calls, location, originalPreRun,
		start() { for (const callback of Module.preRun) callback(); },
		restored(error = null) { assert.equal(restores.length, 1); restores.shift()(error); },
		async nextFlush() {
			for (let step = 0; step < 30 && flushes.length === 0; ++step) await Promise.resolve();
			assert.equal(flushes.length, 1, 'one queued action should reach the flush boundary');
			return flushes.shift();
		},
		pending() { return pending; },
		maximumInFlight() { return maximumInFlight; },
	};
}

test('restore gates writes, preserves preRun callbacks and supplies only a supported profile', async () => {
	const original = [() => {}, () => {}];
	const checked = fixture({ callbacks: original });
	assert.equal(checked.Module.preRun[0], original[0]);
	assert.equal(checked.Module.preRun[1], original[1]);
	assert.equal((await checked.Module.saveVolume(37)).status, 'refused');
	assert.deepEqual(checked.calls, []);
	checked.start();
	assert.equal(checked.Module.saveProfile, 'main');
	assert.equal((await checked.Module.saveVolume(37)).status, 'refused');
	await assert.rejects(checked.Module.flushSave(), /Restore has not completed/);
	assert.equal(checked.calls.includes('flush'), false);
	assert.equal(checked.calls.some((entry) => entry.startsWith('prepare')), false);
	checked.restored();
	assert.equal(checked.Module.settingsReady, true);
	assert.equal(checked.calls.includes('mkdir /save/recovered'), false);
	for (const query of ['', '?profile=', '?profile=main', '?profile=recovered']) {
		const profile = fixture({ url: `https://example.invalid/game.html${query}` });
		profile.start();
		assert.equal(profile.Module.saveProfile, query.endsWith('recovered') ? 'recovered' : 'main');
	}
	const invalid = fixture({ url: 'https://example.invalid/game.html?profile=other' });
	assert.throws(() => invalid.start(), /profile must be main or recovered/);
	assert.equal(invalid.calls.some((entry) => entry.startsWith('mount')), false);
});

test('failed restore keeps main and all save actions blocked', async () => {
	const checked = fixture();
	checked.start();
	assert.throws(() => checked.restored(new Error('fixture restore denied')), /Cannot restore recovery settings/);
	assert.equal(checked.Module.settingsReady, false);
	assert.equal((await checked.Module.saveVolume(37)).status, 'refused');
	assert.equal(checked.calls.includes('ready'), false);
	assert.equal(checked.calls.includes('flush'), false);
});

test('save actions serialize and acknowledge only after each asynchronous flush', async () => {
	const checked = fixture();
	checked.start(); checked.restored();
	const first = checked.Module.saveVolume(37);
	const second = checked.Module.saveVolume(52);
	const finishFirst = await checked.nextFlush();
	assert.equal(checked.Module.settingsState.volume, 100);
	assert.equal(checked.calls.includes('prepare 52'), false);
	assert.equal(checked.calls.includes('complete 1'), false);
	finishFirst();
	assert.equal((await first).status, 'saved');
	const finishSecond = await checked.nextFlush();
	assert.equal(checked.Module.settingsState.volume, 37);
	finishSecond();
	assert.equal((await second).status, 'saved');
	assert.equal(checked.Module.settingsState.volume, 52);
	assert.equal(checked.maximumInFlight(), 1);
	assert.deepEqual(checked.calls.filter((entry) => entry.startsWith('complete')), ['complete 1', 'complete 1']);
});

test('flush failure retains the pending candidate and retry does not prepare another value', async () => {
	const checked = fixture({ volume: 37 });
	checked.start(); checked.restored();
	const saving = checked.Module.saveVolume(52);
	(await checked.nextFlush())(new Error('fixture storage denied'));
	const failure = await saving;
	assert.equal(failure.status, 'flush_failed');
	assert.match(failure.message, /fixture storage denied/);
	assert.equal(checked.Module.settingsState.volume, 37);
	assert.equal(checked.Module.settingsState.retryAvailable, true);
	assert.equal(checked.pending().volume, 52);
	assert.equal((await checked.Module.saveVolume(17)).status, 'refused');
	const retry = checked.Module.retrySave();
	const finishRetry = await checked.nextFlush();
	assert.equal(checked.calls.filter((entry) => entry === 'prepare 52').length, 1);
	assert.equal(checked.Module.settingsState.volume, 37);
	finishRetry();
	assert.equal((await retry).status, 'saved');
	assert.equal(checked.Module.settingsState.volume, 52);
	assert.equal(checked.Module.settingsState.retryAvailable, false);
	assert.deepEqual(checked.calls.filter((entry) => entry.startsWith('complete')), ['complete 0', 'complete 1']);
});

test('invalid JavaScript volumes never reach integer-coercing C++ exports', async () => {
	const checked = fixture();
	checked.start(); checked.restored();
	for (const value of [NaN, Infinity, -1, 101, 1.5, '37', true, undefined]) {
		assert.equal((await checked.Module.saveVolume(value)).status, 'refused');
	}
	assert.equal(checked.calls.some((entry) => entry.startsWith('prepare')), false);
	assert.equal(checked.calls.includes('flush'), false);
	for (const value of [0, 100]) {
		const saving = checked.Module.saveVolume(value);
		(await checked.nextFlush())();
		assert.equal((await saving).status, 'saved');
		assert.equal(checked.Module.settingsState.volume, value);
	}
});

test('recovery chooses the requested C++ policy and changes URL only after acknowledgement', async () => {
	const checked = fixture({ url: 'https://example.invalid/game.html?keep=yes', volume: 37 });
	checked.start(); checked.restored();
	const recovering = checked.Module.recoverPrevious();
	const rejectFlush = await checked.nextFlush();
	assert.equal(checked.Module.settingsState.profile, 'recovered');
	assert.equal(new URL(checked.location.href).searchParams.get('profile'), null);
	rejectFlush(new Error('fixture recovery flush failed'));
	assert.equal((await recovering).status, 'flush_failed');
	assert.equal(new URL(checked.location.href).searchParams.get('profile'), null);
	const retry = checked.Module.retrySave();
	(await checked.nextFlush())();
	assert.equal((await retry).status, 'saved');
	assert.equal(new URL(checked.location.href).searchParams.get('profile'), 'recovered');
	assert.equal(new URL(checked.location.href).searchParams.get('keep'), 'yes');
	assert.ok(checked.calls.indexOf('complete 1') < checked.calls.indexOf('update URL'));
	assert.ok(checked.calls.includes('recover 1'));
	const defaults = fixture();
	defaults.start(); defaults.restored();
	const reset = defaults.Module.recoverDefaults();
	(await defaults.nextFlush())();
	assert.equal((await reset).status, 'saved');
	assert.ok(defaults.calls.includes('recover 0'));
});

test('URL failure after acknowledgement stays a saved outcome with an explicit warning', async () => {
	const checked = fixture({ navigationError: true });
	checked.start(); checked.restored();
	const recovering = checked.Module.recoverPrevious();
	(await checked.nextFlush())();
	const outcome = await recovering;
	assert.equal(outcome.status, 'saved');
	assert.equal(outcome.navigationUpdated, false);
	assert.match(outcome.message, /URL could not be updated/);
	assert.equal(checked.pending(), null);
	assert.equal(checked.calls.includes('complete 0'), false);
});
