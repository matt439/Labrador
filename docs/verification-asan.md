# Native-null AddressSanitizer qualification

SM-82's opt-in maintainer configuration is `x64-debug-null-asan`: MSVC Debug,
null rendering and null audio, with the ordinary native XInput backend.
It does not change consumer or ordinary native/browser build defaults.

## Run it

Use the x64 Native Tools Command Prompt and the existing `VCPKG_ROOT` setup:

```text
cmake --preset x64-debug-null-asan
cmake --build --preset x64-debug-null-asan --parallel 2
python tools/check_ctest_inventory.py --configuration x64-debug-null-asan --build-dir out/build/x64-debug-null-asan
python tools/asan_receipt.py --build-dir out/build/x64-debug-null-asan
ctest --preset x64-debug-null-asan --parallel 1 --output-on-failure --no-tests=error --output-junit asan-results.xml
```

The preset uses an isolated `x64-windows-asan` overlay triplet. It rebuilds
lodepng's static objects and the source-built DirectXTK DLL with
`/fsanitize=address`; native null still calls DirectXTK for XInput. Doctest and
vendored rapidjson are headers compiled in instrumented translation units.
XAudio2's redistributable and Windows runtime/system DLLs are prebuilt platform
edges; their internals are not instrumented here, and null audio uses no
XAudio2 voice or endpoint.

