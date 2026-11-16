// EM_JS_DEPS is consumed after the native link in the pinned SDK. Declare
// these exact private helpers early too, so optimized links retain the native
// stack operations used by the window's pointer callback bridge.
addToLibrary({
	$stackSave__force: true,
	$stackRestore__force: true,
	$stringToUTF8OnStack__force: true,
});
