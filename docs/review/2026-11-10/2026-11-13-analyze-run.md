# One /analyze run, 2026-11-13

Run for `REV#68` at `14464191`, outside CI. Whether the repository adds
`/analyze`, a `.clang-tidy` or CodeQL stays the owner's call; this is the
evidence the finding said was missing. Nothing was fixed.

## How

MSVC 19.51.36256 (Visual Studio 18), scratch build folders under the ignored
`out/`, each configured from a native Debug preset with

```text
cmake --preset <preset> -B out/analyze-<name> "-DCMAKE_CXX_FLAGS_INIT=/analyze /analyze:WX-"
cmake --build out/analyze-<name> --parallel 8 -- -k 0
```

`/analyze:WX-` keeps analysis warnings as warnings under `/WX`; every build
finished. The presets were `x64-debug-null`, `x64-debug` (D3D11, XAudio2,
XInput, Win32), `x64-debug-d3d12`, `x64-debug-gl` and `x64-debug-vulkan`, so
every native engine backend, every test target and the samples were compiled
under analysis: 297 to 306 C++ objects a configuration. The browser build is
clang and was not analysed.

vcpkg headers (DirectXTK, DirectXMath, doctest, lodepng) are included with
`/external:W0`. A second D3D11 build with `/external:W4` added reported the
same 42 warnings and none located in a vcpkg header, so that setting hides
nothing in the D3D11 configuration. `external/rapidjson` is included as an
ordinary directory and is reported.

## Counts

Warning lines in each build log, a header warning repeating once for each
object that includes it:

| Configuration | Lines | rapidjson C6313 | C6262 | Other |
|---|---|---|---|---|
| x64-debug-null | 44 | 21 | 22 | 1 |
| x64-debug | 42 | 21 | 20 | 1 |
| x64-debug-d3d12 | 43 | 21 | 20 | 2 |
| x64-debug-gl | 42 | 21 | 20 | 1 |
| x64-debug-vulkan | 43 | 21 | 20 | 2 |

Distinct sites over all five:

- Vendored: 17 rapidjson sites, all C6313 (`reader.h` 15, `writer.h` 2).
  One C6221 is located in doctest's `doctest.h:2002` but raised by a
  first-party assertion and is classified with those below.
- First-party: 25 sites. 22 are C6262 (large stack frame), one C28020, one
  C6011 and the doctest-located C6221.
- Samples: none.

## Each first-party warning

| Site | Warning | Verdict |
|---|---|---|
| `engine/assets/json.cpp:75` (`read_json_file`) | C6262, 65,864 bytes of stack | True as measured, not a defect. The 64 KiB read buffer is deliberate; `cmake/settings.cmake` states it when it sets the browser stack to 1 MiB, the Windows default. |
| `tests/linesweeper/view_tests.cpp`, 17 test cases (lines 73 to 747) | C6262, 320,664 to 641,800 bytes | True as measured, not a defect. Each builds `ParticleField`, whose 10,000 particles are inline (`particles.h`), as a local; two fields make the 641 KiB cases. `cmake/settings.cmake` states this as the reason for the 1 MiB stack. |
| `tests/linesweeper/pacing_fixture_tests.cpp:66`, `:91` | C6262, about 321 KiB | Same as above. |
| `tests/linesweeper/particle_draw_tests.cpp:56`, `:105` (null render only) | C6262, about 322 KiB | Same as above. |
| `engine/audio/xwb_file.cpp:208` (`decode_adpcm_block`) | C28020, `adaptation` index may reach 16 | False positive. The index is a nibble, `byte >> 4` or `byte & 0x0F` of an `unsigned char`, so 0 to 15 for the 16-entry table. The neighbouring two-entry per-channel arrays are safe because the caller refuses more than two channels before decoding. |
| `tests/render/vulkan/initialization_tests.cpp:172` (test `vkCreateInstance`) | C6011, `*instance` after a null check | False positive in use. The null test at line 156 is defensive; the Vulkan specification requires a valid `pInstance`, the real loader dereferences it before returning `VK_SUCCESS`, and the engine's one call (`device_resources.cpp:556`) passes a local's address. |
| `doctest.h:2002` from `tests/render/d3d12/pending_submission_tests.cpp:242` | C6221, HRESULT compared as an integer | False positive. `CHECK(record.release_result == S_OK)` asserts exactly `S_OK` on purpose, which `SUCCEEDED` would weaken. |

No defect was found. 22 of the 25 first-party sites are one documented choice,
LineSweeper's inline particle field and the JSON read buffer, and the other
three are analyser false positives. Turning `/analyze` on under `/WX` would
need either a stack threshold (`/analyze:stacksize`) above 641,800 bytes or
suppressions, and the rapidjson sites would need the vendored copy excluded;
the zero-suppressions rule in `cmake/settings.cmake` meets both.
