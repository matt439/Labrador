#include <emscripten/emscripten.h>

// Keep this free of native engine references: it consumes the library's link
// dependencies without unrelated objects accidentally retaining stack support.
EM_JS_DEPS(web_stack_dependency_probe,
	"$stackSave,$stackRestore,$stringToUTF8OnStack,$UTF8ToString");

EM_JS(int, web_stack_dependency_round_trip, (), {
	const before = stackSave();
	const text = "Labrador pointer callback";
	const pointer = stringToUTF8OnStack(text);
	const copied = UTF8ToString(pointer);
	stackRestore(before);
	if (copied !== text || stackSave() !== before) {
		console.error("Window stack dependency round trip failed");
		return 0;
	}
	return 1;
});

int main()
{
	return web_stack_dependency_round_trip() == 1 ? 0 : 1;
}
