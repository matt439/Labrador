# Checking C++ source enumeration

`python tools/check_source_enumeration.py` reads this checkout's tracked files
and working-tree CMake lists without configuring, compiling or contacting the
network. `--repo <checkout-or-subdirectory>` selects another local checkout;
Git resolves its root before paths are listed. It runs in CI's
existing `lint` job and in `tools/lint_local.py`, including documentation-only
changes. It adds no build job or ctest entry.

Every tracked `.cpp` below `engine/`, `tests/`, `samples/` and `bench/` must have
an explicit source declaration in a tracked `CMakeLists.txt`. Paths resolve
from that CMake file's directory, including `../` and quoted absolute paths
formed with supported directory variables. Two `main.cpp` files in different
directories are different sources. A comment, a `message()`, an unused list
or an untracked file does not satisfy the requirement. A failure names the
missing path and a nearby CMake file to edit.

The supported declarations are `add_library`, `add_executable`,
`target_sources` and this repository's `labrador_add_test_suite(... SOURCES
... LINKS ...)` wrapper. Alias and imported targets do not declare compiled
sources. The scanner reads line and bracket comments and quoted arguments;
bracket source arguments, escapes and backslash paths are refused rather
than interpreted approximately.

Local literal `set` lists, `unset`, path variables, aliases and
`list(APPEND|PREPEND|REMOVE_ITEM)` are supported. Assignment captures the
values available at that point; an overwritten list cannot keep an orphan
alive. Conditions are not evaluated. Sources in every `if`/`elseif`/`else`
branch count, and source-list alternatives merge after `endif`, so a Windows
check includes browser and other backend declarations. A
`$<$<CONFIG:Debug>:...>` source (or another named configuration) counts too;
`$<TARGET_OBJECTS:target>` contains no source path to add.

Directory variables are `CMAKE_CURRENT_SOURCE_DIR`,
`CMAKE_CURRENT_LIST_DIR` and, inside a function body,
`CMAKE_CURRENT_FUNCTION_LIST_DIR`. Top-level/project directory variables are
not guessed. Functions and macros are not executed: their static declarations
are inspected, their local assignments do not leak, and caller-relative
source paths are refused. Use `CMAKE_CURRENT_FUNCTION_LIST_DIR` for a stable
path inside a helper. The test-suite wrapper's `${suite_SOURCES}` forwarding
parameter is handled explicitly; its call sites supply the checked lists.

This is a lexical check, not a general CMake interpreter. Unknown source
variables, other source generator expressions, `FILE_SET`, unsupported list
operations on consumed source lists and source paths outside the checkout
produce file/line diagnostics. Other built-in variable-processing commands
that mention an existing source-list name invalidate it instead of reusing
stale values; glob output lists are explicitly refused. Dynamic loops, globbing, custom source helpers,
included `.cmake` source declarations, external/generated sources, other C++
extensions, symlink identity and arbitrary CMake expansion are outside the
model. A custom helper's mention does not count: use a supported explicit
declaration or extend the checker and its fixtures with the precise new form.

Passing proves enumeration under those rules. A condition may select no target
on a particular host, an optional target may stay outside the default build,
and a declared source may still fail to compile. Configuration inventories,
the compiler matrix, consumer checks and runtime tests retain their separate
jobs. The check does not count doctest cases or prove that a test ran.
