// Link with --pre-js: restore the mounted directory before C++ main reads it.
if (typeof Module['preRun'] === 'function') Module['preRun'] = [Module['preRun']];
Module['preRun'] ??= [];
Module['preRun'].push(() => {
	addRunDependency('restore-save');
	try {
		FS.mkdir('/save');
		FS.mount(IDBFS, {}, '/save');
	} catch (error) {
		abort(`Cannot mount saved settings: ${error}`);
		return;
	}
	FS.syncfs(true, (error) => {
		if (error) {
			abort(`Cannot restore saved settings: ${error}`);
			return;
		}
		removeRunDependency('restore-save');
	});
});

// Call only after C++ has closed the file. Await completion before another save.
Module['flushSave'] = () => new Promise((resolve, reject) => {
	FS.syncfs(false, (error) => error ? reject(error) : resolve());
});
