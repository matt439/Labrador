# Native starter command helpers, 2026-11-17

OCA-97's optional `build.cmd`, `test.cmd` and `play.cmd` are assembled from
`website/src/walkthrough/helpers/`, with one shared
`tools/starter_commands.ps1`. They select the documented `x64-debug` or
`x64-release` preset, print the manual commands, and run configure/build
before testing or playing. The manual prompt route is unchanged.

The runner uses vswhere's selected installation, its x64 environment and its
bundled CMake/CTest. An explicit valid `VCPKG_ROOT` is retained; otherwise the
selected installation's vcpkg is used. Missing discovery, component files or
an invalid explicit root stop with setup advice. No tool is installed by a
helper. Windows PowerShell's script-policy option applies to that invocation,
not the machine or user's stored policy.

Native processes receive the working directory and executable path directly.
Visual Studio's UTF-8 discovery output is decoded explicitly, including in an
OEM-code-page prompt. Environment values are transferred as JSON without
printing the environment. Both output streams are drained and forwarded,
and a failed stage returns its real Windows exit status before another stage
can run. The play helper reads the project's name from CMake's cache, matching
the existing rename guide without another hard-coded game name.

## Isolated fixtures

`tools/tests/test_starter_helpers.py` runs the actual assembled launchers
against separately executed native stand-ins. It covers non-default Visual
Studio locations, project paths with spaces and shell characters, a Unicode
installation path under code page 437, missing prerequisites, malformed
discovery, explicit vcpkg refusal, x64 setup, rename-aware play, preset/action
selection and configure/build/test/play failures. Large simultaneous stdout
and stderr pin stream handling; a multiline environment value stays intact
without being dumped. The consumer CI step is itself exercised with a
negative Windows crash status followed by otherwise successful commands, so
the failure cannot be hidden by a later stage.

Two read-only review findings were reproduced and repaired before handoff:
the native PowerShell decoder had interpreted vswhere's UTF-8 using the
console code page, and CMD's `if errorlevel 1` missed negative crash statuses.
The explicit decoder and nonzero comparison now have regressions.

## Host consumer proof

The project was assembled against engine source `5cff44b9` at
`C:\dev\Helper Proof`, retaining spaces in the path, and run from an ordinary
environment without `cl` or `cmake` on PATH. Visual Studio discovery selected
`18.9.12120.119` under its usual Program Files path; vcvars reported Developer
Command Prompt `18.9.2`, and CMake identified MSVC `19.51.36256.0`.
The qualification environment limited the build to two parallel jobs.

| Actual launcher | Result |
|---|---|
| `build.cmd` | Debug configure/build exited 0. |
| `test.cmd` | Debug configure/build and both game tests passed, exit 0. |
| `test.cmd -Preset x64-release` | Release configure/build and both game tests passed, exit 0. |
| `play.cmd -SmokeTest` | Built and ran the real game's hidden startup route, exit 0. |

The native tests use the RTX 5080 host. This is command/consumer evidence,
not listening, physical-input or informal game acceptance. Native toolchain
qualification here uses the space-containing path; shell-character path
handling is exercised separately by the isolated stand-ins.

The twelve helper fixtures and starter assembly fixtures pass; the website
gate passes its 200 tests and 180-page build, with the final guide copy also
rebuilt. The full local lint attempt at this frozen base reaches its Python
suite (408 tests) and finds the already-tracked scripted-pad CI retry inventory
mismatch: six explicit native builds against a fixture expecting five. The
separate CI repair and combined-branch lint remain integration work. The
OpenSSL-dependent fixture is skipped because OpenSSL is absent.

## Retired Sandbox attempt

The original plan called for an ordinary prompt, Visual Studio at a
non-default path with spaces, all three launchers and bounded visible play.
The owner retired the fresh-install gate on November 17 in favor of
installed-tool qualification and discovery/path fixtures. The Sandbox
provisioning attempt used the
Microsoft-signed `18.12.12405.243` bootstrapper from the
[documented VS 18 channel](https://c2rsetup.officeapps.live.com/c2r/downloadVS.aspx?sku=community&channel=Stable&version=VS18)
in a disposable 16 GiB guest. The downloaded file is 5,996,752 bytes, SHA-256
`00086cd3243139a1430effa4cf8341dea8f5163eb050012e70ff66d95cee5810`,
with a Valid Authenticode signature from Microsoft Corporation.
The guest install did not yield a qualifying launch result.

## Ordinary-prompt desktop run, 2026-11-20

The development ZIP at `acafccea2f5369b93401a18f44716cedef23ffbb` was
extracted under a path containing spaces. Its SHA-256 was
`a1accee805c737c0b5a25d38afb75e0fc534036326b07d18213f152d250d9bca`.
The wrappers ran from an ordinary Command Prompt. Discovery selected
Visual Studio `18.9.12120.119`, MSVC `19.51.36256.0`, CMake `4.3.1-msvc1`
and Ninja `1.13.2`. `build.cmd` exited zero and `test.cmd` passed both
`MyGameTests` and `MyGameSmokeTests` before exiting zero.

The unmodified `play.cmd` launched the visible game on the RTX 5080. It
drew the greeting and controls. The native close button displayed the
game's `Really quit?` confirmation. The desktop tool's brief Enter
presses left the prompt open. The process later exited and the launcher
returned zero. The final input was unobserved. The local
`out/todo-20261120/starter/exit-codes.txt` receipt records zero for all three
launchers.
