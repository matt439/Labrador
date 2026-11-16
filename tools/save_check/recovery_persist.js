// Link with --pre-js. Restore the one IDBFS mount before the game can read
// defaults or prepare a write; the C++ example owns its schema and recovery.
(() => {
	let restored = false;
	let tail = Promise.resolve();

	function message(error) {
		return error instanceof Error ? `${error.name}: ${error.message}` : String(error);
	}

	function operation(fallback) {
		const current = Module['saveOperation'];
		return {
			status: typeof current?.status === 'string' ? current.status : fallback,
			message: typeof current?.message === 'string' ? current.message : 'The game reported no save detail.',
		};
	}

	function updateProfileUrl(outcome) {
		if (Module['settingsState']?.profile !== 'recovered') return outcome;
		try {
			const url = new URL(globalThis.location.href);
			url.searchParams.set('profile', 'recovered');
			globalThis.history.replaceState(globalThis.history.state, '', url);
			return outcome;
		} catch (error) {
			// The storage acknowledgement has already succeeded. Do not
			// misreport a navigation problem as a rejected storage flush.
			return { ...outcome, message: `${outcome.message} Saved in the recovered profile, but its URL could not be updated: ${message(error)}. Reload with ?profile=recovered.`,
				navigationUpdated: false };
		}
	}

	async function preparedAction(prepare) {
		if (!restored || Module['settingsReady'] !== true) {
			return { status: 'refused', message: 'Saved settings have not finished loading.' };
		}
		let prepared = false;
		try {
			if (prepare() !== 0) return operation('refused');
			prepared = true;
			try {
				await Module['flushSave']();
			} catch (error) {
				Module['_complete_save'](0);
				return { status: 'flush_failed', message: `Browser storage sync failed: ${message(error)}. ${operation('failed').message}` };
			}
			prepared = false;
			if (Module['_complete_save'](1) !== 0) return operation('failed');
			return updateProfileUrl(operation('saved'));
		} catch (error) {
			if (prepared) {
				try { Module['_complete_save'](0); } catch (_) { /* Preserve the original failure below. */ }
			}
			return { status: 'failed', message: `Could not complete the save action: ${message(error)}` };
		}
	}

	function enqueue(action) {
		const result = tail.then(action);
		// Every public action settles to an explicit outcome. A rejected
		// action cannot break the queue or create an unhandled rejection.
		const settled = result.catch((error) => ({ status: 'failed', message: message(error) }));
		tail = settled.then(() => undefined);
		return settled;
	}

	Module['saveVolume'] = (volume) => enqueue(() => {
		if (!Number.isInteger(volume) || volume < 0 || volume > 100) {
			return { status: 'refused', message: 'Effects volume must be an integer from 0 to 100.' };
		}
		return preparedAction(() => Module['_prepare_volume'](volume));
	});
	Module['retrySave'] = () => enqueue(() => preparedAction(() => Module['_retry_save']()));
	Module['recoverPrevious'] = () => enqueue(() => preparedAction(() => Module['_recover_settings'](1)));
	Module['recoverDefaults'] = () => enqueue(() => preparedAction(() => Module['_recover_settings'](0)));

	Module['flushSave'] = () => {
		if (!restored) return Promise.reject(new Error('Restore has not completed; storage was not written.'));
		return new Promise((resolve, reject) => {
			FS.syncfs(false, (error) => error ? reject(error) : resolve());
		});
	};

	if (typeof Module['preRun'] === 'function') Module['preRun'] = [Module['preRun']];
	Module['preRun'] ??= [];
	Module['preRun'].push(() => {
		addRunDependency('restore-recovery-settings');
		try {
			const requested = new URL(globalThis.location.href).searchParams.get('profile');
			if (requested && requested !== 'main' && requested !== 'recovered') {
				throw new Error('The profile must be main or recovered.');
			}
			Module['saveProfile'] = requested === 'recovered' ? 'recovered' : 'main';
			FS.mkdir('/save');
			FS.mount(IDBFS, {}, '/save');
		} catch (error) {
			abort(`Cannot mount recovery settings: ${message(error)}`);
			return;
		}
		FS.syncfs(true, (error) => {
			if (error) {
				abort(`Cannot restore recovery settings: ${message(error)}`);
				return;
			}
			restored = true;
			removeRunDependency('restore-recovery-settings');
		});
	});
})();
