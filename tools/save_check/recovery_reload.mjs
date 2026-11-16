// run_page.mjs --input: actual IDBFS acknowledgements/reloads and a controlled
// abort of a real IndexedDB write transaction, in the runner's fresh profile.
import assert from 'node:assert/strict';
import { wait_for } from '../../tests/render/web/page_wait.mjs';

export const ready = 'typeof Module === "object" && Module.settingsReady === true';
export const strict_complaints = true;

export default async function check({ evaluate, send, wait }) {
	const until = (expression) => wait_for(evaluate, expression,
		{ timeout_ms: 15000, navigating: true, wait, message: `Timed out waiting for ${expression}` });
	const state = () => evaluate('Module.settingsState');
	const files = (profile = 'main') => evaluate(`(() => {
		const directory = ${JSON.stringify(profile === 'main' ? '/save' : '/save/recovered')};
		return Object.fromEntries(['settings.json', 'settings-a.json', 'settings-b.json'].map((name) => {
			const path = directory + '/' + name;
			try { return [name, Module.FS.readFile(path, {encoding:'utf8'})]; }
			catch (error) { if (error.errno === 44) return [name, null]; throw error; }
		}));
	})()`);
	async function reload() {
		// A ready condition alone can accidentally read the old page before
		// Page.reload begins. This marker must disappear in the new realm.
		await evaluate('globalThis.__recoveryReloadOldRealm = true');
		await send('Page.reload', { ignoreCache: true });
		await until('typeof globalThis.__recoveryReloadOldRealm === "undefined" && typeof Module === "object" && Module.settingsReady === true');
	}
	async function abortNextTransaction() {
		await evaluate(`(() => {
			const original = IDBDatabase.prototype.transaction;
			globalThis.__recoveryAbortedTransactions = 0;
			IDBDatabase.prototype.transaction = function(...args) {
				const transaction = original.apply(this, args);
				if (args[1] === 'readwrite') {
					IDBDatabase.prototype.transaction = original;
					transaction.addEventListener('abort', () => {
						++globalThis.__recoveryAbortedTransactions;
					}, { once: true });
					queueMicrotask(() => transaction.abort());
				}
				return transaction;
			};
			return true;
		})()`);
	}
	const volumeRecords = (snapshot, volume) => Object.entries(snapshot).filter(([name, bytes]) => {
		if (!name.startsWith('settings-') || bytes === null) return false;
		const record = JSON.parse(bytes);
		return record.version === 2 && record.effects_volume === volume;
	});

	await until('typeof Module === "object" && Module.settingsReady === true');
	assert.equal((await state()).status, 'missing');
	assert.equal((await evaluate('Module.saveVolume(37)')).status, 'saved');
	assert.equal((await state()).volume, 37);
	await reload();
	assert.equal((await state()).status, 'current');
	assert.equal((await state()).volume, 37);
	const saved37 = await files();
	const prior37 = volumeRecords(saved37, 37);
	assert.equal(prior37.length, 1, 'one committed generation holds volume37');
	const [priorName, priorBytes] = prior37[0];

	await abortNextTransaction();
	const failed52 = await evaluate('Module.saveVolume(52)');
	assert.equal(failed52.status, 'flush_failed');
	assert.match(failed52.message, /sync failed/i);
	await until('globalThis.__recoveryAbortedTransactions === 1');
	assert.equal(await evaluate('globalThis.__recoveryAbortedTransactions'), 1, 'the browser aborted a real IndexedDB transaction');
	assert.equal((await files())[priorName], priorBytes, 'failed sync never changes the acknowledged generation in memory');
	assert.equal((await state()).volume, 37);
	assert.equal((await state()).retryAvailable, true);
	await reload();
	assert.equal((await state()).volume, 37, 'a real reload restores the last committed generation after this transaction abort');
	assert.equal((await files())[priorName], priorBytes);

	await abortNextTransaction();
	assert.equal((await evaluate('Module.saveVolume(52)')).status, 'flush_failed');
	await until('globalThis.__recoveryAbortedTransactions === 1');
	assert.equal((await files())[priorName], priorBytes);
	assert.equal((await evaluate('Module.retrySave()')).status, 'saved');
	assert.equal((await state()).volume, 52);
	assert.equal((await state()).retryAvailable, false);
	assert.equal((await files())[priorName], priorBytes, 'retry also preserves the old generation');
	await reload();
	assert.equal((await state()).volume, 52);
	const beforeFuture = await files();

	const future = '{"version":99,"keep":"future data must remain byte-for-byte"}\n';
	await evaluate(`Module.FS.writeFile('/save/settings.json', ${JSON.stringify(future)}, {encoding:'utf8'}); true`);
	await evaluate('Module.flushSave().then(() => true)');
	await reload();
	assert.equal((await state()).status, 'recovery_required');
	assert.equal((await state()).canSave, false);
	assert.equal((await state()).recoveryAvailable, true);
	assert.equal((await evaluate('Module.saveVolume(17)')).status, 'refused');
	assert.equal((await files())['settings.json'], future);

	assert.equal((await evaluate('Module.recoverPrevious()')).status, 'saved');
	assert.equal((await state()).profile, 'recovered');
	assert.equal((await state()).volume, 52);
	assert.equal(new URL(await evaluate('location.href')).searchParams.get('profile'), 'recovered');
	assert.equal((await files())['settings.json'], future, 'explicit recovery never changes the original future file');
	for (const name of ['settings-a.json', 'settings-b.json']) {
		assert.equal((await files())[name], beforeFuture[name], 'original generations remain intact in the protected profile');
	}
	await reload();
	assert.equal((await state()).profile, 'recovered');
	assert.equal((await state()).status, 'current');
	assert.equal((await state()).volume, 52);
	assert.equal((await files())['settings.json'], future);
	process.stdout.write('recovery save check: acknowledged reload, real transaction abort, retry, future-data refusal and fresh-profile recovery passed\n');
}
