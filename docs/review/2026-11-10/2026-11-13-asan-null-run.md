# One AddressSanitizer run of x64-debug-null, 2026-11-13

The first step of `REV#49`: `x64-debug-null` built once with MSVC's
AddressSanitizer in its own build folder, and its ctest entries run. The
finding's guess holds: vcpkg's static lodepng does not link against
sanitized code until the standard library's container annotations are
turned off. With them off, everything builds, and no test reported a memory
error. One entry fails, and the cause is a conflict with the test harness,
not an engine defect. No flag, preset or workflow changed. A scheduled job,
and any amendment to `SECURITY.md` and `tests/ctest_inventory.json`, stay
the owner's call.

## What was run

- The tree at `14464191`, Visual Studio 18 Community's cl 19.51.36256
  (toolset folder 14.51.36231), CMake 4.3.1-msvc1, Ninja, under `vcvars64.bat`.
- The dependencies already installed for `x64-debug`, read in place:
  triplet `x64-windows`, lodepng 2021-12-04 as a static library only
  (`lodepng.lib`, no DLL), DirectXTK and the XAudio2 redistributable as DLLs.
  The configure passed `VCPKG_INSTALLED_DIR` at that folder and
  `VCPKG_MANIFEST_INSTALL=OFF`, so nothing in vcpkg was rebuilt with the
  sanitizer.
- The configure command, in a build folder of its own:

  ```text
  cmake --preset x64-debug-null -B out/build/asan-x64-debug-null
    -DVCPKG_INSTALLED_DIR=<x64-debug>/vcpkg_installed -DVCPKG_MANIFEST_INSTALL=OFF
    "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc /fsanitize=address /D_DISABLE_STL_ANNOTATION"
    "-DCMAKE_CXX_FLAGS_DEBUG=/Ob0 /Od"
    "-DCMAKE_EXE_LINKER_FLAGS_DEBUG=/debug /INCREMENTAL:NO"
    (and the same /INCREMENTAL:NO for the shared and module linker flags)
  ```

  The first two flag values are the preset's own defaults with
  `/fsanitize=address` added and `/RTC1` removed. Debug information stayed
  `-Zi` through CMake's own setting. `labrador_settings` was untouched, so
  `/W4 /WX /permissive- /sdl /utf-8 /fp:precise` all still applied.
- `cmake --build out/build/asan-x64-debug-null --parallel 8`, then
  `ctest --test-dir out/build/asan-x64-debug-null --output-on-failure --parallel 4`.

## The two documented conflicts

- **`/RTC1`.** Microsoft documents run-time checks as incompatible with
  `/fsanitize=address`, so the debug flags drop it. On this toolset a
  one-file probe compiled with both flags at once, with no error and only a
  warning about missing debug information. The build did not rely on that.
- **Incremental linking.** A probe linked with `/debug /INCREMENTAL` gave
  `LNK4075: ignoring '/INCREMENTAL' due to '/INFERASANLIBS' specification`.
  The linker drops it and carries on. The build passed `/INCREMENTAL:NO`, so
  no link printed the warning. `/WX` applies to the compiler, not the linker,
  so the warning alone would not have failed the build.

## What vcpkg's lodepng did

With only `/fsanitize=address` added, compilation succeeded and the first
link that pulls in lodepng, `StartupInfoChild.exe`, failed:

```text
lodepng.lib(lodepng.cpp.obj) : error LNK2038: mismatch detected for 'annotate_string': value '0' doesn't match value '1' in startup_child.cpp.obj
lodepng.lib(lodepng.cpp.obj) : error LNK2038: mismatch detected for 'annotate_vector': value '0' doesn't match value '1' in startup_child.cpp.obj
lodepng.lib(lodepng.cpp.obj) : error LNK2038: mismatch detected for 'annotate_optional': value '0' doesn't match value '1' in startup_child.cpp.obj
tests\app\StartupInfoChild.exe : fatal error LNK1319: 3 mismatches detected
```

The standard library records in each object whether its `std::string`,
`std::vector` and `std::optional` carry ASan annotations, and an
uninstrumented static library says they do not. Defining
`_DISABLE_STL_ANNOTATION` for this tree's code, which the header
`__msvc_sanitizer_annotate_container.hpp` documents, makes the two agree.
All 344 build steps then completed with no warning. Every test executable
imports `clang_rt.asan_dynamic-x86_64.dll`, checked with `dumpbin /dependents`
on `AssetsTests.exe`.

That opt-out has a cost. ASan still checks heap, stack and global accesses in
this tree's code, but it no longer reports a read or write past a container's
size but inside its capacity. The other route, building lodepng with
`/fsanitize=address` through an overlay triplet so that annotations can stay
on, was not tried. lodepng's own code is uninstrumented in this run either
way, so an overrun inside the decoder itself would not be reported. The
engine's reads of the buffers it returns would be.

## What the run reported

21 of 22 entries passed: every entry `x64-debug-null` registers. The whole run
took 14 seconds of wall time at `--parallel 4`. This was one run on a loaded
machine, with no unsanitized run for comparison. **No entry printed an
AddressSanitizer report.** By default MSVC's ASan ends the process at its first
report with a non-zero exit, so a report would have failed its entry. The 21
passing entries ran to completion, and `AudioTests` reached doctest's summary.

`AudioTests` failed five cases, all in the suite `AudioResourcesFailureTests`:
`REQUIRE( sample.injected )` was false twice, and `CHECK( failures >= N )`
saw zero four times. That suite refuses one chosen allocation through
`_CrtSetAllocHook`, the Debug CRT's allocation hook. Under `/fsanitize=address`
the sanitizer's runtime serves those allocations instead, so the hook is never
called and nothing is refused. Run from its own directory with
`--test-suite-exclude=AudioResourcesFailureTests`, `AudioTests` passes all 48
remaining cases and 1229 assertions. `tests/render/allocation_scope.h` uses the
same hook, but for the D3D12 and Vulkan backends only, so this configuration
never compiled those tests.

To show that a report would have been seen, a one-file probe reading one `int`
past a `new int[4]`, built with the same flags, printed
`AddressSanitizer: heap-buffer-overflow` with its source line and exited
non-zero.

## What this does not settle

- Whether to run this as a scheduled job, and the changes it makes to
  `SECURITY.md` and the inventory, are the owner's call. A job would need to
  choose between the annotation opt-out and a sanitized vcpkg triplet. It
  would also need the hook suite excluded or made conditional on
  `__SANITIZE_ADDRESS__`.
- One run finds only what the tests already execute. The three reader bugs
  `REV#49` cites sat on inputs no test reached.
- MSVC has no UBSan. The wasm Node suites and clang's `-fsanitize=undefined`
  were not tried.
