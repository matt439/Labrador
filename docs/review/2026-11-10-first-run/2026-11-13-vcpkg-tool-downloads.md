# Why the first configure downloads a CMake and a PowerShell, 2026-11-13

The first step of `REV#121`: why a starter's first configure downloads a
53 MB CMake and a 117 MB PowerShell, and whether a setting makes vcpkg use
what is already installed. Neither download is asked for by a port. vcpkg
itself fetches both, because it records their versions in every package's ABI
hash, and it accepts an installed copy only at or above the version its own
tool list names. Visual Studio's CMake is one patch release older than that,
and Visual Studio installs no PowerShell 7 at all, so the finding's premise is
half right: one of the two tools is in Visual Studio, the other is not.
`VCPKG_FORCE_SYSTEM_BINARIES=1` makes vcpkg take Visual Studio's CMake, but it
then refuses to start without a PowerShell 7 on `PATH`. Nothing in the starter
was changed; whether to change it stays the owner's call.

## What was run

- The starter: `tools/consumer_check/starter_zip.py out/zip` at `14464191`,
  a development ZIP (10,208,764 bytes), extracted with
  `C:\Windows\System32\tar.exe` into a fresh `C:\dev\r121<case>` folder for
  each case, then `cmake --preset x64-debug` in its `mygame` folder.
- A prompt set up by Visual Studio 2026 Community's `vcvars64.bat`, which sets
  `VCPKG_ROOT` to Visual Studio's own vcpkg
  (`C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg`, vcpkg
  `2026-05-27-d5b6777d666efc1a7f491babfcdab37794c1ae3e`) and puts Visual
  Studio's CMake, `cmake version 4.3.1-msvc1`, first on `PATH`.
- For each case, `VCPKG_DOWNLOADS` and `VCPKG_DEFAULT_BINARY_CACHE` named
  empty folders inside the case's own folder, so every tool and package was
  downloaded and built afresh. Git was installed on this machine, so vcpkg's
  private Git was not fetched in any case; the registry clone under
  `%LOCALAPPDATA%\vcpkg\registries` was shared.
- No `pwsh.exe` exists anywhere under the Visual Studio installation or in
  `C:\Program Files\PowerShell`, and none is on `PATH`. Windows PowerShell 5.1
  is present; vcpkg does not consider it a `powershell-core`.
- The machine was shared with other agents' builds, so the times below are
  noisy and say nothing about the cost of a setting.

## Why vcpkg fetches them

Visual Studio's vcpkg reads its tool list from
`VC\vcpkg\scripts\vcpkg-tools.json`. For Windows x64 it names `cmake` 4.3.2
(`cmake-4.3.2-windows-x86_64.zip`) and `powershell-core` 7.5.4, fetched as
`PowerShell-7.6.1-win-x64.zip`. vcpkg looks for each on `PATH` first and takes
it only when its version is at least the listed one; otherwise it downloads
the listed archive into `VCPKG_DOWNLOADS\tools`.

The configure log shows when each is wanted. `A suitable version of cmake was
not found (required v4.3.2).` comes first, before vcpkg reads the registry.
`A suitable version of powershell-core was not found (required v7.5.4).`
comes during `Detecting compiler hash for triplet x64-windows...`, before any
package is built. Neither is a port's request: every package's
`vcpkg_abi_info.txt` lists `cmake 4.3.2` and `powershell 7.6.1` among its
inputs, beside `triplet` and the helper ports, so vcpkg needs both versions to
compute any package's ABI hash. The pair is therefore fixed by which vcpkg
release Visual Studio carries, not by Labrador's `vcpkg.json` or baseline.
Whether a later Visual Studio update brings its CMake level with its vcpkg's
list was not checked.

## The cases

| Case | Setting | `PATH` holds a PowerShell 7 | Configure | Tools downloaded |
|---|---|---|---|---|
| a | none | no | passed, 169 s | CMake 52,961,068 B and PowerShell 117,085,974 B |
| b | `VCPKG_FORCE_SYSTEM_BINARIES=1` | no | failed after 9 s | none |
| c | `VCPKG_FORCE_SYSTEM_BINARIES=1` | yes | passed; build and both tests passed; 148 s for all three | none |
| d | none | yes | passed, 196 s | CMake 52,961,068 B |

In every passing case the package downloads were the same seven files,
29,287,629 bytes: the XAudio2 redistributable (24,018,213), doctest,
lodepng, DirectXMath, DirectX Tool Kit and two MSYS2 archives for `pkgconf`.

Case b stopped with:

```text
error: error: Could not fetch powershell-core. A downloadable copy of this tool is available and can be used by unsetting %VCPKG_FORCE_SYSTEM_BINARIES%. You may be able to install this tool via your system package manager.
```

Case c's PowerShell 7 was the 7.6.1 that case a had downloaded, copied to its
own folder and put first on `PATH`, standing in for an installed one. vcpkg
built every package with Visual Studio's CMake 4.3.1-msvc1, the project built,
and `ctest --preset x64-debug` passed `MyGameTests` and `MyGameSmokeTests`.
Its packages' `vcpkg_abi_info.txt` record `cmake 0` and `powershell 0`: with
the setting, vcpkg leaves tool versions out of the ABI hash, so its binary
cache entries do not match those of a configure without it.

Case d shows the two tools are independent: an installed PowerShell 7 removes
the 117 MB download without any setting, and CMake is still fetched.

## What follows

- No setting removes both downloads for a reader who has only what the
  prerequisites page installs. `VCPKG_FORCE_SYSTEM_BINARIES` saves the 53 MB
  CMake and makes PowerShell 7 a prerequisite, whose own installer is a
  download too; its size was not measured here.
- The setting applies to every tool vcpkg fetches, so it also stops the
  private Git download that the starter page describes for a reader without
  Git. That consequence follows from the same check and was not run here.
- The starter page and the Build Labrador page said vcpkg downloads a CMake
  and a PowerShell "if it finds Visual Studio's unsuitable". Visual Studio has
  no PowerShell 7 to find, so both sentences now say so; nothing else on the
  pages or in the starter changed.
- What remains for the owner is the finding's own choice: accept the two
  downloads, put the setting in the starter's presets and add PowerShell 7 to
  the prerequisites, or another mechanism such as a vcpkg whose pinned tool
  versions match Visual Studio's.
