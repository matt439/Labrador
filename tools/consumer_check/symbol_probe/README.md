# Release symbol probe

An explicitly run native MSVC x64 check for a consumer's optimised executable,
its full PDB and an exact preserved source file. It builds independently of
the engine and launches only the executable passed to it, with `--smoke-test`.
It does not install, register or distribute debugging tools.

From an x64 Native Tools Command Prompt:

```bat
cmake -S tools/consumer_check/symbol_probe -B out/symbol-probe -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/symbol-probe
out\symbol-probe\LabradorSymbolProbe.exe --exe C:\private\game\MyGame.exe --pdb C:\private\symbols\MyGame.pdb --source C:\private\source\main.cpp --timeout 60
```

The executable needs `/Zi /ZH:SHA_256` compilation and `/DEBUG:FULL` linking,
while retaining its Release optimisations and runtime. Keep the recorded PDB
basename when relocating it. The source copy can live elsewhere and must
retain its original bytes, including line endings. The executable still
needs the DLLs and content its smoke test loads.

The probe uses the already installed Windows SDK's x64 DbgHelp DLL. Its
default location comes from `WindowsSdkDir`, or the Windows Kits 10 folder
under `ProgramFiles(x86)`. Override it with `--dbghelp <absolute dbghelp.dll>`
if needed; the actual loaded path is printed. No symbol server or network
path is configured. `--symbol` defaults to `wWinMain`; another function must
have a source line reached by that executable's smoke route.

Success requires an exact local PDB with line information and the executable's
RSDS GUID/age, a matching SHA-256 source checksum, a real one-shot source
breakpoint hit, and smoke-test exit zero. Failure stops the launched child.
The receipt prints private file paths and hashes: retain it with the matching
private build evidence. This checks symbol loading and one reachable source
line; optimised locals, inlined calls and every source line are not promised.