The repository's engine/tests retain `/W4 /WX /permissive- /sdl /fp:precise`,
Debug information and the DLL Debug CRT. The isolated preset removes `/RTC1`
and uses `/INCREMENTAL:NO`; the overlay removes those incompatible options
from its instrumented dependencies too. STL string, vector and optional
annotations stay enabled. No annotation-disabling macro or sanitizer-exempt
function is used. The [MSVC limitations](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-known-issues)
and [container contract](https://learn.microsoft.com/en-us/cpp/sanitizers/error-container-overflow)
explain these constraints.

`asan_receipt.py` refuses a translation unit without the instrumentation flag,
an annotation opt-out, `/RTC`, missing lodepng annotation directives, or a
missing ASan import in AudioTests or DirectXTK. It prints the translation-unit
count and actual dependency hashes. `AsanDetectionTests` requires a safe
subprocess to exit zero and two deliberately invalid subprocesses to exit
nonzero with the exact heap-buffer-overflow and container-overflow reports.
The latter accesses a vector's unused capacity through `data()`, so Debug
iterator checks cannot substitute for the sanitizer report.

## Allocation-failure and coverage boundaries

The ordinary Debug tests retain their CRT allocation hook. In ASan doctest
executables, test-only global new/delete replacements use intercepted
malloc/free and aligned allocation, with a lexical observer for counting and
one-shot C++ refusal. Audio's five failure cases retain their original
assertions and failpoint walks. Calibrations require vector and raw-malloc
counting, refusal, restoration/unwind and aligned/nothrow operation. The
per-frame no-allocation cases still require a working calibration before
trusting a zero count.

Permanent ASan malloc hooks use a fixed table of at most 64 live probe threads.
Inactive hooks touch no compiler TLS during runtime teardown. All hook helpers
are instrumented. No test is removed or excluded from the configured suite.
Raw-C allocation failure cannot be injected by the post-allocation callback;
a requested such failpoint marks a conflict rather than claiming success.
Counting raw allocations remains covered. Injected C++ refusal bypasses a
custom new_handler; the selected audio cases explicitly require that no handler
is installed.

The replacement operators reduce ASan's specific new/delete mismatch
classifications, as [Microsoft documents](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-known-issues#overriding-operator-new-and-delete).
Heap and container checks are positively demonstrated through the same
replacement path by the detection probe. Other suites and sample executables
without allocation-failure instrumentation retain ASan's own operators.
The Windows default leaves `alloc_dealloc_mismatch` off; this lane does not
claim that extra classification. It enables container-overflow detection.
This is neither fuzzing, leak detection, UBSan, TSan, a race-freedom claim nor
hardware/input/audio acceptance.

## Local qualification, 2026-11-16

Source: `64bcc505` plus this commit's sanitizer slice, before committing the
slice. Windows, Visual Studio 18 Community, cl 19.51.36256 (toolset
14.51.36231), CMake 4.3.1-msvc1 and Ninja 1.13.2 on Windows 11 Pro build
26300; build parallelism 2, CTest 1.
The manifest baseline remains `d92484ed3c5020c6679d095ad3e5add907887b62`.

- A complete native-null ASan build succeeded. The exact inventory registered
  24 tests and CTest passed all 24 in 14.91 seconds. No test filter or exclusion
  was used. The expected detection children reported heap-buffer-overflow and
  container-overflow and each exited 1; their safe control exited zero.
- A focused corroboration of the unchanged AudioResourcesFailureTests suite
  passed all five cases and 395 assertions. Both ASan allocation-calibration
  cases passed all 20 assertions. These focused runs supplement the complete
  24-entry run; they do not replace it.
- The receipt checked 328 instrumented translation units. Lodepng
  2021-12-04's Debug library has `annotate_string=1`, `annotate_vector=1`,
  `annotate_optional=1` and `InferAsanLibs`; SHA-256
  `f2745d12bff57ec555f2770e80aa109064154c9d6a2e72fc0f928b7e1234d02a`.
- AudioTests and DirectXTK 2026-05-07's Debug DLL import
  `clang_rt.asan_dynamic-x86_64.dll`. The DirectXTK DLL's SHA-256 is
  `eb0f84520eb3fbc7b32b7121b72dbd18e072219fb6f5ab8e3eb6ca134a30f916`.
  Its own build flags also contain `/fsanitize=address` and no `/RTC1`.
- The ordinary `x64-debug-null` build also succeeded and passed all 23 entries
  in 11.88 seconds, exercising the retained CRT-hook path. The website gate
  passed its type check, all 200 tests and build with internal links validated.
- All twelve local lint stages passed, including the 399-test offline Python
  suite (one environment-dependent skip). An independent source review found
  no actionable defect in the configuration, probes, inventory or workflow.

Logs and the raw receipt are retained locally under
`out/asan-qualification/`; CTest's JUnit is in the sanitizer build directory.
The earlier [November 13 rehearsal](review/2026-11-10/2026-11-13-asan-null-run.md)
remains historical: its annotation opt-out and failed allocation suite do not
qualify this configuration.

### Observed pre-main array classification

An exploratory run with the additional `alloc_dealloc_mismatch=1` option
passed MinimalTests' three cases and 20 assertions, then reported scalar-new
versus array-delete inside doctest's correctly paired `new char[]`/`delete[]`.
This source reproduces the classification without doctest or custom operators:

```cpp
#include <cstring>
#include <cstdio>
__declspec(noinline) char* make_text(unsigned int size) {
    char* text = new char[size];
    std::memset(text, 'x', size);
    return text;
}
char* global_text = make_text(69);
int main() {
    char* local_text = make_text(69);
    std::printf("%c %c\n", global_text[0], local_text[0]);
    delete[] local_text;
    delete[] global_text;
}
```

Built with this toolset's `/Od /EHsc /MDd /Zi /fsanitize=address` and
`/INCREMENTAL:NO`, the local allocation deletes cleanly; deleting the
pre-main allocation reports the mismatch with an array-thunk-to-scalar-thunk
allocation stack and exits 1. With the Windows default option it exits zero.
The reproduction's source SHA-256 is
`35cc924d2486bf24fe1087e10b344b3c538a769a9e24d73f321ba1fec3ae839a`.
This supports an observed toolchain classification limitation; it is not a
vendor-confirmed defect. No doctest source or test assertion was changed.

## Scheduled execution

After the full local suite passed, `.github/workflows/sanitizers.yml` adds a
separate non-required Windows lane, on Monday 03:47 UTC or explicit manual
dispatch. It configures the isolated preset, builds every target, checks its
exact inventory and instrumentation receipt, then runs the entire configured
suite including the detection probe. CTest results are retained as an artifact.
The ordinary pull-request matrix and its required contexts do not change.

The workflow is staged in source only. No dispatch, push, repository-setting
change or hosted sanitizer run was performed for this qualification. The first
hosted result after publication remains to be inspected; local green does not
establish runner-image qualification.
