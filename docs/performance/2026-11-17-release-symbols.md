# Optimised Release symbols qualification, 2026-11-17

[PI-24](../product/BACKLOG.md#pi-24) supplies an optional copied game recipe,
not an engine default. The starter assembler neither places nor enables
`website/src/walkthrough/release-symbols.cmake`. The game includes it before
its subdirectories and selects `MYGAME_RELEASE_SYMBOLS=ON` explicitly.

## Source and tools

The consumer at `C:\dev\pi24` was assembled from the worktree based on
`11380e81`, with the new recipe copied to its root and included before the
engine. Its engine/game source files remained unchanged during qualification.
These checks use MSVC 19.51.36256.0, CMake 4.3.1-msvc1, the installed Windows
SDK and x64 DbgHelp 10.0.26100.8249. SM-81's earlier CPU figures are not
generalised to this source/compiler.

## Flags and builds

Both symbol-enabled builds passed exactly `MyGameTests` and
`MyGameSmokeTests`. All 101 translation-unit commands retained `/O2`,
`/fp:precise` and `/MD`, with no `/Od` or debug runtime. They gained
`/Zi /ZH:SHA_256`; final links used `/DEBUG:FULL`, followed by
`/OPT:REF /OPT:ICF /INCREMENTAL:NO`.

| Configuration | Engine / caller IPO | Verified result |
|---|---|---|
| `release-symbols-default` | Engine OFF by fresh consumer default; caller unset | No `/GL` or `/LTCG`; both entries and the real debug probe passed. |
| `release-symbols-ipo` | Explicit `LABRADOR_NATIVE_RELEASE_IPO=ON` and `CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON` | Compilation retained `/GL`, final links `/LTCG`; both entries and the probe passed. |
| `release-symbols-off` | Same explicit full IPO; symbols OFF | Fresh configure/generate only: all 101 command records retained optimization/runtime/IPO without `/Zi` or `/ZH`. Not built. |

Compiler-free fixtures check default OFF on another platform/configuration,
preserve existing flags/runtime and both IPO choices, and reject explicit
selection for Debug or browser compilation. The recipe sets no parent IPO
policy and adds no install rule.

## Actual PDB and source loading

The standalone Windows SDK probe starts only the selected smoke executable
under `DEBUG_ONLY_THIS_PROCESS`. It loads the selected local PDB with exact
matching, compares GUID/age with the image's RSDS record, verifies the source
checksum, hits and restores a one-shot breakpoint, and requires smoke exit
zero. Calls to DbgHelp run on one thread. A deadline and own-child cleanup
bound it; it installs/registers nothing and configures no symbol server or
host debugger setting.

Both actual consumers loaded the exact provided PDB, hit `wWinMain` at
`game/main.cpp:102` and exited zero. The source SHA-256 was
`d6a997d1a10c0a898a4771edfed8d423075556368caf25015276840bfe835da2`.

| Build | EXE bytes / SHA-256 | PDB bytes / SHA-256 | PDB GUID bytes / age |
|---|---|---|---|
| Ordinary policy | 381,440 / `4cf5f072ad5052820aaf84a754e32079fbf905787a9d643519a6fddbaf9aaa3c` | 7,360,512 / `072184b049e9d555cf9a3aa7330e88febc0c700f3970c387710fd8204058c3bc` | `6a6af1d380975c4fb84c3e38cfcc805a` / 1 |
| Full IPO | 385,536 / `ce6291bb501ab1b17568415eac0bd17922e0a88ca97ec9e5eaac51f8f69865ed` | 7,024,640 / `07020ed5765ad30d732671f84853188a1ddfd78db94dea33d4c2294a201c043a` | `3f4bf0f4fd97f94299960dd2be6ac5e8` / 1 |

Relocated matching symbols and preserved source passed. Wrong-PDB and
wrong-source fixtures failed. A fixture that reaches the breakpoint but does
not quit failed at its one-second deadline; all probes left no child process.
This verifies one reachable source line, not every optimized local, inline
call or source line, nor a Visual Studio GUI stepping session.

## Installation and retention

The full-IPO install at `C:\dev\pi24-installed-ipo` passed `check_release.py`:
exactly the twenty listed files, the embedded DPI manifest and no PDB.
Its installed smoke test exited zero.

`retain_symbols.py` captured the explicit pair, receipt and 1,437 source files
under `out/private-symbols/ipo`. Archived `game/main.cpp` bytes matched the
PDB checksum. The real probe also loaded the retained PDB from that private
folder and exited zero. Capture fixtures preserve CRLF, exclude generated/VCS
directories, reject missing/misnamed symbols and unexcluded in-tree output,
and refuse replacement of an existing receipt. The final streaming capture
also hashes the selected probe receipt, rejects Windows junctions through
reparse attributes on Python 3.11+, and propagates directory-scan failures.
The real junction fixture passed; creating a symbolic-link fixture lacked
the host privilege and was explicitly skipped. Working-tree bytes avoid
Git archive's possible line-ending conversion. Hash capture remains separate
from matching verification and reproducible dependency builds.

Commands, compile records, flags audit and probe receipts remain under the
local `out/worktree-cleanup-2026-11-20/Labrador-agent-release-symbols/out/`
in `pi24/` and `pi24-*-symbol-probe.log`, and in the named private capture.
Nothing was uploaded, tagged, signed, scheduled or added to the
distributed game. Other compilers, browser symbols, third-party DLL symbols
and informal device/display acceptance remain separate work.
