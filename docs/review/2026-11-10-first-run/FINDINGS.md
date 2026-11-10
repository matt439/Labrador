# First-run review of 2026-11-10: findings

Every kept finding of [the review](README.md), in the order of its `TODO.md` id, then the evidence added to rows `TODO.md` already had, then what was not taken. Each describes master at `3067c67c` and the released `v0.3.0` starter ZIP, and is not rewritten as work lands; [STATUS.md](STATUS.md) is the live ledger.

Under each finding, the first paragraph is the review's statement. The entries after it are the verifiers' corrected statements and their evidence, kept as they were returned, with the lens that found each. Paths under `C:\dev\nv-...` were the agents' own copies of the starter and are gone.

## New rows

### REV#106: A 32-bit prompt builds the starter silently, and the next x64 build fails with a linker flood

<a id="rev106"></a>
*fix: no owner call · OPEN · should · S*

The `x64-debug` preset configures, builds and passes both tests in a 32-bit prompt, producing a 32-bit game. A later build from the correct prompt, after any edit, ends in about 1,400 to 1,500 linker lines, and running the configure line again does not cure it; only deleting the build folder does. The same happens the other way round when a reader who built correctly comes back in the Developer Command Prompt. Read a build error then leads to the `LNK2019` entry, which blames a source file missing from `game\CMakeLists.txt`. A guard in the starter's CMake that refuses a compiler that is not x64, and a Troubleshooting entry headed with `LNK4272`, would end it.

**`trouble-6`: A 32-bit prompt builds a 32-bit game from the 'x64-debug' preset with no error, and the starter page calls it a usual build failure** (corrected, major; starter.mdx:132-134 (step 6); troubleshooting.mdx:112-134; starter ZIP CMakeLists.txt / CMakePresets.json (v0.3.0))

Confirmed, and understated: the silent 32-bit build is not harmless, and reconfiguring does not cure it. A starter configured in a 32-bit prompt builds and passes both tests as a 32-bit game. When the reader later uses the correct x64 prompt and builds after any change, the link fails with about 1,400 lines of `error LNK2001: unresolved external symbol __purecall` and `warning LNK4272: library machine type 'x64' conflicts with target machine type 'x86'`, ending `fatal error LNK1120: 220 unresolved externals`. MyGame.exe is gone and the smoke test says `***Not Run`. Running `cmake --preset x64-debug` again exits 0 and the build still fails, because the build folder keeps the 32-bit compiler; only deleting the build folder first cures it. None of those words is on any page, and the entry heading names `x86-windows`, `Hostx86` and `warning C4309`, none of which is on screen at that point. Step 6's link calling the 32-bit prompt a usual first-build failure is wrong for the starter: the first build succeeds. The entry's own cure (rmdir, configure, build in the x64 prompt) works.

*Evidence.* RAN in C:\dev\nv-trouble-v\x86\mygame. With vcvars32.bat: `cmake --preset x64-debug` exit 0, `cmake --build --preset x64-debug` exit 0. Then with vcvars64.bat (`echo %VSCMD_ARG_TGT_ARCH%` -> `x64`), one comment line appended to game\states\hello_state.cpp: `cmake --build --preset x64-debug` -> `[4/4] Linking CXX executable game\MyGame.exe` / `FAILED: [code=4294967295] game/MyGame.exe` / `LabradorMath.lib(quad.cpp.obj) : error LNK2001: unresolved external symbol __purecall` (1,375 LNK2001 lines) / `...\um\x64\kernel32.lib : warning LNK4272: library machine type 'x64' conflicts with target machine type 'x86'` / `game\MyGame.exe : fatal error LNK1120: 220 unresolved externals`. ctest -> `MyGameTests Passed`, `MyGameSmokeTests ***Not Run`, `50% tests passed`. `cmake --preset x64-debug` without rmdir -> exit 0, `Detecting compiler hash for triplet x86-windows...`; build again -> the same LNK1120. Cure: `rmdir /s /q out\build\x64-debug`, configure, build -> exit 0, `100% tests passed`, `dumpbin /headers` -> `8664 machine (x64)`.
READ: reviewer's s3 logs (first 32-bit build passes, `14C machine (x86)`); troubleshooting.mdx:112-134; starter.mdx:132-134; grep of website/src/content/docs for `LNK4272`, `purecall` and `machine type` finds no entry.

*Suggested.* Make the mistake loud. In the starter's and engine's top CMakeLists, stop configure with a one-line message naming the x64 Native Tools Command Prompt when an `x64-*` preset finds a 32-bit compiler (CMAKE_SIZEOF_VOID_P is 4). Then head the Troubleshooting entry with that message, and reword step 6 so it no longer lists the 32-bit prompt as a build failure.

**`trouble-x1`: Coming back in the wrong (32-bit) prompt: the first edit ends in 1,500 linker lines that point at the reader's own code** (found by the verifier, major; website/src/content/docs/docs/troubleshooting.mdx:112-134 (32-bit prompt entry) and :527-568 (LNK2019 entry); docs/get-started/starter.mdx:246-256 (Coming back later); docs/get-started/build-errors.md)

The novice built correctly on day one. Next time they open 'Developer Command Prompt', the likeliest wrong window, go to the project and build. With nothing changed the build succeeds, so the prompt looks fine. After their first edit, the link step fails with 1,566 lines: `error LNK2001: unresolved external symbol _purecall`, hundreds of `LNK2019`, `warning LNK4272: library machine type 'x86' conflicts with target machine type 'x64'`, and `fatal error LNK1120: 209 unresolved externals`. MyGame.exe is deleted and the smoke test says `***Not Run`.

Following step 6 and Read a build error, they take the first `error` line and look up 'unresolved external symbol'. That leads to the LNK2019 entry, which says a .cpp file is missing from game\CMakeLists.txt or a function was never written. So they hunt in their own change for a fault that is not there. The 32-bit prompt entry's heading names `x86-windows`, `Hostx86` and `warning C4309`, none of which is on screen. The actual cure is trivial: open the x64 Native Tools Command Prompt and build again. No page says so for these words.

*Evidence.* RAN in C:\dev\nv-trouble-v\mygame, configured and built with vcvars64 (100% tests passed). Then with vcvars32.bat (`echo %VSCMD_ARG_TGT_ARCH%` -> `x86`): `cmake --build --preset x64-debug` with no change -> `[2/2] Checking that no engine file includes a game header`, exit 0. After appending one comment line to game\states\hello_state.cpp: `[4/4] Linking CXX executable game\MyGame.exe` / `FAILED: [code=4294967295] game/MyGame.exe` / `LabradorMath.lib(quad.cpp.obj) : error LNK2001: unresolved external symbol _purecall` / `LabradorEngine.lib(animation_strip.cpp.obj) : error LNK2019: unresolved external symbol "void * __cdecl operator new(unsigned __int64)" ...` / `...\lib\x86\MSVCRTD.lib : warning LNK4272: library machine type 'x86' conflicts with target machine type 'x64'` / `game\MyGame.exe : fatal error LNK1120: 209 unresolved externals` (1,539 LNK2001 or LNK2019 lines). ctest -> `MyGameTests Passed`, `MyGameSmokeTests ***Not Run`, `50% tests passed`. Back in vcvars64, same folder, no rmdir: `cmake --build --preset x64-debug` -> `[3/3] Linking CXX executable game\MyGame.exe`, exit 0, `100% tests passed`.
READ: grep of website/src/content/docs for `LNK4272`, `purecall` and `machine type` finds no entry. Live Pagefind: `library machine type conflicts with target machine type` returns only /docs/design/philosophy/; `fatal error LNK1120: 220 unresolved externals` returns 0 results. troubleshooting.mdx:535-550 gives only own-code causes for unresolved externals.

*Suggested.* Add `warning LNK4272: library machine type 'x86' conflicts with target machine type 'x64'` and 'hundreds of `LNK2001` / `LNK2019` lines, `LNK1120` with a large count' to the 32-bit prompt heading, covering both directions. Say: if the build folder was made in the right prompt, open the x64 Native Tools Command Prompt and build again; if it was made in a 32-bit prompt, delete the build folder and configure, because configuring again alone does not fix it. Put one line at the top of the LNK2019 entry and in Read a build error's linker section: hundreds of unresolved externals at once, or any LNK4272, is the wrong prompt, not your code; check `echo %VSCMD_ARG_TGT_ARCH%`. Better still, have the starter's CMakeLists stop with a one-line message when the prompt's VSCMD_ARG_TGT_ARCH is not x64.

### REV#107: Install the tools sends deep-folder readers to a subst drive, where the starter's extract fails

<a id="rev107"></a>
*fix: no owner call · OPEN · should · XS*

Windows' `tar` cannot extract at the root of a `subst` drive: every file fails with `Can't create '\\?\P:\mygame\...'`. *Keeping a deep folder* says to use `P:\` wherever the pages use `C:\dev`, which is exactly that root. It works in a subfolder of the drive. The starter page's boxes also hard-code `C:\dev` inside multi-line boxes, which *Typing the commands* says never need editing. The orchestrating session reproduced the failure twice before the review began, with a drive mapped to a folder of 110 characters.

**`tools-3`: 'Keeping a deep folder' sends the reader to a subst drive, where the next page's extract fails** (corrected, minor; prerequisites.mdx:307-332 ('Use `P:\` wherever the following pages use `C:\dev`'))

Confirmed by my own run, with a narrower cause: Windows' tar fails only when extracting at the ROOT of a subst drive, which is exactly where 'Use P:\ wherever the following pages use C:\dev' puts the reader; in a subfolder of the same subst drive it works. The failure is 1,147 'Can't create' lines and an empty folder, and starter.mdx's only tar recovery line names a different message ('Error opening archive'). Severity is minor for this reader: the section is opt-in for people whose 'projects have to live in a deep folder', the section above tells a novice to use C:\dev, and starter step 1 makes C:\dev itself, so a first-timer with no existing projects has no reason to go there. Also true: the section's first box is 'git config --global core.longpaths true' with no 'if you use Git', which a starter reader cannot run. The reviewer did not note that the starter page's boxes hard-code C:\dev inside multi-line boxes, so a subst reader must also hand-edit them (see tools-x1).

*Evidence.* Ran (tools-v\subst.bat, Windows tar bsdtar 3.8.9, v0.3.0 ZIP): 'subst W: <scratch>\sub'; 'cd /d W:\' then 'C:\Windows\System32\tar.exe -xf W:\labrador-starter-v0.3.0.zip' gave rc=1, 1147 error lines beginning "mygame/.github/workflows/ci.yml: Can't create '\\\\?\\W:\\mygame\\.github\\workflows\\ci.yml': No such file or directory", nothing extracted. Same command from 'W:\deep' gave rc=0 and the eight expected entries in W:\deep\mygame. Drive removed afterwards ('subst W: /d' rc=0). Read prerequisites.mdx:307-332, starter.mdx:65-66. Read get-started/index.mdx:78-80: the subst advice was 'checked by hand again on 13 October', before the starter ZIP existed, so with Git, never with tar.

*Suggested.* Until the extract step works on a subst drive, either remove the subst section for starter readers or tell them to extract in a real short folder (or with Explorer) first. Put the Git line behind 'if you use Git'.

**`tools-x1`: F0 is narrower than recorded, and the subst reader must also hand-edit boxes the site says never need changing** (found by the verifier, minor; prerequisites.mdx:330 ('Use `P:\` wherever the following pages use `C:\dev`'); starter.mdx steps 1-4 and website/src/components/StarterDownload.astro:29-41; troubleshooting.mdx:250-252)

A reader who took the subst advice reaches Get the starter project and finds every box hard-codes C:\dev inside multi-line boxes: 'if not exist C:\dev mkdir C:\dev / cd /d C:\dev', 'cd /d C:\dev / curl ...', 'cd /d C:\dev / tar -xf C:\dev\labrador-starter-v0.3.0.zip', 'cd /d C:\dev\mygame'. Pasted as written they silently put the project in C:\dev, not on P:, defeating the point. To follow the advice they must edit lines inside multi-line boxes, which Typing the commands says never happens ('Nothing in a box needs changing unless the text around it says so', and an editable line is always alone in its box). If they do edit them and extract at P:\, tar fails. The fix on the page can be small because tar works in a subfolder of the subst drive.

*Evidence.* Ran: on a temporary subst drive W:, Windows tar (bsdtar 3.8.9) extracting the v0.3.0 ZIP with cwd W:\ failed (rc=1, 1147 lines "Can't create '\\\\?\\W:\\mygame\\...': No such file or directory"); with cwd W:\deep it succeeded (rc=0, full mygame folder). Read prerequisites.mdx:19-20 and :37-41, starter.mdx:25-28 and :79, StarterDownload.astro:29-41. Read starter.mdx:307-311: its checks ran 'with a drive letter standing in for C:\dev', which does not say whether tar ran at a subst root.

*Suggested.* Record F0 as 'tar fails at the root of a subst drive, not in a folder on it'. In Keeping a deep folder, either tell the reader to make and use a folder on the drive (P:\dev) and say which lines of the next page to change, or say plainly that the starter route should use C:\dev and that the subst drive is for an existing project hit by C1083 (the case Troubleshooting links here for). Put the Git line behind 'if you use Git'.

### REV#108: Move a starter project to a newer Labrador carries over only the game folder

<a id="rev108"></a>
*fix: no owner call · OPEN · should · S*

A reader who put the starter in Git, or who followed the guides that place `web-release.bat` and source pictures in the project's top folder, finds none of them in the new project, and the page then invites them to delete `mygame-old`.

**`skew-8`: starter-update carries over only the game folder: .git, web-release.bat, pictures and CI edits stay in mygame-old** (confirmed, major; website/src/content/docs/docs/guides/starter-update.mdx steps 5, 6, 8 and 'Deleting the old project'; guides/your-github.mdx:113-116)

starter-update.mdx carries only the game folder into the new project. A reader who followed Put your project on GitHub (which keeps the engine as plain files and says to follow this page 'and commit the result') ends with a C:\dev\mygame that has no .git; web-release.bat and source pictures such as hero.png, which the guides place in the project's top folder, also stay in mygame-old, which the page then invites them to delete. The page never mentions .git or files outside game. It applies only to readers who did those guides, and nothing is lost until the rmdir.

*Evidence.* READ the whole of starter-update.mdx: step 5 is 'rmdir /s /q C:\dev\new\mygame\game' then 'xcopy C:\dev\mygame\game ...'; step 6 compares four files; grep for '.git' and 'hero.png' in the page is empty; the only Git mention sends submodule projects to move-pin. READ your-github.mdx:113-116 'This keeps the engine's files in your repository as plain files ... follow Move a starter project to a newer Labrador and commit the result'. READ your-picture.mdx:32 (hero.png in C:\dev\mygame) and your-web-page.mdx:90 (web-release.bat created in C:\dev\mygame). READ the other reviewer's sim.log setup lines (mkdir mygame\.git, web-release.bat, hero.png added); I did not re-run it. Not in beginner-guides.md, the 2026-11-10 findings or TODO.md.

*Suggested.* Add a step before the swap: list what is in the old project's top folder that the new one lacks (`dir /a /b` of both) and copy it across. For a project in Git, move the history explicitly (`move C:\dev\mygame-old\.git C:\dev\mygame\` after the swap, then `git status` and commit), and add .github and .gitignore to step 6's comparisons. In 'Deleting the old project', say to check for files outside game first.

### REV#109: Troubleshooting has no entry for several messages a first-timer really gets

<a id="rev109"></a>
*fix: no owner call · OPEN · should · S*

Each of these was produced in a starter made from the v0.3.0 ZIP, or read from its build, and then looked up as the reader would: the `CMakeCache.txt` error after the project folder is moved or renamed; the build and ctest messages that follow a failed or skipped configure; the three different outcomes of a deep folder by path length; a download that fails during the first configure; the `Overwrite ...?` prompt in the Extract All recovery box; the debug runtime box a friend gets from the build folder's exe; and a build that prints nothing because the edit was never saved. No page tells the reader to use the browser's own find on the 1,048-line page.

**`starter-05`: No Troubleshooting entry for the error after moving or renaming the project folder** (confirmed, minor; get-started/starter.mdx:258-262 (Starting the build again from nothing); website/src/content/docs/docs/troubleshooting.mdx has no entry; only guides/your-name.mdx:250 quotes the message)

Confirmed. After the project folder is renamed or moved, both build and configure fail with `CMake Error: The current CMakeCache.txt directory ... is different than the directory ... where CMakeCache.txt was created`. Troubleshooting has no entry for that message. Starting the build again from nothing names `the folder the project is in` as a reason to delete the build folder but does not quote the message, so a novice looking it up finds nothing. The recovery box itself is correct.

*Evidence.* RAN (<scratch>\starter-v\mv.log) after a successful build in C:\dev\nv-starter-v\mygame: `move mygame thirdgame`, then `cmake --build --preset x64-debug` gave `CMake Error: The current CMakeCache.txt directory C:/dev/nv-starter-v/thirdgame/out/build/x64-debug/CMakeCache.txt is different than the directory c:/dev/nv-starter-v/mygame/out/build/x64-debug where CMakeCache.txt was created.`, `CMake Error: The source directory "C:/dev/nv-starter-v/mygame" does not appear to contain CMakeLists.txt.`, `ninja: error: rebuilding 'build.ninja': subcommand failed`, exit 1. `cmake --preset x64-debug` gave the same first error plus `does not match the source ... used to generate cache`, exit 1. A `mygame` folder reappeared in the parent listing. READ: all of troubleshooting.mdx, which has no CMakeCache entry; starter.mdx:258-271.

*Suggested.* Add a Troubleshooting entry headed with the `The current CMakeCache.txt directory ... is different` message, pointing at Starting the build again from nothing, and quote the message's first line in that section.

**`trouble-5`: Renaming or moving the project folder: the CMakeCache error is explained only in Rename your game, not in Troubleshooting** (confirmed, minor; troubleshooting.mdx (no entry); guides/your-name.mdx:250-262 has it; starter.mdx:263-278 names the cause but not the message)

Confirmed. The CMakeCache error after renaming or moving a built project is explained only in a caution box in Rename your game; Troubleshooting has no entry. The starter page's 'Starting the build again from nothing' names the cause ('the folder the project is in') and its box is the cure, but does not quote the message. Not re-run by me: the reviewer's log shows the message and the cure working, and site search does reach the Rename guide's box, so the reader recovers.

*Evidence.* READ: reviewer's s2.bat and s2.log (ren mygame spacegame, build, `CMake Error: The current CMakeCache.txt directory ... is different than the directory ... where CMakeCache.txt was created`, then rmdir and configure exit 0). guides/your-name.mdx:250-262 quotes both variants and the empty old folder. starter.mdx:258-271. grep of troubleshooting.mdx for `CMakeCache` finds nothing. docs/beginner-guides.md records the messages for the Rename guide only.

*Suggested.* Add a Configuring entry headed with the `CMakeCache.txt directory ... is different than the directory` line. It should say do not edit CMakeCache.txt, give the rmdir and configure box, and link the Rename guide's line for the empty old folder.

**`trouble-1`: Carrying on after a failed or skipped configure gives three messages no page mentions, and none says FAILED** (corrected, minor; website/src/content/docs/docs/troubleshooting.mdx (Configuring and Building sections, no entry); docs/get-started/starter.mdx steps 6-7 (lines 119-136))

Confirmed as fact, but minor rather than major, and there are four unlisted messages, not three. After a failed or skipped configure, the next boxes print messages no page mentions, and ctest reports nothing wrong (exit 0). Which build message appears depends on how configure failed: `Error: ... is not a directory` (never configured), `no such file or directory` / `CMake Error: Generator: build tool execution failed, command was:` (VCPKG_ROOT unset, or a folder of 158+ characters), or `ninja: error: loading 'build.ninja': The system cannot find the file specified.` (a folder of about 150 characters). Step 5 does give the success lines and says what to do when it ends `Configuring incomplete, errors occurred!`, so only a reader who carries on past a failure they were told how to recognise reaches these. The dead end is real: steps 7 and 8 send them back to 'run step 6 again and read its last lines', and those last lines are the unlisted ones, with nothing pointing back to step 5.

*Evidence.* RAN in C:\dev\nv-trouble-v\mygame (v0.3.0 ZIP, Windows tar, vcvars64).
- No configure: `cmake --build --preset x64-debug` -> `Error: C:/dev/nv-trouble-v/mygame/out/build/x64-debug is not a directory` (exit 1); `ctest --preset x64-debug` -> `Test project C:/dev/nv-trouble-v/mygame` / `No tests were found!!!` (exit 0).
- After a configure with VCPKG_ROOT unset (exit 1): build -> `no such file or directory` / `CMake Error: Generator: build tool execution failed, command was:` (exit 1); ctest -> `No tests were found!!!` (exit 0).
- After the 150-character configure failure: build -> `ninja: error: loading 'build.ninja': The system cannot find the file specified.`
READ: starter.mdx:104-116 (success lines and the `Configuring incomplete` instruction), :166-171 and :187-190 (both say run step 6 again). grep of website/src/content/docs for `No tests were found`, `build tool execution failed`, `loading 'build.ninja'` and `is not a directory` finds no troubleshooting entry. Live Pagefind query `No tests were found` returns move-pin first.

*Suggested.* Add one Configuring entry headed with the three literal messages (`build tool execution failed`, `is not a directory`, `No tests were found!!!`). It should say the configure in step 5 did not finish, and to run `cmake --preset x64-debug` again and read its first Error. In starter step 6, add that a build printing no numbered lines at all means step 5 failed.

**`trouble-2`: A deep folder fails the starter at configure with a vcpkg message that has no heading and looks like the VCPKG_ROOT entry** (corrected, minor; troubleshooting.mdx:244-256 and 296-304; starter.mdx:132-134 (step 6 calls C1083 a usual first-build failure))

Partly wrong and overstated; minor. The reviewer's depths skipped a band. The starter has three outcomes by project-path length:
- 145 characters: CMAKE_OBJECT_PATH_MAX warnings, then it builds and passes. The entry's 'the build that follows will stop with C1083' is false here.
- 150 characters: configure itself fails with the warning and then `fatal error C1083: Cannot open compiler generated file: '': Invalid argument`, inside `The C++ compiler ... is not able to compile a simple test program`. So C1083 does appear for the starter, at step 5 rather than step 6, and both existing entries match it by their words.
- 158 characters and deeper: the vcpkg `unable to detect the active compiler's information` failure, ending in the same Ninja and CMAKE_CXX_COMPILER lines as the VCPKG_ROOT failure. Its only mention is a trailing sentence at troubleshooting.mdx:254-256. A browser find for `unable to detect` reaches it; a reader looking up `Ninja` reaches the VCPKG_ROOT entry instead.

A novice who follows the page is in C:\dev\mygame (13 characters). Extract All into Downloads adds about 45 and still works, and the page has a box to move it. Prerequisites states the 100-character rule. Reaching 150 or more takes a deliberate deep folder, so this is a stumble for a reader who ignored two instructions, not a likely loss. Two points stand: starter step 6 calls C1083 a usual first-build failure when for the starter it is a configure failure, and 'Keeping a deep folder' opens with a `git config` line a starter reader has no Git for.

*Evidence.* RAN (my own copies under C:\dev\nv-trouble-v, `cmake --preset x64-debug`).
- Project path 150 chars: exit 1. Log has `CMake Warning ... has 241 characters. The maximum full path to an object file is 250 characters (see CMAKE_OBJECT_PATH_MAX)` twice, then `CMake Error at .../CMakeTestCXXCompiler.cmake:73 (message): The C++ compiler ... is not able to compile a simple test program.` containing `testCXXCompiler.cxx : fatal error C1083: Cannot open compiler generated file: '': Invalid argument`, then `-- Configuring incomplete, errors occurred!`.
- Project path 158 chars: exit 1 with `error: while detecting compiler information:` ... `Error code: no such file or directory` ... `error: vcpkg was unable to detect the active compiler's information.` ... `CMake Error: CMake was unable to find a build program corresponding to "Ninja".` / `CMake Error: CMAKE_CXX_COMPILER not set, after EnableLanguage`.
READ: the reviewer's e4 logs (145 chars: warnings `has 236 characters`, `[97/97] Linking CXX executable game\MyGame.exe`, `100% tests passed`); prerequisites.mdx:294-301 (100-character rule, warning then C1083) and :307-315 (`git config --global core.longpaths true` first); troubleshooting.mdx:219-220, 244-256, 296-304; starter.mdx:68-74, 132-134.

*Suggested.* Give the vcpkg failure its own Configuring heading quoting `vcpkg was unable to detect the active compiler's information` and `Error code: no such file or directory`. In the VCPKG_ROOT entry, say the Ninja and compiler lines also end a deep-folder failure, and that the first Error line tells them apart. Change 'will stop with C1083' to 'may'. In starter step 5, list the deep folder as a usual configure failure. Give a copyable cure for the starter: `move` the project to C:\dev\mygame, `rmdir /s /q out`, configure.

**`critic-4`: A failed download during the first configure has no description and no Troubleshooting entry** (completeness pass, not separately verified, minor; get-started/starter.mdx step 5 (:93-116); docs/troubleshooting.mdx 'Configuring' (:192-284))

The first configure fetches from github.com, nuget.org and mirror.msys2.org for about a minute and a half. On hotel, school or mobile internet, or behind a filter, one of them fails. The reader is told to 'find the first line that says Error and look it up', and the two 'usual ones' linked are the wrong prompt and VCPKG_ROOT. What they actually get is a twelve-line block about HTTPS_PROXY, v2ray and shadowsocksr, which reads like they misconfigured something. No page says to check the connection and run the same line again, or that what was already downloaded is kept.

*Evidence.* RAN: `vcpkg.exe x-download <file> --url=https://nonexistent-host.invalid/x.zip --sha512=000...` with Visual Studio 2026's vcpkg printed: 'error: curl operation failed with error code 6 (Could not resolve hostname).', 'error: Not a transient network error, won't retry download from ...', 'note: If you are using a proxy, please ensure your proxy settings are correct.', then 'Possible causes are: 1. You are actually using an HTTP proxy, but setting HTTPS_PROXY variable to `https://address:port`...' through to '...please submit an issue to https://github.com/Microsoft/vcpkg/issues'; exit 1. READ: run2.log lines 21-148 show the hosts a real v0.3.0 configure downloads from. READ: a search of troubleshooting.mdx and get-started/*.mdx for 'failed to download', 'could not resolve', 'proxy', 'offline' and 'connection' finds only the Visual Studio installer's own retry line (prerequisites.mdx:96) and a browser-page message. NOT RUN: a whole configure with the network cut, so the CMake lines that wrap this block are not quoted.

*Suggested.* Add a Configuring entry headed `error: curl operation failed with error code`: it is the connection, not your setup; ignore the proxy text unless you know you use one; run `cmake --preset x64-debug` again and it carries on with what it has. Mention it in starter step 5 next to the download size. On a school or work network, say which hosts must be reachable.

**`starter-01`: Extract All recovery box stops at an unmentioned Overwrite prompt when C:\dev\mygame already exists** (confirmed, minor; website/src/content/docs/docs/troubleshooting.mdx:57-67 (linked from get-started/starter.mdx:73); live /docs/troubleshooting/#the-project-is-in-a-folder-named-after-the-zip)

Confirmed as stated. When C:\dev\mygame already exists, the Extract All recovery box in Troubleshooting stops at `Overwrite C:\dev\mygame? (Yes/No/All):`, which the page never mentions. The following pasted lines are taken as answers. No prints `1 dir(s) moved.` although nothing moved; Yes prints `Access is denied.`. Nothing is lost either way. The page's promised `A duplicate file name exists...` message did not appear in any case. It needs an existing mygame plus an Extract All into the suggested folder, so it is uncommon. Not settled: both reviewers fed lines on stdin, so a real console paste was not exercised; pasting the box into a real prompt would settle whether the later lines are swallowed the same way.

*Evidence.* RAN (<scratch>\starter-v\feed.log), lines fed to cmd.exe on stdin with C:\dev\nv-starter-v\d2 standing for C:\dev. Three-line box as written: `Overwrite C:\dev\nv-starter-v\d2\mygame? (Yes/No/All): for /d %d in (...) do rmdir "%d"`, then the same prompt swallowing `dir /b ...`, then at end of input `1 dir(s) moved.`. Answer No: `1 dir(s) moved.`, then `The directory is not empty.`, and mygame still held only the old file. Answer Yes: `Access is denied.`, source folder still there. READ: troubleshooting.mdx:63-67 promises only `Access is denied.` or `A duplicate file name exists...`. No match for `Overwrite` in docs/beginner-guides.md or docs/review/2026-11-10.

*Suggested.* Guard the move as the second-project box does: `if exist C:\dev\mygame (echo C:\dev\mygame already exists.) else (move ...)`, or add `/-Y` handling and describe the prompt. Correct the sentence that promises `Access is denied.` and say what to do next (rename via A second project).

**`critic-7`: Zipping the folder you have been running from and sending it gives the friend a missing-DLL box no page names** (completeness pass, not separately verified, minor; get-started/starter.mdx step 8 (:173-190); docs/troubleshooting.mdx 'Sharing the game' (:1004-1044))

The first thing a pleased beginner does is send what they have: out\build\x64-debug\game\MyGame.exe, or that folder zipped. A debug build needs Visual Studio's debug runtime, which a friend's PC does not have, so the friend should get a box naming VCRUNTIME140D.dll, MSVCP140D.dll or ucrtbased.dll. Step 8 does not say 'this copy only runs on your PC', and Troubleshooting's sharing section lists only DirectXTK.dll. The reader has to already know that Send your game to a friend exists.

*Evidence.* READ: a search of the docs for '140d', 'ucrtbased' and 'MSVCP140D' finds only a debugger call-stack line (debug-your-game.mdx:416). READ: troubleshooting.mdx:1033-1044 lists 'Windows protected your PC', the DirectXTK.dll message and a GitHub Pages 404. READ: your-release.mdx:12-13 says the release build 'is the one Microsoft lets you give away', and :438-441 that the debug runtime 'may not be given away'; neither is said at starter step 8. NOT RUN: I did not start a debug build on a machine without Visual Studio; the DLL names are the standard debug runtime's.

*Suggested.* Add one sentence to starter step 8: this build runs only on a PC with Visual Studio; to give the game to someone, follow Send your game to a friend. Add a Troubleshooting entry under Sharing headed `VCRUNTIME140D.dll was not found` (also MSVCP140D.dll, ucrtbased.dll) that says the same.

**`change-2`: Neither page says what a build looks like when the edit never reached the file** (confirmed, minor; first-change.mdx:116-188 (step 5 and its failure paragraphs); build-errors.mdx:51-55)

When the edit never reached the file, the step 5 line prints only two progress lines, no Building or Linking line, both tests pass and the game opens unchanged. Neither first-change, build-errors nor Troubleshooting describes this case; build-errors.mdx:53-55 says a build with no error 'ends with a Linking line', which is false for a no-change build. The page does tell the reader to save ('Ctrl+S saves', 'Save and close Notepad' in every step), so a reader who follows it does not get here; the gap is the diagnosis for one who slips. Whether Windows 11 Notepad closes an edited file without a save prompt (its session-restore default) was not checked by either of us, because Notepad must not be launched; closing an edited file in Notepad on a stock Windows 11 would settle it, and if it closes silently this deserves major.

*Evidence.* Ran in C:\dev\nv-change-v\mygame (v0.3.0 ZIP, built, nothing changed): 'cmake --build --preset x64-debug && ctest --preset x64-debug' printed '[1/2] Running utility command for game_content', '[2/2] Checking that no engine file includes a game header', then '100% tests passed, 0 tests failed out of 2'. Read: build-errors.mdx:53-55; first-change.mdx:186-188 (covers only a failed build with separate commands). Grep over website/src/content/docs for 'not saved', 'no work to do', '[2/2]', 'did nothing': no native-build entry; Troubleshooting's headings (read all) have 'The page shows the old game after a rebuild' for the browser only.

*Suggested.* Add to step 5: 'If the build prints only two lines, [1/2] and [2/2], and no Building line, it found nothing changed: your edit was not saved, or was made in another file. Open the file again, check the edit is there, press Ctrl+S, and run the line again.' Amend build-errors' sentence to 'ends with a Linking line, or with [2/2] if nothing had changed'.

**`trouble-3`: Finding your entry on a 1048-line page: search lands on the page, not the entry, and nothing says to use Ctrl+F** (corrected, minor; https://labradorengine.com/docs/troubleshooting/ (intro, troubleshooting.mdx:9-17); guides/ask-for-help.mdx:25-27)

Confirmed in substance, but minor rather than major. Site search puts Troubleshooting first for a pasted message, yet the sub-links under it are not the matching entry, and neither Troubleshooting nor Ask for help mentions the browser's own find. What limits the cost: every entry heading is the literal message, so the right-hand list and a scroll through the nine-entry Configuring section do find a message that has an entry, and starter steps 3, 5, 6 and 8 link the commonest entries directly. The real loss is for messages that are only in body text or absent (the vcpkg compiler-detection line, the ctest and build messages of trouble-1 and trouble-4, the linker flood in trouble-x1). The sub-result ranking was computed by hit count, not seen in the rendered search box; watching the live search modal once would settle exactly which three sub-links it shows.

*Evidence.* RAN: the reviewer's script against the live Pagefind index, re-run by me. `Could not find toolchain file` -> /docs/troubleshooting/ first, sub-results `LNK2019: unresolved external symbol`, `Could not read presets from C:/dev ...`, `The game could not be loaded. when opened from file://`; the toolchain entry is not among them. `No tests were found` -> /docs/guides/move-pin/ first.
READ: troubleshooting.mdx:9-17 (intro says 'by the message they see', gives no find instruction); grep for `Ctrl+F` finds only `Ctrl+F5` at line 751; ask-for-help.mdx step 1 ('the search box is at the top of every page, and Troubleshooting is ordered by message'); headings list (about 100 entries in 9 sections).

*Suggested.* Open the page with two sentences: press Ctrl+F and type three or four distinctive words from the first line that says error, not the whole line, and leave out your own folder names. Add a short 'which step were you on' jump list (extract, configure, build, test, play) to the five sections. Say the same in Ask for help step 1. Consider splitting own-code errors, Content and Browser builds into their own pages so search sub-results are not dominated by the long LNK2019 section.

### REV#110: Install the tools: the installer's own screens are unwritten and nothing says where a starter reader is finished

<a id="rev110"></a>
*fix: no owner call · OPEN · should · S*

Step 1 is the only step with no account of what the reader will see between the download link and the Workloads tab, and whether Visual Studio starts itself with a sign-in window when the install ends is not established. After step 4 about 1,250 words remain that a starter reader does not need, mostly unmarked. The installer has still not been walked by hand; see the review's limits for the Sandbox run that was started.

**`tools-1`: Nothing between 'download the installer' and the Workloads tab: file name, administrator prompt and licence screen are missing** (corrected, minor; website/src/content/docs/docs/get-started/prerequisites.mdx:51-52 (live /docs/get-started/prerequisites/ step 1))

Step 1 gives no click-by-click between the download link and the Workloads tab: which of the three cards to use (the page does say 'The free Community edition is enough', and the card is labelled 'Visual Studio Community - Free download'), the file name VisualStudioSetup.exe, the Windows 'Yes' prompt (administrator rights needed) and the licence 'Continue'. That falls short of the owner's exhaustive-guide standard and is the only step with no 'what you will see'. It is a stumble, not a likely loss: download, run, Yes, Continue is the sequence of any Windows installer, and the installer opens on the Workloads tab. The 2026 'how to configure' screen appears only for someone who already has a Visual Studio, who is not this reader. The real risk is a novice on a non-administrator account, which the page never mentions. Nobody has run the installer, so the sequence is still from Microsoft's page only.

*Evidence.* Read prerequisites.mdx:48-52. Fetched visualstudio.microsoft.com/downloads: 'Download Visual Studio 2026', cards 'Visual Studio Community / Free download', 'Visual Studio Professional / Buy now / Free trial', 'Visual Studio Insiders / Get Insiders'. Fetched learn.microsoft.com/en-us/visualstudio/install/install-visual-studio?view=visualstudio: 'double-click the bootstrapper named VisualStudioSetup.exe, or named something like vs_Community.exe'; 'If you see a User Account Control notice, select Yes ... Select Continue'; 'If you have other versions of Visual Studio installed, the installer gives you flexible options' (only then). System requirements: 'Administrator rights are required to install or update Visual Studio.' Prior record docs/beginner-guides.md:544-547 says the installer was never run; the pre-Workloads sequence is not recorded.

*Suggested.* Write step 1 as numbered clicks: under Visual Studio Community click Free download; open VisualStudioSetup.exe from Downloads; click Yes on the Windows prompt (you need an administrator account); click Continue; wait while the installer sets itself up; if asked how to configure, choose 'Manually choose workloads and components'; then the Workloads tab. Verify the sequence once in Windows Sandbox.

**`tools-4`: After step 4 a starter reader does not know they are done: about 1,250 skippable words remain, mostly unmarked** (confirmed, minor; prerequisites.mdx:204-354 (step 5, If VCPKG_ROOT is not set, Choose a short folder, Keeping a deep folder, Vulkan aside, What was checked))

After step 4 nothing tells a starter reader they are finished. Steps 2 and 5 say 'Skip this step if you are going to use the starter project' (live page shows that wording), and 'If VCPKG_ROOT is not set' opens with a skip line, but 'Choose a short folder', 'Keeping a deep folder', the Vulkan aside and 'What was checked' carry no marker, and the only way on is the 'Next: Get the starter project' pagination link at the very bottom. Nothing down there is needed: starter step 1 makes C:\dev. I did not recount the words; the section list and order match the source.

*Evidence.* Fetched live /docs/get-started/prerequisites/: steps 2 and 5 read 'Skip this step if you are going to use the starter project.'; headings in order 'Typing the commands, If VCPKG_ROOT is not set, Choose a short folder, Keeping a deep folder, What was checked'; next link 'Get the starter project'. Read prerequisites.mdx:204-354 and starter.mdx:26 ('if not exist C:\dev mkdir C:\dev').

*Suggested.* End the Steps with a line such as 'Starter project: you are done here. Next: Get the starter project', followed by a short 'why a short folder' note. Move VCPKG_ROOT repair, deep folders, Vulkan and What was checked below a heading that says they are only for when something went wrong, or onto Troubleshooting.

**`tools-6`: Visual Studio may open itself with a sign-in window when the install ends; the page does not mention it** (unverifiable, minor; prerequisites.mdx:76-79)

Whether the 2026 installer starts Visual Studio by itself when the install ends (and so shows a first-launch sign-in window at this step) is not established. Microsoft's install page describes only a manual Launch button and says Community does not require sign-in; no fetched source documents a 'Start after installation' box or its default. If it does auto-start, the page's 'you do not need to click Launch' leaves the novice facing a sign-in window that only the later Edit and debug page explains. One fresh install in Windows Sandbox, watching the progress screen and what opens at the end, settles it.

*Evidence.* Fetched learn.microsoft.com install page: Step 8 'Visual Studio Community doesn't require you to sign in'; Step 9 'Select the Launch button'; no mention of automatic start. Web search for the checkbox found no documentation. Read debug-your-game.mdx:52-56 (sign-in and theme covered there) and docs/beginner-guides.md:556-560 (those windows were never seen, written from Microsoft's documentation).

*Suggested.* Check in a Sandbox install. If Visual Studio opens itself, add: 'If Visual Studio opens and asks you to sign in, you can close it; signing in is not needed.'

### REV#111: No check of the machine, and no total cost, before the reader starts a multi-gigabyte install

<a id="rev111"></a>
*fix: no owner call · OPEN · could · S*

No page states a Windows version; Microsoft lists Visual Studio 2026 as supported on Windows 11 and not on Windows 10. Arm laptops, Windows 11 in S mode and machines without administrator rights are not mentioned. The total disk, download and time first appear inside step 1 of the second page, and the home page never names Visual Studio.

**`peers-9`: No Windows version is stated, and Microsoft's requirements for the Visual Studio the download link serves list only Windows 11 among desktop Windows** (confirmed, minor; prerequisites.mdx:9, 48-51; starter.mdx:46)

No page states a Windows version for the build machine. Microsoft's requirements for Visual Studio 2026, which the linked downloads page serves, list only Windows 11 and Windows Server among supported systems (Windows 10 only where still under Extended Security Updates). The page does say 'Visual Studio 2022 or newer', which is the workable answer for Windows 10, but gives no link to 2022 and does not connect it to Windows 10. What the 2026 installer actually does on Windows 10 is unverifiable here; running the bootstrapper in a Windows 10 VM would settle it.

*Evidence.* RAN curl on learn.microsoft.com/.../2026/vs-system-requirements: 'Visual Studio is supported on the following 64-bit operating systems: Windows 11 ,  supported versions ...; Windows Server: Standard and Datacenter editions of 2025, 2022, 2019', and 'If an operating system is in the Extended Security Updates (ESU) phase then it is supported only with ESU patches installed'. READ prerequisites.mdx:9, :48-51, :58 ('a Windows 11 SDK'); starter.mdx:46. RAN grep for 'Windows 10' across get-started and troubleshooting: only starter.mdx:46 (tar).

*Suggested.* State the floor on Install the tools: which Windows versions the path was checked on (Windows 11) and, for Windows 10, that Visual Studio 2022 is the one to install, with its link. Or state that Windows 10 is unchecked.

**`tools-2`: Windows 10 readers are sent to Visual Studio 2026, which Microsoft supports only on Windows 11** (corrected, minor; prerequisites.mdx:9 and :48-51)

No Get Started page states which Windows version is needed to build, and the download link offers only Visual Studio 2026, whose supported list has Windows 11 and Windows Server but no Windows 10. Two parts of the finding are wrong. (1) Visual Studio 2022 is not a supported fallback either: its own requirements page also lists only Windows 11 and Server, since Windows 10 itself left support in October 2025. (2) Community 2022 is not unobtainable: Microsoft's 2022 release-history page has a public Community bootstrapper link, though a novice would never find it. What a Windows 10 reader actually experiences (a clean install, a warning or a refusal) is unknown to both reviewers; 'unsupported' is not 'blocked'. Settle it by running VisualStudioSetup.exe on a Windows 10 22H2 x64 machine or VM. Until then the defect is a missing sentence, not a demonstrated block.

*Evidence.* Fetched learn.microsoft.com/.../releases/2026/vs-system-requirements: supported list is 'Windows 11 ...' and 'Windows Server: Standard and Datacenter editions of 2025, 2022, 2019'; unsupported list names only 'Windows 10 ARM64'. Fetched .../releases/2022/system-requirements: 'Windows 11 supported OS versions ... Windows Server 2025 ... 2022 ... 2019 ... 2016', no Windows 10. Fetched .../releases/2022/release-history: Evergreen row 'Current | 17.14 | ... Community (https://aka.ms/vs/17/release/vs_community.exe)'. Fetched /vs/older-downloads/: 2022 section lists Professional and Enterprise, 'Older versions require an active Visual Studio Subscription'. Web search found no source showing how the 2026 installer behaves on Windows 10. Grep of website docs: Windows 10 is named only for tar (starter.mdx:46) and for running a released game (1607 floor); prerequisites.mdx:9 says only 'Labrador builds on Windows'.

*Suggested.* State the Windows version needed to build at the top of the page (Windows 11 for Visual Studio 2026). Say what a Windows 10 reader should do, or that building on Windows 10 is not covered. Change 'Visual Studio 2022 or newer' to say that the download page gives 2026 and that an existing 2022 install also works.

**`critic-6`: No 'will this work on my PC' check before the install: Arm laptops, S mode and no administrator rights are never mentioned** (completeness pass, not separately verified, minor; get-started/prerequisites.mdx:9-15 and step 1; get-started/index.mdx)

Three kinds of machine a beginner commonly has cannot follow the route as written, and none is named before the reader starts downloading several gigabytes: a Snapdragon (Arm) Windows laptop, a cheap laptop in Windows 11 S mode (which runs only Store apps), and a school or work PC where they cannot approve an administrator prompt. The page says only 'Labrador builds on Windows, with the Microsoft C++ compiler'. An Arm reader also does not know whether an 'x64 Native Tools Command Prompt' exists on their machine or whether the x64 game will run.

*Evidence.* READ: prerequisites.mdx:9-15 has no processor, edition or account requirement; a search of the docs for 'administrator', 'admin rights', 'S mode' and 'ARM64' finds nothing on any Get Started page. READ: the site knows Arm exists elsewhere: content-tools.mdx:126-127 ('not the files ending _arm64.exe, which are for Windows on ARM computers') and windows-release.mdx:277 ('Windows 10 and Windows on Arm have not' been tested). READ: the v0.3.0 starter's CMakePresets.json fixes `"architecture": { "value": "x64", "strategy": "external" }` and the pages require `echo %VSCMD_ARG_TGT_ARCH%` to print x64. NOT RUN: no Arm, S-mode or standard-user machine was available; what each actually does is unknown.

*Suggested.* Put a three-line 'Before you install' check at the top of Install the tools: Settings > System > About shows 'System type' (x64-based is needed; say plainly that Arm is untested) and the Windows edition (S mode must be switched out of, which is one-way); and you need to be able to approve an administrator prompt. Give one line for a school or work PC: ask whoever manages it.

**`door-6`: The real cost (disk, time, which Windows, GPU or not) is not stated until step 1 of the third page** (corrected, minor; https://labradorengine.com/ facts list; website/src/content/docs/docs/get-started/index.mdx:10-33; prerequisites.mdx:62-74)

Partly overstated. Windows-only IS stated before any investment: the home page says 'Not cross-platform yet. Windows and a desktop browser only.', the Get Started index opens 'from a Windows machine with nothing installed' and says 'Labrador builds on Windows', and Install the tools opens 'Labrador builds on **Windows**'. The disk figure is also given before the costly click ('Before you click Install, look at the bottom of the installer's window ... 20 to 50 GB free ... about 16 GB on drive C:'), though after the installer has been downloaded and opened. What is really missing at the front door and on the Get Started index is a short up-front summary: total disk and download, total time (the Visual Studio install time is stated as unmeasured), which Windows versions (only inferable from 'tar comes with Windows 10 and later' on the starter page), whether Windows on ARM works, and whether a graphics card is needed. The home fact 'Windows, and a desktop browser through WebGL2' can be misread as 'runs in my browser'. Whether an ARM PC or a machine with no GPU can complete the path is unverifiable here: it needs a run on such a machine (the troubleshooting entry 'No Direct3D hardware device found in a release build' implies debug builds fall back to software, but that was not run).

*Evidence.* READ index.mdx:153; get-started/index.mdx:10-22; prerequisites.mdx:9, 62-74. RAN: grep -i for 'windows 10|64-bit|ARM|graphics|GPU' in prerequisites.mdx, get-started/index.mdx and index.mdx: only Windows 11 Terminal/SDK mentions, '64-bit installer' for Git and '64-bit compiler'; nothing on ARM or a graphics card. READ run2.log: '=== 13:09:09.01 curl' to '=== 13:10:50.58 done', build exit 0, '100% tests passed, 0 tests failed out of 2', so the reviewer's 100-second figure is right.

*Suggested.* Add a four-line 'What you need' box to the Get Started index above the cards: a Windows 10 or 11 PC (64-bit; not a Mac, Chromebook or phone); about 20 GB free on C:; a connection that can download several gigabytes; one to two hours, most of it waiting for Visual Studio to install, then about ten minutes for the project. Say whether a graphics card is needed. Make the home fact read 'Build on Windows; games run on Windows and in a desktop browser'.

**`peers-1`: The whole cost of the path is never stated before the reader commits; the hub says 'very little' and the home page never mentions Visual Studio** (corrected, minor; https://labradorengine.com/docs/get-started/ (website/src/content/docs/docs/get-started/index.mdx:10-33, 47-52); website/src/content/docs/index.mdx:29-35, 165-169; website/src/content/docs/why.mdx:214)

The hub and home page give no total cost (disk, download, time) before the reader clicks into step 1, and the home page never names Visual Studio; Why's cost paragraph against raylib/SDL omits setup. But the reader does NOT find out 'partway through the commitment': the hub's first card already says 'Visual Studio with the C++ workload', and step 1 states 20-50 GB, ~16 GB measured, several GB download and 'tens of minutes to a couple of hours' explicitly 'Before you click Install', i.e. before anything large is downloaded. 'Very little' answers 'what you need to know', not cost. So this is an honest-but-late expectation gap (one click late), not a major. The reviewer's suggested wording '16 GB or more' is wrong against the page: prerequisites.mdx:346 says the C++ workload alone needs LESS than the 16 GB measured; the minimal figure is unmeasured (REV#63).

*Evidence.* READ index.mdx (hub) :10-33, :47-52; prerequisites.mdx:63-74 ('Before you click Install, look at the bottom of the installer's window ... 20 to 50 GB ... about 16 GB ... tens of minutes to a couple of hours'), :346-347. READ why.mdx:214-216 (cost list: structure, platforms, community, C++ only). RAN curl on live https://labradorengine.com/ : 'Visual Studio' does not occur in the page text; live hub contains 'GB' once only (the 1.3 GB build folder in 'What it was checked against'). Peer sizes (LÖVE, Godot, raylib) not re-fetched by me.

*Suggested.* Put a short 'What this costs' box at the top of Get started, above the cards, repeated in one line on the home page's Start here: one install (Visual Studio, free, no account); several GB to download; 16 GB or more of disk, 20-50 GB by Microsoft's figure; 1-3 hours, almost all of it the installer running unattended; then about 2 minutes for the first build and about 1.2 GB more; every step is a pasted command. Add setup cost to the Why page's raylib/SDL cost paragraph.

### REV#112: Smart App Control or Defender blocking the reader's own MyGame.exe is not covered

<a id="rev112"></a>
*verify, then fix · OPEN · could · S*

From the completeness pass, and not reproduced: no machine with Smart App Control on was tried. The starter page would send such a reader back to rebuild.

**`critic-1`: Nothing covers Smart App Control or Defender blocking the novice's own freshly built MyGame.exe** (completeness pass, not separately verified, major; website/src/content/docs/docs/get-started/starter.mdx steps 6-8; docs/troubleshooting.mdx (Building, Running); the only mention is guides/your-release.mdx:245-257 and :535-540, written about the frien)

A new Windows 11 laptop can have Smart App Control on, and it blocks unsigned programs it does not recognise, which is what the novice has just compiled. Defender can also remove or lock the exe. The novice would see a Windows Security notice, or a failed test, or a missing exe. Step 7 would tell them 'MyGameSmokeTests failing means the game cannot start on this machine' and send them to re-run the build. No page tells them it is a Windows security feature, how to check it, or what to do.

*Evidence.* READ: a search of troubleshooting.mdx and get-started/*.mdx for 'Smart App Control', 'virus', 'quarantin', 'blocked', 'Access is denied' and 'CreateProcess' found nothing. READ: your-release.mdx:247-251 says Smart App Control 'blocks an unsigned program it does not recognise with no Run anyway button', but only for the friend. READ: your-release.mdx:535-537, the site's own record: 'Microsoft Defender reported Behavior:Win32/DefenseEvasion.A!ml against that build of MyGame.exe and blocked every copy of it on the machine'. NOT RUN: no machine with Smart App Control on was available, so what cmake, ctest and the prompt print in that state is unknown.

*Suggested.* Add a Troubleshooting entry under Running, linked from starter steps 7 and 8: how to see whether Smart App Control is on (Windows Security > App & browser control), what its notice and the Defender 'contains a virus' message look like, that a rebuild gives a different file, and the honest options. Check on a machine with Smart App Control on what steps 6 to 8 print, and quote it.

### REV#113: On a Windows that is not in English the quoted messages do not appear

<a id="rev113"></a>
*verify, then fix · OPEN · could · S*

From the completeness pass, and not reproduced: every recovery on the site matches a quoted English message, and cmd, vcpkg and the compiler localise theirs. Whether `VSLANG=1033` restores the English text for all three needs one run on such a system.

**`critic-2`: On a non-English Windows the quoted messages do not appear; vcpkg's output is localised and no page says so** (completeness pass, not separately verified, major; docs/troubleshooting.mdx (whole page, ordered 'by the message they see'); get-started/starter.mdx:96-97; get-started/prerequisites.mdx:187-191; guides/ask-for-help.mdx:25)

The recovery method on every page is to match the message on screen against a quoted English one. A reader on a German, French or Spanish Windows sees translated vcpkg lines, so 'A suitable version of git was not found' never appears and a search of Troubleshooting finds nothing. Windows' own cmd messages ('is not recognized as an internal or external command', 'File Not Found', '1 file(s) moved.') are also translated on such a system. A bug report they file would carry a translated log.

*Evidence.* RAN: Visual Studio's own vcpkg.exe with VSLANG=1031, `vcpkg x-download <file> --url=https://nonexistent-host.invalid/x.zip --sha512=000...` printed 'error: Fehler beim Curl-Vorgang. Fehlercode: 6 (Could not resolve hostname).' and 'Kein vorübergehender Netzwerkfehler...'; `vcpkg version` printed 'Version des vcpkg-Paketverwaltungsprogramms 2026-05-27-...'. RAN: a text search of that vcpkg.exe found 'Eine geeignete Version von {tool_name} wurde nicht gefunden (v{version} erforderlich)' and the French 'Une version appropriée de {tool_name} est introuvable', the translations of the line starter.mdx:97 and troubleshooting.mdx:69 quote. READ: a search of the docs for 'non-English', 'language pack' and 'VSLANG' found nothing. NOT RUN: a real localised Windows. I forced the language with VSLANG; whether vcpkg follows the Windows display language without it, and whether the Visual Studio installer adds a matching compiler language pack, were not checked here. This machine has only the English (1033) compiler resources.

*Suggested.* Add a short section to Install the tools: on a Windows that is not in English, run `set VSLANG=1033` in the prompt (and `setx VSLANG 1033` to keep it) so vcpkg and the compiler print the English these pages quote. Say that error codes such as C2143 and LNK1168 are the same in every language. Say which Windows messages will still be translated. Ask for help should ask for the log in English the same way.

### REV#114: Coming back after a Visual Studio update, and removing everything, are not covered

<a id="rev114"></a>
*verify, then fix · OPEN · could · S*

From the completeness pass. What a build prints after an update was not produced. No page says where vcpkg's cache is or how the space is given back.

**`critic-5`: Coming back after a Visual Studio update is not covered, and the debug page says updating is 'fine'** (completeness pass, not separately verified, minor; get-started/starter.mdx:246-271 ('Coming back later', 'Starting the build again from nothing'); get-started/debug-your-game.mdx:70-73)

Visual Studio offers updates often, and a novice accepts them. The build folder was configured against a compiler in a folder named after its version. After an update the reader runs the same build line, as 'Coming back later' says is 'always safe'. They are likely to get either a failure naming a cl.exe path that no longer exists, or a silent re-download and rebuild of every library because vcpkg sees a different compiler. The list of reasons to start again from nothing names VCPKG_ROOT, the folder and the prompt, but not an update.

*Evidence.* READ: the compiler path the build uses contains the version: prerequisites.mdx:179 shows '...\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe', ask-for-help.mdx:67 quotes a build command line starting 'C:\PROGRA~1\MICROS~1\18\COMMUN~1\VC\Tools\MSVC\1451~1.362\bin\Hostx64\x64\cl.exe', and run2.log line 30 has vcpkg's 'Compiler found: .../MSVC/14.51.36231/...'. READ: starter.mdx:260-262 lists 'VCPKG_ROOT, the folder the project is in, or which prompt you use'. READ: debug-your-game.mdx:71-72: 'Update now closes Visual Studio and updates it, which takes several minutes. Either is fine'. READ: a search of troubleshooting.mdx for 'CreateProcess' finds nothing. NOT RUN: no update was applied, so the exact message is not known; this machine keeps two compiler versions side by side (14.44.35207 and 14.51.36231), so an update does not always remove the old folder.

*Suggested.* Apply one Visual Studio update to a machine with a built starter and record what `cmake --build --preset x64-debug` prints. Then add 'after Visual Studio updates' to the reasons in 'Starting the build again from nothing', add the message to Troubleshooting, and say on the debug page that the next build after an update may need the three-line reset and will rebuild the libraries.

**`critic-3`: No page says how to remove it all or what the starter left on the disk; vcpkg's cache location is never named** (completeness pass, not separately verified, minor; get-started/starter.mdx:99-102 ('vcpkg keeps what it downloaded and built'); get-started/prerequisites.mdx step 1; no uninstall section on any Get Started page)

A reader who gives up, or needs the space back on a 256 GB laptop, deletes C:\dev\mygame and thinks they are done. Visual Studio's 16 GB or so stays. So does vcpkg's cache in their user profile, which no page locates. They are not told that Visual Studio is removed from Visual Studio Installer (More > Uninstall) rather than only from Settings, or that the installer's package cache is separate.

*Evidence.* READ: a search of the docs for 'uninstall' finds only prerequisites.mdx:89 (uninstall other programs to make room), browser.mdx:170 ('To uninstall Emscripten, delete the folder') and a Python line. RAN (measured): the cache folders that the real run in run2.log filled from empty hold 633 MB of downloads and unpacked tools (cmake-4.3.2, PowerShell-7.6.1, msys2 packages, DirectXTK, DirectXMath, doctest, xaudio2redist) and 6.4 MB of built packages. On a novice's machine these go to %LOCALAPPDATA%\vcpkg\downloads and \archives; this machine's %LOCALAPPDATA%\vcpkg has archives, downloads and registries folders. READ: starter.mdx:100 says the configure 'downloaded about 400 MB' and does not say where it is kept. Other pages add C:\dev\mygame-release, mygame-old, new, original and emsdk with no single list.

*Suggested.* Add 'Removing everything' to Get the starter project or Troubleshooting: delete C:\dev\mygame (and the other C:\dev folders these pages make), `rmdir /s /q "%LOCALAPPDATA%\vcpkg"` with its measured size, and Visual Studio Installer > More > Uninstall, with the space each gives back. Name the cache folder at step 5.

### REV#115: The release page invites two wrong downloads, and extracting again silently overwrites an edited project

<a id="rev115"></a>
*fix: no owner call · OPEN · could · XS*

A reader who arrives at the release from GitHub rather than from the starter page sees the docs tarball first and GitHub's *Source code (zip)* at nearly the starter's size; neither holds a `mygame` folder and no page recognises the mistake. Step 3's `tar` line run a second time restores every starter file over the reader's edits without a word.

**`door-1`: Release page: 'Source code (zip)' looks like the download and nothing recognises the mistake** (corrected, polish; https://github.com/matt439/Labrador/releases/tag/v0.3.0 Assets list; website/src/content/docs/docs/get-started/starter.mdx step 2-3 (browser download and tar); docs/troubleshooting.mdx (no entry))

The facts hold (GitHub's 'Source code (zip)' saves as Labrador-0.3.0.zip, holds the engine tree with no mygame folder, and no page names that mistake), but the described dead end is overstated and it does not happen to a novice who follows the page. Step 2's first instruction is a curl box that downloads the right file whatever is already in Downloads; the browser route is the stated alternative and names the exact file ('find labrador-starter-v0.3.0.zip in its Assets list and click it'), and the move box names it again. If they still picked the wrong file, move fails and the page's Ctrl+J advice shows them a file called Labrador-0.3.0.zip, a visible mismatch. The reviewer's chain 'dir /b C:\dev\*.zip shows Labrador-0.3.0.zip' cannot occur by following the page: the failed move never put that file in C:\dev. What remains is a one-sentence gap: nothing says 'Labrador-0.3.0.zip / Labrador-master.zip is the wrong file'.

*Evidence.* READ website/src/components/StarterDownload.astro:66-90 (curl first, then browser route naming the asset, then move with the exact name and the Ctrl+J recovery); starter.mdx:38-66; troubleshooting.mdx:28-32. RAN: fetched live https://labradorengine.com/docs/get-started/starter/ and confirmed the rendered text 'The starter ZIP for Labrador v0.3.0 is labrador-starter-v0.3.0.zip ... curl -fL -o labrador-starter-v0.3.0.zip https://github.com/matt439/Labrador/releases/download/v0.3.0/labrador-starter-v0.3.0.zip'. RAN: curl -I on /archive/refs/tags/v0.3.0.zip returned 'content-disposition: attachment; filename=Labrador-0.3.0.zip'. RAN: fetched expanded_assets/v0.3.0; order is docs tarball, starter zip, Source code (zip), Source code (tar.gz), attestation; the Source code rows show no size. RAN: grep for 'Source code', 'Labrador-0', 'wrong file' in website/src/content/docs and components: no relevant hit. README.md:10-14 sends newcomers to Get Started, not to the release page.

*Suggested.* Add one sentence to the starter page's browser-download paragraph and a Troubleshooting entry: 'If the file you have is Labrador-0.3.0.zip or Labrador-master.zip, that is GitHub's "Source code" link (the engine alone, not a project). Delete it and download labrador-starter-v0.3.0.zip.' For future releases, put a bold 'Download: labrador-starter-vX.Y.Z.zip, then follow Get the starter project' line at the top of the release notes (v0.3.0 is immutable).

**`door-2`: Release page: the docs tarball is listed first, unpacks loose into the current folder, and cannot be read offline** (corrected, polish; https://github.com/matt439/Labrador/releases/tag/v0.3.0 asset labrador-docs-v0.3.0.tar.gz; release body paragraph 1)

Confirmed facts: the docs tarball is the first asset, has no top-level folder (503 entries under 11 top-level names) and the release body never says what it is. The harm is speculative and smaller than stated: no page tells the novice to download or extract it, the release body's only instruction links Get the starter project, the starter page names the starter ZIP exactly, and the only tar command the novice is given names the starter ZIP, so they would have to adapt a command by themselves to spill it into C:\dev. The site does describe the file, on /versions/ ('Archive labrador-docs-v0.3.0.tar.gz, SHA-256 ...'), though not as 'you do not need this'. Offline unreadability was inferred by the reviewer from absolute paths and not observed by either of us.

*Evidence.* RAN: downloaded the asset (8853554 bytes) into my scratch folder; C:\Windows\System32\tar.exe -tf gives top-level names 404.html, _astro (119), docs (168), examples, favicon.svg, index.html, pagefind (208), sitemap-0.xml, sitemap-index.xml, snapshot-build.json, why. RAN: gh release view v0.3.0: the body mentions 'its own documentation, frozen at this tag' and never the tarball. RAN: fetched https://labradorengine.com/versions/ which lists the archive name and hash under v0.3.0. Not opened in a browser.

*Suggested.* In future release notes say: 'labrador-docs-vX.Y.Z.tar.gz is the website's frozen copy, published at labradorengine.com/versions/vX.Y.Z/; you do not need to download it.' Consider packing it under one top-level folder so a stray extraction makes one folder. For v0.3.0 (immutable), add the same sentence to the starter page's browser-download paragraph.

**`starter-04`: Step 3 silently overwrites an existing, edited mygame; the only protection is a sentence in step 1** (confirmed, minor; get-started/starter.mdx:31-36 (warning) and :42-48 (the tar box, StarterDownload.astro:41))

Confirmed. Running step 3's tar line again in a folder that already holds an edited mygame prints nothing, exits 0 and silently restores every starter file over the novice's edits; files they added survive. The only protection is the prose in step 1 (starter.mdx:33-36), which does say `extracting another starter here would overwrite its files with the starter's`. So the page covers it for someone who reads step 1's paragraph, but the command itself does nothing to stop them. Minor is right: it needs a second pass through the page after editing, and the warning is on the page.

*Evidence.* RAN (<scratch>\starter-v\feed.log, case D) in C:\dev\nv-starter-v\d3 with C:\Windows\System32\tar.exe: extract, `echo MY CHANGE>> mygame\game\main.cpp`, `echo x> mygame\game\mine.txt`, extract again: no output, `tar exit 0`; `findstr /c:"MY CHANGE" mygame\game\main.cpp` printed nothing (`findstr exit 1`); `dir /b mygame\game\mine.txt` printed `mine.txt`.

*Suggested.* Make the extract box refuse: `if exist C:\dev\mygame (echo C:\dev\mygame already exists. See A second project.) else (tar -xf ...)`, as the second-project box already does for its name.

### REV#116: Links and next-page order cross from the starter route into the Git route, and the guides index is one ungrouped grid

<a id="rev116"></a>
*fix: no owner call · OPEN · could · S*

Small findability stumbles, each recovered from in seconds, that together make the starter route feel less finished than it is.

**`door-4`: Home page never tells a beginner they are welcome or what they will have at the end** (confirmed, minor; https://labradorengine.com/ ; website/src/content/docs/index.mdx:8, 29-35, 81-121, 168)

The live home page has no sentence telling a near-beginner that Get Started is written for them or what they end up with; that reassurance exists only on Why ('for someone who has hardly programmed') and under the six cards on the Get Started index. The page's body is written for a C++ reader (static libraries, virtual calls, four renderers). Mitigation: 'Get started' is the first hero button, so a novice who clicks before reading reaches the reassurance in two clicks; the tagline now says 'people', no longer 'developers' as the 2026-10-30 audit quoted, but no welcoming sentence was added.

*Evidence.* RAN: fetched https://labradorengine.com/ and grepped the stripped text for 'hardly programmed', 'beginner', 'starter': zero hits. READ index.mdx:8 (tagline), 29-35 (facts), 81-121, 165-172; why.mdx:20-22; get-started/index.mdx:47-52; docs/beginner-guides.md:946 (the audit's entry on why.mdx, which also named the homepage tagline).

*Suggested.* Add one sentence under the hero or above 'Start here': 'New to C++? Get Started takes a Windows PC with nothing installed to a small game of your own that runs, with every command to copy; going further means learning C++.' Link the words to /docs/get-started/.

**`door-8`: Examples' 'Start a project from it' button sends a novice to the Git-route page** (confirmed, minor; website/src/content/docs/examples.mdx:53 -> /docs/get-started/your-project/)

Examples' only button under the minimal sample, 'Start a project from it', opens the Git-route page 'Start your own project', whose first paragraph speaks of a repository and a Git submodule. The recovery is immediate: the second thing on that page is an aside titled 'The starter ZIP is this page, done for you' linking the starter page, so it is a stumble of a few seconds, at the low end of minor.

*Evidence.* RAN: fetched https://labradorengine.com/examples/ ; it contains 'href="/docs/get-started/your-project/">Start a project from it'. READ examples.mdx:53; your-project.mdx:16-25.

*Suggested.* Point the button at /docs/get-started/ (or /docs/get-started/starter/) and, if wanted, add a second minimal link 'or build it by hand with Git'.

**`peers-8`: The home page's 'Get started' card describes the long Git route, not the starter route the path now takes** (confirmed, minor; website/src/content/docs/index.mdx:168 (home page, 'Start here'))

The home page's 'Get started' card still describes the Git route's four stages ('Install the tools, build the engine, run the minimal sample and start a project of your own'), while the hub it links to gives three different steps and the starter page says 'you do not build Labrador on its own first'. The reader lands on the right hub regardless, so it misleads expectation but does not misroute; minor is right.

*Evidence.* RAN curl on live https://labradorengine.com/ : the text 'Install the tools, build the engine, run the minimal sample and start a project of your own.' is present. READ website/src/content/docs/index.mdx:168; hub index.mdx:31-33; starter.mdx:11. Not found in docs/review/2026-11-10 or docs/beginner-guides.md.

*Suggested.* Change the card to 'Install the tools, download the starter project, build it, play it and make a first change.'

**`ide-5`: Edit and debug page: no short version at the top, and a third of it is reference and test record a beginner should not have to pass** (confirmed, minor; website/src/content/docs/docs/get-started/debug-your-game.mdx:9-23 (intro), 561-623, 645-737; live https://labradorengine.com/docs/get-started/debug-your-game/)

Confirmed, with the balance noted. The essential path is first on the page and not buried: Open the project, then Edit, build and run, end by line 198. What is missing is a sentence telling a reader who only wants an editor that they can stop there, and from line 561 the page changes register to reference material and then a 93-line test record. Two of the three reference sections already say a beginner can skip them (line 588 'a beginner can leave them out', line 611 'A beginner can skip this section'); 'Where content comes from' does not.

*Evidence.* Read debug-your-game.mdx in full (737 lines): 15 second-level headings; Open the project 25-148; Edit, build and run 150-198; Where content comes from 561-582; Direct3D diagnostics 584-607; Keep matching symbols 609-623; keys table 625-643; What was checked 645-737. Line 14 'It takes about 30 minutes.' Quotes at 571, 614 and 736 as given. Every line number the page tells the reader to use (44, 75, 76, 77, 86, 87, 105, 155) matches the v0.3.0 starter's hello_state.cpp, read line by line.

*Suggested.* Add a five-line 'In short' box at the top (devenv ., choose x64-debug, choose MyGame.exe, Ctrl+Shift+B builds, F5 runs under the debugger, Shift+F5 stops) and a sentence saying where a reader who only wants an editor can stop. Move 'Where content comes from', 'Direct3D diagnostics' and 'Keep matching symbols' to a guide or to Troubleshooting. Put 'What was checked' behind a collapsed disclosure or on the repository record, and move 'The keys on this page' up to follow 'Stop debugging'.

**`ide-6`: The debug page's Next link leads the starter reader into the Git route, and the page has no 'where to go next'** (confirmed, minor; live https://labradorengine.com/docs/get-started/debug-your-game/ footer; website/astro.config.ts sidebar order)

Confirmed from the sources. The page has no 'Where to go next' section and no prev/next override, and the sidebar puts 'Build Labrador', the first page of the Git route, directly after it, so the default footer link crosses routes.

*Evidence.* Read website/astro.config.ts:62-64 and :70 (Make a first change, Read a build error, Edit and debug in Visual Studio, then the group holding Build Labrador). Ran grep for '^prev' and '^next' in get-started/*.mdx: no matches. debug-your-game.mdx ends at 'What was checked' (645-737). The live footer was the reviewer's fetch; I did not re-fetch it.

*Suggested.* Add a 'Where to go next' card grid (the your-* guides, Troubleshooting) before 'What was checked', and set the page's `next` in frontmatter to the guides index so the footer link does not cross into 'With Git instead'.

**`ide-7`: The Visual Studio page is hard to find from the body of Make a first change** (confirmed, minor; website/src/content/docs/docs/get-started/first-change.mdx:23-28, 276-301)

Confirmed. The Notepad paragraph of Make a first change mentions 'any text editor or IDE' and Visual Studio by name without a link, and the Visual Studio card is 11th of 18. The page is reachable from the sidebar, from Read a build error, the guides index and Troubleshooting, so it is a findability stumble only.

*Evidence.* Read first-change.mdx:23-28 and the card list at 283-300 (Edit and debug in Visual Studio at line 293). Ran grep for 'debug-your-game' across the docs: linked from build-errors, first-change, get-started/index, guides/index, your-name and troubleshooting.

*Suggested.* Link the page from the Notepad paragraph ('Visual Studio, which you already installed, is a better editor: Edit and debug in Visual Studio opens your project in it') and move its card into the first row of 'Where to go next'.

**`content-4`: Guides index is 43 ungrouped cards; a novice cannot tell starter walk-throughs from sample reference pages** (corrected, minor; website/src/content/docs/docs/guides/index.mdx:11-230 (live: https://labradorengine.com/docs/guides/))

Confirmed with a count correction: the Guides index is one ungrouped grid of 42 cards (not 43) with no headings, under an intro written for the sample-reference pages ('shows how a sample in the repository does it ... The quoted code is in the engine's samples folder, not in your game'), which misdescribes the step-by-step starter pages that come first. Near-identical pairs (Play a sound in your game / Audio, Put collision in your game / Collision, Make a title menu / Menus, Save settings in your game / Settings and saves) sit in one grid unlabelled, and no order, prerequisite or time is shown. The sidebar's Guides group is likewise one flat autogenerated list. Mitigations exist but do not remove it: the starter pages are listed first, each your-* page states its own prerequisites and minutes at the top, and Make a first change's 'Where to go next' is a curated 18-card grid whose first cards are the right ones. Minor stands.

*Evidence.* READ/RAN: `grep -c "<LinkCard" guides/index.mdx` = 42; one `<CardGrid>` at :19-230, no `#` headings; intro :11-17. Live https://labradorengine.com/docs/guides/ fetched: only heading is 'On this page'. astro.config.ts:79 `{ label: 'Guides', collapsed: true, items: [{ autogenerate: { directory: 'docs/guides' } }] }` (no sub-groups). first-change.mdx has 18 LinkCards, starting Put a picture / Colours / Play a sound. your-picture.mdx:14-22 states time and prerequisites.

*Suggested.* Split the grid under three headings: 'Step by step in your starter project' (the your-* pages, in a suggested order with minutes and 'needs: content tools'), 'Sharing and tools' (Git, GitHub, release, browser, version, help), and 'How the engine's samples do it' (reference pages). Reword the intro so the first sentence addresses a starter reader.

**`skew-10`: The two v0.3.0 errata are visible only on /versions/, and their links open the Development page, not the page they correct** (confirmed, minor; website/published-versions.json errata[]; live https://labradorengine.com/versions/ ; snapshot pages /versions/v0.3.0/docs/get-started/build-labrador/ and /your-project/)

The two v0.3.0 errata appear only on /versions/, and each links to the Development page rather than the frozen page it corrects; the frozen Build Labrador page carries no mark. Both concern the Git route, so the starter-ZIP novice is not affected.

*Evidence.* READ website/published-versions.json: errata href '/docs/get-started/build-labrador/' and '/docs/get-started/your-project/'. RAN curl: /versions/ has href="/docs/get-started/build-labrador/" on the erratum text; /versions/v0.3.0/docs/get-started/build-labrador/ has 0 lines with 'errat' and 0 with 'git checkout v0.3.0'.

*Suggested.* Point each erratum's href at the snapshot page it corrects (/versions/v0.3.0/docs/get-started/build-labrador/). The version selector already fetches /versions/index.json at run time on frozen pages, so it could also show 'This release has 2 errata' with a link, without rebuilding the snapshot.

### REV#117: Day-two guides interact in ways no page states

<a id="rev117"></a>
*fix: no owner call · OPEN · could · S*

Found by following the guides in a v0.3.0 starter: four pages still present the start-up test as the check after *Add a screen* has changed what it runs; *Bigger text* followed by the levels page times the test out; the build folder keeps an old texture after the source is swapped; and the picture guide says a picture must be a PNG where its own `texconv` line reads a JPG.

**`gameplay-6`: After Add a screen, the start-up test no longer runs HelloState, but five later pages still present `ctest` passing as the check and diagnose failures through it** (corrected, minor; website/src/content/docs/docs/guides/your-level.mdx:633-634 and :680-695; your-camera.mdx:280-285; your-two-players.mdx:408-412; your-collision.mdx:541-543. Cause stated only at your-screen.mdx:454-46)

Add a screen does tell the novice, under its own heading 'What the start-up test checks now' (your-screen.mdx:454-462), that ctest now checks only the title, that a HelloState mistake 'still passes ctest', and to 'play past the title before you rely on it'. So the cause is covered where the change is made, and the failure, when it comes, is the same message in an error box that the later pages already list. What remains true: four later pages (your-level, your-collision, your-camera, your-two-players; the reviewer's title says five) never mention the title screen and state 'The start-up test fails with this' as fact, and your-level.mdx:633 offers the build line as a way 'to check a file for mistakes without playing every level', which is false after Add a screen. That is a contradiction between pages a novice may not reconcile days later, but they were warned and can recover: minor, not major. I did not re-run the broken levels.json case.

*Evidence.* READ: your-screen.mdx:454-462; your-level.mdx:633-634, :680-695; your-camera.mdx:280-285; your-two-players.mdx:408-412; your-collision.mdx:541-543. grep for 'title screen|your-screen|TitleState|Add a screen' in your-level, your-collision, your-camera, your-two-players, your-window: no match (your-menu.mdx:508 does state it). Sidebar orders: screen 0.91, level 0.92, collision 0.96, camera 0.965, two-players 0.966, so the later pages do follow. The reviewer's run (ctest passes with broken levels.json behind a title) is consistent with the page's own statement; not re-run by me.

*Suggested.* On each later page, where it says the start-up test catches something, add: 'If your game opens on a title screen from Add a screen, the start-up test checks only the title: press Enter and play past it.' Better, have Add a screen show how to make `--smoke-test` wrap HelloState (or run both), so the later pages stay true.

**`gameplay-7`: 'Bigger text' from the window page plus the levels page makes the start-up test time out for 60 s and opens the game on level 2** (confirmed, minor; website/src/content/docs/docs/guides/your-window.mdx:150-170 with your-level.mdx:621-626 and :760-769)

Bigger text (`, 2.0f`) from the window page, which the sidebar puts before the levels page, doubles the greeting's box to about 490-1190 across and 330-490 down; the levels page's own first goal is at x 1000-1120, y 300-420, so the greeting touches it at once and the start-up test times out at 60 s. The levels page's Timeout entry exists but gives the 1x extent (490 to 840), so its explanation appears not to apply. Confirmed by arithmetic from the page's figures and the reviewer's run; I did not apply the levels page myself. That the game opens on level 2 is inferred, not seen.

*Evidence.* READ: your-window.mdx:150-170 (Bigger text; 'grows to the right and down'); your-level.mdx:61 and :592 (`{ "x": 1000, "y": 300, "width": 120, "height": 120 }`), :508 (`this->greeting_->bounds().intersects(this->goal_->bounds())`), :621-626 ('its text covers about 490 to 840 pixels across and 330 to 410 down'), :760-769 (Timeout entry). The levels page's 'Before you start' (:28-37) says it works regardless of the picture, animation and font pages and does not mention the window page. Not run by me: would be settled by applying the levels page plus `, 2.0f` and running ctest (the reviewer reports `***Timeout 60.02 sec`).

*Suggested.* In the levels page's 'Keep goals away from where the greeting starts' bullet and its Timeout entry, add: 'If you made the greeting bigger on the colours page, its text is twice as wide and reaches to about 1190 across; move level 1's goal or put the text back to its own size.'

**`content-3`: Build folder keeps the old texture file, so swapping .dds and .png misleads in both directions** (confirmed, minor; your-picture.mdx:94-95 ("Keep one of the two files in the folder, not both"), :413-417 (Changing the picture), :489-491; troubleshooting.mdx:881-884 ("Delete the one you no longer use"))

Confirmed for v0.3.0 by my own run: after the .dds route, copying hero.png in and deleting game\content\textures\hero.dds as the box says leaves the old hero.dds beside the exe, so the test passes and the game keeps drawing the stale .dds; the novice concludes PNG works and later edits to hero.png never show. The newer-engine half (the 'two files' stop persisting after deleting one from content) is read from master's source, not run, but follows directly from the same stale copy. Neither the box, 'Changing the picture', nor Troubleshooting's two-files entry mentions the copy under out\build; assets.mdx:52-58 explains the mechanism but is not linked from the picture page. Minor is right: it only bites a reader who switches routes.

*Evidence.* RAN (content-v\e.log section B) in C:\dev\nv-content-v\mygame: with both files present, `del game\content\textures\hero.dds`, build, ctest: `MyGameSmokeTests ... Passed`; `dir /b game\content\textures` -> `hero.png`; `dir /b out\build\x64-debug\game\textures` -> `hero.dds` and `hero.png`. READ master engine/render/resource_factory.cpp:85-87 (`if (has_dds && has_png)` throws `Texture '...' has two files`), which tests the folder the game reads. READ troubleshooting.mdx:881-884 ('Delete the one you no longer use'), your-picture.mdx:94-95; grep of your-picture.mdx for 'stale'/'never removes' finds nothing.

*Suggested.* In the PNG box and in Troubleshooting's two-files entry, add the line to run: `del out\build\x64-debug\game\textures\hero.dds` (or .png), with one sentence that the build copies new files beside the game but never removes old ones.

**`content-5`: Using your own picture: only PNG is covered, with no route for a photo/JPG, a large image, or replacing hero.png** (corrected, minor; your-picture.mdx:32-55 (step 1) and :413-417 (Changing the picture))

Partly confirmed, and one part is stricter than the reviewer said. Step 1 says the picture 'must be a PNG' and gives no route for a JPG or phone photo and no way to shrink a large one; that stands as a minor gap. In fact the page's own texconv line reads a JPG as it is, so 'must be a PNG' is only true for the PNG box, and the page could simply say a .jpg works with the texconv line (I did not load the resulting .dds in the game). The `Overwrite hero.png? (Yes/No/All):` part is polish at most: the prompt is self-explanatory, and neither reviewer could reproduce it because `copy` does not ask outside an interactive prompt; it rests on documented Windows behaviour and on your-sound.mdx quoting the same prompt for the same command.

*Evidence.* READ your-picture.mdx:32, :44 (`copy` without /y), :53-55; your-sound.mdx:458-463 (`copy /y`, quotes the Overwrite prompt). RAN (content-v\e.log section D) the page's texconv options on a real JPEG: `reading ...photo.jpg (64x64 R8G8B8A8_UNORM 2D a:Opaque) as (64x64 R8G8B8A8_UNORM 2D a:Opaque)` / `writing ...photo.dds`; a JPEG merely renamed to .png converted the same way. RAN `echo n| cmd /c copy ... hero.png` over an existing file from a batch file: `1 file(s) copied.` with no prompt, so the prompt is unverifiable here; an interactive prompt (which I may not drive) would settle it.

*Suggested.* Use `copy /y` in step 1 with the sound page's sentence. Add two sentences: 'A JPG or other picture: open it in Paint and use File > Save as > PNG picture. Too big: Paint's Resize (Ctrl+W), in pixels.'

### REV#118: The starter ZIP is the whole engine repository, and its own files describe themselves as the minimal sample

<a id="rev118"></a>
*owner's decision · DECISION · could · S*

Of 1,146 files, 16 are the reader's. A search for `hello_state.cpp` finds two. `game\README.md` is titled *Minimal sample* and its comments name targets the project does not have. Whether the ZIP is slimmed, and whether `starter_zip.py` rewrites those names, is a scope call.

**`starter-02`: Starter folder is 1,146 files of which 16 are the novice's; the rest is the whole engine repository, with a second copy of every game file** (corrected, minor; labrador-starter-v0.3.0.zip, mygame\external\labrador; page tree at get-started/starter.mdx:211-212 says only `labrador/ the engine, which you do not edit`)

Confirmed with one count corrected. The extracted starter is 1,146 files (24 MB), of which 16 are in game\. external\labrador is the whole engine repository: AGENTS.md, CLAUDE.md, TODO.md, CONTRIBUTING.md, SECURITY.md, its own CMakePresets.json, plus docs, website, tests, tools and bench. A search for hello_state.cpp finds two files. README-named files number 30, not 31 (122 .md files is right). The page does say `Everything you change is in game. Leave external\labrador as it is` (starter.mdx:242-243) and README.txt says `The game in the game folder is yours to change`, and every guide gives full game\ paths. The cost is therefore confusion when browsing or searching, not a wrong instruction.

*Evidence.* RAN on the pristine extract: `find . -type f | wc -l` gives 1146; `find game -type f | wc -l` gives 16; `find . -name hello_state.cpp` gives ./external/labrador/samples/minimal/states/hello_state.cpp and ./game/states/hello_state.cpp; `find . -iname 'README*' | wc -l` gives 30; `find . -name '*.md' | wc -l` gives 122; `du -sh` gives 24M; `ls external/labrador` lists AGENTS.md CHANGES.md CLAUDE.md CMakeLists.txt CMakePresets.json CONTRIBUTING.md LICENSE NOTICE README.md SECURITY.md TODO.md bench cmake docs engine external samples tests tools vcpkg.json website. READ: starter.mdx:211-212 and 241-244; the ZIP's README.txt.

*Suggested.* Either leave website/, docs/review, bench, tools, agent briefs and TODO out of the starter ZIP (keeping what the guides use: engine, cmake, samples, LICENSE/NOTICE), or say plainly on the page and in README.txt that `external\labrador` is the whole engine repository, that it contains a look-alike `samples\minimal`, and that only `game\` is theirs.

**`ide-4`: The starter's own files describe themselves as the minimal sample and name things the project does not have** (confirmed, minor; v0.3.0 ZIP: game\README.md:1-24; game\main.cpp:1-3, 46; game\rules\motion.h:10; game\tests\motion_tests.cpp:2)

Confirmed for the v0.3.0 ZIP. game\README.md is titled 'Minimal sample', describes how the folder gets copied, and says the includes name samples/minimal here, which is false in the starter. Comments name MinimalRules, MinimalTests and MinimalSmokeTests, where the project's targets are MyGameRules, MyGameTests and MyGameSmokeTests. main.cpp opens with 'copy samples/minimal/, rename the target'. Two things soften it: the README's first paragraph does say the starter ZIP is this template ready-made, and the root README.txt, the first file a novice sees, is short, correct and written for the starter.

*Evidence.* Ran grep over the pristine v0.3.0 game folder: README.md:1 '# Minimal sample', :13 'which name `samples/minimal/...` here'; main.cpp:2 'project starts from: copy samples/minimal/, rename the target'; main.cpp:46 'MinimalSmokeTests'; rules/motion.h:10 'MinimalRules'; tests/motion_tests.cpp:2-3 'MinimalRules', 'MinimalTests'; README.md:134-137, :197 'MinimalSample'. game\CMakeLists.txt:3 'add_library(MyGameRules STATIC', :26, :78 'add_test(NAME MyGameSmokeTests'. starter.mdx:215 'README.md how the template is arranged, and what changed in it'. Read tools/consumer_check/starter_zip.py: it adds README.txt; only include paths are rewritten, not comments.

*Suggested.* Have starter_zip.py rewrite or replace these on the way into the ZIP: a short game\README.md written for the starter (what each file is, link to the guides, the change list as an appendix), `MinimalRules`/`MinimalSmokeTests` rewritten to the MyGame names as the include paths already are, and main.cpp's first comment reworded for a project that is already a copy.

### REV#119: No page reads the starter's own files at a beginner's level, and Get Started has no glossary

<a id="rev119"></a>
*owner's decision · DECISION · could · M*

*Reading the C++ in minimal* states an audience above the 2026-10-30 floor, and *Make a first change* routes the beginner to it and to comments written for engine programmers. The `your-*` guides teach the basics in place, so this is a gap in understanding and not in doing.

**`ide-1`: The route to understanding the code is a loop with no exit for a near-beginner** (corrected, minor; website/src/content/docs/docs/get-started/first-change.mdx:278-280; concepts/reading-minimal.mdx:16-18; get-started/index.mdx:57-70; concepts/index.mdx:20-22)

Overstated as 'nowhere to go'. The reader has a clear route for doing: reading-minimal says outright that the page is not needed to change the game and that first-change and the step-by-step guides give every line, and first-change's card grid follows immediately with nine your-* guides, which explain header/.cpp, #include, const and override as they go. What stands: (a) first-change's prose puts the expert comments and Reading the C++ ahead of the cards ('After that:'), so the first thing recommended is the page that says it is not for them; (b) concepts/index tells a reader 'new to C++' to keep reading-minimal open, while reading-minimal says it assumes they can already write functions and classes; (c) the 'preparation route' names no introduction, and its companion-introduction paragraph describes something unavailable and aimed at people who already program. A stumble and a contradiction, not a dead end.

*Evidence.* Read. reading-minimal.mdx:18-20 'You do not need this page to change your game: Make a first change and the step-by-step guides give every line to type.' first-change.mdx:278-301 (prose, then 'After that:' and the cards). guides/your-screen.mdx:32 'A class in C++ is usually two files: a header, ending .h ...', :76 (const, override), :187 ('The #include lines bring in what the code uses'); guides/your-rule.mdx:57 'A header, a .h file, says what functions a file of code offers'. Contradiction confirmed: concepts/index.mdx:20 against reading-minimal.mdx:16. get-started/index.mdx:57-70 confirmed as quoted; it also says plainly 'these pages do not teach the language', which is a stated scope. docs/beginner-guides.md:825 records the assumption as minor, with 'Try it' boxes proposed and not present on the page.

*Suggested.* Name one or two specific free introductions in get-started/index.mdx (with the chapters that matter: variables, functions, classes, pointers). Make concepts/index and reading-minimal say the same thing about who the page is for. Drop or shorten the companion-introduction status until it exists. Add the per-section 'Try it' edits the audit proposed, so the page is usable by doing rather than by prior knowledge.

**`ide-2`: Reading the C++ in minimal skips what a beginner meets first and explains expert terms instead** (corrected, minor; website/src/content/docs/docs/concepts/reading-minimal.mdx (whole page, 199 lines); starter game\states\hello_state.cpp and game\main.cpp)

The facts are right: reading-minimal's nine sections do not follow either file's order, three are Direct3D vocabulary, and it does not explain #include, ::, this->, const, & and * or the unnamed namespace. But the page states its audience (can already write small C++ functions and classes), so this is not the page failing; and the basics are not absent from the site, they are taught in place by the your-* guides. The real gap is narrower: no page walks the starter's own three files top to bottom at beginner level, and Make a first change routes the beginner to this page as if it did. run-minimal's 'What you are looking at', which starter.mdx links for 'which code makes each thing on the screen', is three short paragraphs and is on the Git route.

*Evidence.* Read. Section order and the starter line references checked against the pristine v0.3.0 copy: hello_state.h:26 class, :48-49 fan-out comment and unique_ptr; hello_state.cpp:139 lambda, :163 span, :211 deferred context; main.cpp:22 using namespace, :100 wWinMain, :120-121 dynamic vertex buffer. reading-minimal.mdx:16 states the assumption. In-place explanations: guides/your-screen.mdx:32, :76, :187; guides/your-rule.mdx:57. starter.mdx:195-197 links run-minimal#what-you-are-looking-at; run-minimal.mdx:35-58 is that section. Ran a script over every SourceFile excerpt of samples/minimal on the site: all from/to anchors on reading-minimal, game, tick, states and the debug page exist in the v0.3.0 starter's files.

*Suggested.* Add a page (or a first half of this one) that walks game\main.cpp then hello_state.h/.cpp in file order at the 'what is this punctuation' level: include, namespace, const, `::`, `this->`, `->` versus `.`, `&`/`*`, `L""` and `f` suffixes, a function's parts, try/catch. Keep the current nine sections as the second, optional half, and move the three rendering sections under a 'you can skip these' heading.

**`ide-3`: The comments recommended as 'the best next thing to read' are written for engine programmers** (confirmed, minor; first-change.mdx:278-279; starter game\states\hello_state.cpp:19-42, 82-83, 206-219; game\main.cpp:118-122)

Confirmed. first-change calls the comments in hello_state.cpp 'the best next thing to read'; in the v0.3.0 starter they are 66 of 221 lines and are written to an engine programmer: tenet numbers, deferred contexts, sprite batches, command lists, the pad API's deadzone modes.

*Evidence.* Read the v0.3.0 starter's game\states\hello_state.cpp: lines 19-21, 24-27, 82-83, 211-214 as quoted. Ran: grep -c '^\s*//' hello_state.cpp = 66; wc -l = 221. first-change.mdx:278-279 as quoted.

*Suggested.* In first-change, stop calling the comments the best next read for everyone; say they are written for programmers and point beginners at the guides (Put a picture in your game, Change a control) as the next thing to do. Longer term, have starter_zip.py ship a plainer comment set, or put a two-line plain summary above each expert comment block.

**`peers-4`: Get Started's three pages use about 45 technical terms without explaining them, against 'what you need to know already: very little'** (corrected, minor; get-started/index.mdx, prerequisites.mdx, starter.mdx, first-change.mdx (the hub plus the three steps))

The count is roughly right (about 40-45 terms used before or without explanation across the hub and three steps) and no glossary exists on the site. But the reviewer concedes the terms 'rarely block', and several listed items are explained in place or sit in passages the starter reader is told to skip: LNK1168 is explained where used ('a program that is running cannot be replaced') and linked; 'wide string' is explained ('which is what the engine's text drawing takes') and the instruction 'keep ... the L' comes first; UTF-8/ANSI is given its consequence and where Notepad shows it; Git/clone/submodule/commit/tag/CI are in 'If you use Git' and 'What it was checked against', which are not on the starter line; vcpkg is glossed on the same page (prerequisites.mdx:107), 50 lines after first use. The terms that are on the main line and never explained are: workload, CMake, Ninja, SDK, shader compiler, PATH, VCPKG_ROOT, x64/64-bit/32-bit, compiler, configure, debug build, Direct3D 11 renderer, 'warnings are errors', frames, constant, radians, window class name. That is a stumble that makes the prose feel harder than the actions are, against a 'nothing to infer' decision; it does not stop or likely lose the reader, so minor, not major.

*Evidence.* READ hub index.mdx, prerequisites.mdx:9-15, :57-61, :107, :146-148; starter.mdx:90-102, :124-127, :183-185; first-change.mdx:23-28, :72-76, :93-96. RAN grep -i glossary over website/src/content/docs: no page; BACKLOG.md:2653 SRL-16 is 'a glossary of the terms minimal uses' (C++ terms, concepts/reading-minimal), not the setup vocabulary.

*Suggested.* Add a one-screen 'words these pages use' list linked from the hub, with one plain sentence each for compiler, build, configure, CMake, vcpkg, preset, PATH, 64-bit, library, test and engine. Remove names the reader never acts on from the main line: the five library names in configure, 'shader compiler', Ninja, Cross Tools. Move them to an expandable 'what it is doing' note. For first-change, say 'leave the L and the f where they are' before the reason.

### REV#120: The one specific command prompt is the costliest concept on the route, and the starter ships nothing that removes it

<a id="rev120"></a>
*owner's decision · DECISION · could · S*

Batch files in the starter that find Visual Studio with `vswhere` and run configure, build, test and play would remove the prompt, the five-line check and `REV#106`'s cause. They border the 2026-10-30 decision that terminal steps are acceptable when guided, so the call is the owner's.

**`peers-2`: The specific command prompt is the costliest concept on the route, and the product offers no script that removes it** (corrected, minor; prerequisites.mdx:123-192 (steps 3-4), 237-278; starter.mdx:18-19, 248-253; starter ZIP root (README.txt, no .bat files))

The x64 Native Tools Command Prompt is a real extra concept no peer has, and the v0.3.0 ZIP ships no helper script (root holds only .github, .gitignore, CMakeLists.txt, CMakePresets.json, external, game, README.txt, vcpkg.json). But the claim that a wrong window 'gets a failure many steps later, at configure or build' is refuted for the first run: step 4 'Check it' follows immediately and maps each wrong window to its symptom ('cmake is not recognized' -> not the x64 prompt; 'x86' -> 32-bit prompt), and step 3 has the banner line to compare. On later sessions a plain prompt fails at the first cmake command with the same message, which Troubleshooting's 'The wrong command prompt' covers; only a 32-bit Developer prompt on a return visit fails later, at build. The vswhere line in run2.log is the harness's own script and is not evidence about the novice. As a product idea (check.bat/build.bat that call vcvars64 themselves) it stands, is not recorded, and is not a bundled installer; as a defect it is a minor, well-guided stumble, not a major.

*Evidence.* READ prerequisites.mdx:123-192 (step 3 banner, three wrong-window bullets, step 4 five commands and four readings). RAN ls on starter-pristine\mygame: no .bat/.cmd/.ps1 at root. RAN curl on live /docs/troubleshooting/: 'wrong command prompt' occurs (4 matches). READ run2.log:1 ('vswhere.exe' is not recognized) precedes the harness's own '=== curl' marker. RAN grep of TODO.md, docs/review/2026-11-10, docs/beginner-guides.md for check.bat/bootstrap: no such item (GAP-REGISTER:1276 mentions a walkthrough check.bat output only as a past record; website/src/walkthrough holds no .bat). Not settled: whether build.bat by double-click works on a machine where VS is in a non-default place; a Sandbox run of a prototype would settle it.

*Suggested.* Product, not page: ship three small batch files in the starter ZIP. check.bat finds Visual Studio with vswhere, calls vcvars64.bat itself, verifies cmake, ninja, cl, fxc and VCPKG_ROOT, the project path length and free disk, and prints one line saying it is ready or the single thing to fix. build.bat calls vcvars64 itself, then runs the same three cmake/ctest preset commands. play.bat starts the game. They work from any command prompt or a double-click, so the special prompt becomes optional knowledge. This installs nothing, so it is not the 'bundled installer' the owner declined (docs/product/BACKLOG.md:25), and it touches no PHILOSOPHY non-goal (PHILOSOPHY.md:871-891): the scripts wrap the documented commands and can print each one as they run.

### REV#121: The first configure downloads a CMake and a PowerShell although Visual Studio has just installed both

<a id="rev121"></a>
*owner's decision · DECISION · could · S*

170 of about 199 MB of the cold configure's downloads on 2026-11-10 were these two tools, fetched because vcpkg wanted newer versions than Visual Studio 2026 carries. Whether anything in the starter can avoid that was not investigated.

**`peers-6`: The first-run configure spends its 93 seconds and its downloads on tools Visual Studio already has; a bootstrap or cached dependencies would cut what is left after the install** (corrected, minor; starter.mdx:84-116 (step 5); run2.log lines 18-196)

Confirmed that the v0.3.0 first configure prints 'A suitable version of cmake was not found (required v4.3.2)' right after the previous page showed 'cmake version 4.3.1-msvc1' as the passing check, then fetches a 53 MB CMake and a 117 MB PowerShell: 170 of about 199 MB downloaded. The page covers it only as 'if it finds Visual Studio's unsuitable' and quotes the Git message but not these two. Overstated: the title says configure 'spends its 93 seconds' on those tools; the log shows 39 s is building the six packages (DirectX Tool Kit alone 24 s), so the tools are most of the BYTES and at most about half the TIME on this link, more on a slow one. Suggested fix (2), shipping prebuilt dependencies in the ZIP, would not remove the two big downloads: vcpkg fetches CMake before it reads the registry and PowerShell before any package is built, so only a different mechanism (no vcpkg at configure, or a vcpkg whose pinned tool versions match) removes them. Not recorded elsewhere.

*Evidence.* READ run2.log:19-29 (cmake and powershell-core messages precede 'The following packages will be built'), :142 ('directxtk ... 24 s'), :185 ('completed successfully in: 39 s'), :193 ('Configuring done (92.6s)'). RAN ls -la on the run's vcpkg-cache/dl: PowerShell-7.6.1-win-x64.zip 117,085,974; cmake-4.3.2-windows-x86_64.zip 52,961,068; xaudio2redist 24,018,213; doctest 2,245,013; msys2 runtime 1,987,651; DirectXTK 429,008; DirectXMath 262,470; pkgconf 102,496. READ starter.mdx:95-98 and prerequisites.mdx:175.

*Suggested.* Product options in rising cost. (1) A one-paste bootstrap (download, extract, configure, build, test, play) as an alternative to steps 1-8; it installs nothing, so it is not a bundled installer and touches no non-goal. (2) Ship the four small dependencies prebuilt for x64-windows in the starter ZIP, or as a vcpkg binary-cache asset, so the first configure needs no network beyond the ZIP. (3) Prebuilt engine libraries would save only the 7 s compile and work against the readable, source-built engine the design sells (FINDINGS.md:524 calls a package 'against ARCHITECTURE's source-built game project'); I do not recommend it. On the page, quote the two 'A suitable version of ... was not found' lines as expected output, as it already does for Git.

## Evidence added to existing rows

### REV#62: Development guides use features the release starter does not have

<a id="rev62"></a>
The 2026-11-10 review inferred one instance and did not run it. This review ran the PNG instance in a v0.3.0 starter and found four more by following the pages.

**`content-1`: "Or keep it a PNG" box fails on the released v0.3.0 starter, and nothing at the passage says so** (confirmed, major; website/src/content/docs/docs/guides/your-picture.mdx:89-98 (live: https://labradorengine.com/docs/guides/your-picture/ step 2); also content-tools.mdx:10-14 and :18 ("The engine reads a PNG as it is )

Confirmed by an independent from-scratch run. A novice on the released v0.3.0 starter who takes the 'Or keep it a PNG' box (no texconv, as content-tools also invites) and does steps 1-8 with hero.png in place of hero.dds gets a clean build and then a failed start-up test naming a missing hero.dds. No sentence at the box, in content-tools, or in the failure box says the starter ZIP's engine cannot read PNG; the only version signals on the live page are the footer line and a sidebar picker labelled 'Documentation version: Development', neither at the point of use. The failure box's bullets (spelling, folder, copy block) are all correct in this state; only its last clause ('that game\content\textures\hero.dds exists') leads back to texconv. Recovery does work once found: running the step 2 texconv line and building again passes, even with hero.png left in the folder. Major stands (not blocker: the main route with texconv works, and it is the route the numbered step teaches). Already recorded as REV#62 (Open, owner's decision), where the failure was inferred; it is now run twice.

*Evidence.* RAN in my own copy C:\dev\nv-content-v\mygame (v0.3.0 ZIP extracted with C:\Windows\System32\tar.exe; base build 2/2 passed): step 1 orange-face line, `mkdir game\content\textures`, `copy hero.png game\content\textures`, steps 3-7 as worded with `content/textures/hero.png` in the CMake line, then build (exit 0, [5/6] Linking) and ctest: `2/2 Test #2: MyGameSmokeTests .................***Failed    0.22 sec` / `startup failure: No file at C:\dev\nv-content-v\mygame\out\build\x64-debug\game\.\./textures/hero.dds.`; `dir /b out\build\x64-debug\game\textures` printed only `hero.png` (log: scratchpad\content-v\pngroute.log). Then texconv line + build + ctest: 2/2 Passed (content-v\e.log section A). READ: grep for png in the pristine ZIP's external\labrador\engine finds only a SEAM.md sentence, no reader. READ live https://labradorengine.com/docs/guides/your-picture/ fetched 2026-11-10: box present, no version note near it; footer 'Development documentation (unreleased). Built from Labrador 3067c67 of 2026-11-10.' READ content-tools.mdx:11-14 and :18; your-picture.mdx:568-570 ('have not been followed in a starter'); starter README.txt points to https://labradorengine.com/docs/get-started/. READ FINDINGS.md REV#62: 'That outcome is inferred ... and was not run'.

*Suggested.* Until a release contains PNG loading, put one sentence at the top of the box and in content-tools: 'This needs a Labrador newer than v0.3.0. The starter ZIP's engine reads only .dds; with a PNG it stops with `No file at ...hero.dds.`' Or cut a release. Longer term, settle REV#62 (banner or per-passage notes) and consider pointing the ZIP's README.txt at /versions/<tag>/docs/get-started/.

**`skew-2`: 'Or keep it a PNG' cannot work in the v0.3.0 starter, and three more pages now repeat the claim** (confirmed, major; website/src/content/docs/docs/guides/your-picture.mdx:89-98 (aside) and :483 (failure title); guides/content-tools.mdx:10-14; guides/sprites-and-text.mdx:25-32; concepts/resources.mdx:26-28; troublesh)

The 'Or keep it a PNG' aside cannot work with the v0.3.0 starter: that engine loads only name + '.dds'. A reader who copies hero.png instead of running texconv gets a startup failure naming hero.dds alone, and the page's failure aside is titled with the two-path master message and says the game looked for 'either a .dds or a .png'. The failure is inferred from source, not built. content-tools.mdx:10-14 repeats the claim (REV#17 marked Done made it so).

*Evidence.* READ your-picture.mdx:89-98 and :483-485 as quoted. READ in the ZIP: external/labrador/engine/render/resource_factory.cpp:74 'read_dds_file(directory + name + ".dds")'; no .png or lodepng in any engine .cpp/.h (hits only in tools, tests, samples and external/labrador/vcpkg.json). RAN curl: live your-picture page has 'keep it a PNG' 3 times, /versions/v0.3.0/ copy 0 times. The page itself (:568-570) says the note has 'not been followed in a starter'.

*Suggested.* Put a version note in the aside itself: 'Needs a Labrador newer than v0.3.0; check README.txt. With v0.3.0, use the texconv line.' Mirror it in content-tools. In the failure aside, show the one-path v0.3.0 message as well.

**`content-2`: Quoted "No file at ... .dds or ... .png." message is not what v0.3.0 prints** (confirmed, minor; your-picture.mdx:483-492 (failure box title) and troubleshooting.mdx:886-892)

Confirmed. On v0.3.0 the missing-texture message names one path ending .dds; the live page's failure box title and Troubleshooting's 'No file at ...' entry describe a two-path message ('...hero.dds or ...hero.png.') and say the game looks for either a .dds or a .png. The novice can still match the box by its first half, so it is a stumble, but its wording reinforces the false belief behind content-1. The v0.3.0 snapshot page has neither the box nor the two-path title.

*Evidence.* RAN (my copy): `startup failure: No file at C:\dev\nv-content-v\mygame\out\build\x64-debug\game\.\./textures/hero.dds.` (one path). First reviewer's png.log E2 shows the same shape for `heroo.dds`. READ your-picture.mdx:483-485 and troubleshooting.mdx:886-890 ('for a texture it names both the `.dds` and the `.png`'). READ master engine/render/resource_factory.cpp:97 builds the two-path message; v0.3.0's comes from byte_reader's single-path `No file at`. RAN curl on /versions/v0.3.0/docs/guides/your-picture/: HTTP 200, 0 occurrences of 'Or keep it a PNG' and of the two-path text.

*Suggested.* In the box body add: 'An engine from v0.3.0 or earlier names only the .dds path.' Same sentence in Troubleshooting's `No file at ...`.

**`gameplay-1`: Two players step 4 quotes starter code the v0.3.0 ZIP does not have; following it as worded stops the build, and the page's own entry misdiagnoses the error** (confirmed, major; website/src/content/docs/docs/guides/your-two-players.mdx:159-186 (step 4) and :385-391 (the C2065 entry); live https://labradorengine.com/docs/guides/your-two-players/)

Two players step 4 quotes master's starter code (ending `const Vector2F stick_direction = pad_input.stick;`) and says to replace 'down to and including the `stick_direction` line'. The v0.3.0 ZIP's hello_state.cpp has no such line: its first `stick_direction` line is the declaration above the for loop. Replacing as worded leaves the old loop, and the build stops with C2065 'stick_direction': undeclared identifier. The page's only entry for that message blames step 5, which is not the cause. Nothing on the page or its links at that step mentions a version. The quoted block visibly differs from the novice's file, which is a cue but not a route out. The v0.3.0 snapshot page words the step correctly for the ZIP.

*Evidence.* RAN in my own copy C:\dev\nv-gameplay-v\mygame (v0.3.0 ZIP, extracted with Windows tar; baseline built and passed 2/2): applied only step 4 literally (comment through `Vector2F stick_direction = Vector2F::zero;` replaced by the page's block), `cmake --build --preset x64-debug -- -k 0` gave `hello_state.cpp(133): error C2065: 'stick_direction': undeclared identifier` and `(162): error C2065: 'stick_direction': undeclared identifier`, build exit 2. (Line numbers differ from the reviewer's 164 because I did not do steps 1-3 and 5; line 162 is the step_motion call, 133 the leftover loop.) READ: your-two-players.mdx:159-186 and :385-391; the ZIP's hello_state.cpp:118-129. grep for 'v0.3|newer|your-version' in the page finds nothing. FETCHED the live /versions/v0.3.0/ page: HTTP 200, footer 'Documentation for Labrador v0.3.0. Built from Labrador 0f4f503'. Logs: <scratch>\gameplay-v\b2.log

*Suggested.* Until the next release, word step 4 so it works on both: 'from the comment down to the line before `if (quit_requested)`'. Add a second cause to the C2065 entry: the old for loop is still in the file. Longer term, apply the REV#62 fix (gameplay-3).

**`skew-4`: Two players step 4 shows a block to replace that the v0.3.0 hello_state.cpp does not contain** (confirmed, major; website/src/content/docs/docs/guides/your-two-players.mdx:159-170 (step 4) and :385 (C2065 aside))

Two players step 4 shows master's pad code and says to replace 'from the comment's first line down to and including the stick_direction line'. In v0.3.0 the block under that comment is different, and the first stick_direction line is the declaration above the for loop, so a literal follow leaves the old loop using a deleted variable; taking the last stick_direction line instead cuts a two-line statement in half. Either way the build breaks and the page's C2065 aside names the wrong cause (step 5). Which error and line number appear was not built.

*Evidence.* READ your-two-players.mdx:159-170 and :385-391. READ ZIP game/states/hello_state.cpp:118-129: comment, 'bool quit_requested', 'Vector2F stick_direction = Vector2F::zero;' (123), then the loop with 'stick_direction += apply_deadzone(...' over lines 127-128. RAN a script over every <SourceFile> excerpt of samples/minimal in the docs against the ZIP's game folder: the 'to' text 'const Vector2F stick_direction = pad_input.stick;' is absent (your-two-players.mdx:162 and the reference-style input.mdx:278, :288). Step 5's excerpt and the 'dark_gray));' search do exist in v0.3.0 (lines 155-157, 93). RAN curl: live page contains 'pad_input.stick'.

*Suggested.* Word the step so it holds for both versions: 'replace from the comment's first line down to the blank line above `if (quit_requested)`'. Say that a v0.3.0 starter's lines look different from the excerpt, and give both line numbers in the aside.

**`ide-x1`: Give your game two players, step 4, shows and tells the reader to replace code the v0.3.0 starter does not contain** (found by the verifier, major; website/src/content/docs/docs/guides/your-two-players.mdx:159-170; v0.3.0 starter game\states\hello_state.cpp:118-129)

The step says to search for 'A single-player game accepts', shows 'these lines' and says to replace them 'from the comment's first line down to and including the stick_direction line'. The box shows master's code: an array named readings, combine_pads, and a last line 'const Vector2F stick_direction = pad_input.stick;'. The novice's v0.3.0 file has none of those. Theirs declares 'Vector2F stick_direction = Vector2F::zero;' before the loop and adds to it inside the loop. The code on screen does not match the page, and the stopping point is ambiguous: stopping at the first stick_direction line leaves the old loop using a variable that no longer exists; stopping at the one inside the loop cuts a statement in half. Either gives compiler errors in a step-by-step guide that promised every line. A reader who guesses to replace through the loop's closing brace gets through. The page does not mention versions; the general advice in Which Labrador you have ('When a page is ahead of your engine') is written for a name that is missing, not for a passage that reads differently.

*Evidence.* Ran a script (scratchpad\ide-v\chk.py) checking every SourceFile from/to anchor for samples/minimal against the pristine v0.3.0 starter: 'NOMATCH docs\guides\your-two-players.mdx:162 samples/minimal/states/hello_state.cpp to=const Vector2F stick_direction = pad_input.stick;', also guides/input.mdx:278 (same anchor) and :288 ('PadsInput combine_pads(' in rules/motion.cpp), assets.mdx:47 and browser.mdx:647. Read your-two-players.mdx:159-170 and the starter's hello_state.cpp:118-129. Ran diff of master against v0.3.0: master lines 123-132 (readings, combine_pads, pad_input) replace v0.3.0's 123-129. Not built: I did not follow the guide to a compile, so the exact errors are inferred from the code.

*Suggested.* Have the website build, or a CI check, run each your-* guide's excerpt anchors against the newest released starter and fail or flag a mismatch. For this step now, word the replacement by its end points in both forms ('down to and including the closing brace of the for loop, and the stick_direction line after it if your file has one'), or cut v0.3.1 so the released starter matches the pages.

**`skew-3`: Your window's 'Any other size' uses ApplicationOptions::client_size, which v0.3.0 does not have** (corrected, major; website/src/content/docs/docs/guides/your-window.mdx:232-246)

your-window.mdx:232-246 has the reader paste options.client_size, which v0.3.0's ApplicationOptions lacks, so the build fails on a line copied exactly; the page has no version note and no build-failure entry for it. Correction: a recovery path exists two hops away. Troubleshooting's C2039 entry ends 'If the name is spelt as a page of this site spells it, the page may describe a newer engine than yours: Which Labrador you have checks', and your-window.mdx:665 links Read a build error. A novice who looks the code up can get there; one who rereads the page cannot. The compiler message was not produced by either reviewer.

*Evidence.* READ your-window.mdx:232-246. RAN grep in the ZIP: client_size appears in external/labrador/engine/app only in window.h (WindowOptions, lines 147, 287-310), not in application.h, and nowhere in game/. RAN curl: live page has client_size 4 times (reviewer said 3). READ troubleshooting.mdx:419-425. Would settle the exact message: build a v0.3.0 starter with the line pasted.

*Suggested.* Add 'Needs a Labrador newer than v0.3.0' at the passage, and a failure entry: `error C2039: 'client_size': is not a member of 'labrador::ApplicationOptions'` means your engine is older than this page, with a link to Which Labrador you have.

**`gameplay-2`: The 'Any other size' line `options.client_size` does not compile on the v0.3.0 starter, and nothing at the passage says it needs a newer engine** (corrected, minor; website/src/content/docs/docs/guides/your-window.mdx:232-246; live https://labradorengine.com/docs/guides/your-window/)

The `options.client_size` line does stop the v0.3.0 build with C2039, and the window page has no version note or entry for it. But 'they recover only by guessing' is overstated: the site's standard error route covers it. Read a build error's table links C2039 to Troubleshooting, whose entry ends 'If the name is spelt as a page of this site spells it, the page may describe a newer engine than yours: Which Labrador you have checks.' The route is three hops from the passage and the novice must think to take it, and the passage is an optional extra ('Any other size') after four working presets. That makes it a stumble with a documented recovery, not a major. A note at the passage is still the right fix.

*Evidence.* RAN: added `options.client_size = Vector2I(1600, 900);` under the resolution line in my copy's game\main.cpp; build gave `main.cpp(113): error C2039: 'client_size': is not a member of 'labrador::ApplicationOptions'`, exit 2. READ: your-window.mdx:232-246 (no version text; grep for 'v0.3|newer|your-version' in the page finds nothing); get-started/build-errors.mdx:257 (C2039 row linking troubleshooting); troubleshooting.mdx:411-425 (the newer-engine sentence); first-change.mdx:294 (LinkCard to Which Labrador you have).

*Suggested.* Put a one-line aside on the passage: 'Needs a Labrador newer than v0.3.0; with the v0.3.0 starter ZIP the build stops with C2039 'client_size': is not a member. Use one of the four presets, or move to a newer Labrador.' Add that C2039 line to the page's failure entries.

**`gameplay-4`: Add a rule and a test tells the novice to expect 6 test cases and 27 assertions; the v0.3.0 starter gives 5 and 19, and the 'file not listed' warning names 3 where it is 2** (confirmed, minor; website/src/content/docs/docs/guides/your-rule.mdx:362-392 (step 8), :431-432, :477, :689-692)

On the v0.3.0 starter the test program starts at 2 test cases and 9 assertions, not the page's 3 and 17, so after the page's steps the novice sees 5 and 19 where the page shows 6 and 27, the listed name 'the only pad counts in whichever slot it is in' is absent, and the 'If it says `test cases: 3`' warning corresponds to 2. The page calls the count 'the only sign' of an unlisted test file, so the mismatch sends a careful reader hunting. I ran the baseline only; 5 and 19 follow from the page's own 'your three' tests and 'your 10' checks and the reviewer's run.

*Evidence.* RAN on a clean build of the v0.3.0 ZIP: `out\build\x64-debug\game\MyGameTests.exe` printed `[doctest] test cases: 2 | 2 passed | 0 failed | 0 skipped` and `[doctest] assertions: 9 | 9 passed | 0 failed |`. READ: your-rule.mdx:362-392, :689-692, and its own note at :763-768 ('still passed with two test cases'; '2026-11-04 the starter gained a third test ... not seen in a run of these steps'). Log: <scratch>\gameplay-v\base.log

*Suggested.* Say it relatively and give both: 'three more test cases and 10 more assertions than before: 6 and 27 on a current starter, 5 and 19 on the v0.3.0 ZIP'. Reword the warning as 'if the count did not go up by three'.

**`skew-5`: Add a rule expects 6 test cases and 27 assertions; a v0.3.0 starter gives 5 and 19** (confirmed, minor; website/src/content/docs/docs/guides/your-rule.mdx:366-392, :431-432, :477, :689)

your-rule.mdx expects 6 test cases and 27 assertions and calls 3 the sign of a missing test file; a v0.3.0 starter has 2 tests of its own, so a correct result is 5 and a missing file shows 2. The assertion total for v0.3.0 was not run.

*Evidence.* READ your-rule.mdx:366-373, :689. RAN grep -c in the ZIP: game/tests/motion_tests.cpp has 2 TEST_CASE and 9 lines containing CHECK. The page's own record (:763-764) mentions 'two test cases' and 'five test cases' from the earlier check.

*Suggested.* State both: 'a v0.3.0 starter has two tests of its own, so 5 and 19; a missing file shows 2'. Or word it relatively: 'three more test cases than before you started'.

**`gameplay-5`: Rename your game step 4 says to search shell.html for `aria-label` and change 'Minimal sample'; the v0.3.0 shell.html has neither** (confirmed, minor; website/src/content/docs/docs/guides/your-name.mdx:111-117 (step 4))

Rename your game step 4 tells the novice to search shell.html for `aria-label` and change 'Minimal sample'; the v0.3.0 shell.html has no aria-label, and its canvas line is `<canvas id="canvas" tabindex="0"></canvas>`. The search finds nothing and the page gives no 'if it is not there'. Nothing breaks. The rest of the page holds on v0.3.0: the replacement count of 27 matches the ZIP.

*Evidence.* RAN: grep for 'aria-label|Minimal|canvas' in the pristine ZIP's game/web/shell.html: line 34 is the canvas without a label; 'Minimal' appears only in the comment at line 5. RAN: counted `MyGame` in the ZIP's CMakeLists.txt and game/CMakeLists.txt: 27 (28 including README.txt, which the command does not touch), matching your-name.mdx:50. READ: your-name.mdx:111-117 and :316-317.

*Suggested.* Add: 'If the search finds nothing, your starter is v0.3.0 and its canvas has no label; skip this, or add `aria-label="Sky Hopper"` inside the `<canvas` tag.'

**`skew-6`: Browser-page guides describe a shell.html the v0.3.0 starter does not have** (confirmed, minor; guides/your-name.mdx:111-117; guides/your-web-page.mdx:51-69 and :347; guides/browser.mdx:333, :619-656, :796; troubleshooting.mdx:763)

your-name.mdx:111-116 and your-web-page.mdx:60-66 describe aria-label and role="status" lines the v0.3.0 shell.html does not have, and the browser failure entries are titled 'The game could not be loaded.', a sentence the v0.3.0 page never shows. The reader recovers: the '<p id="status"' search still lands on the right line, and browser.mdx:796's title also says 'or stays at Loading...'. The your-name search for aria-label finds nothing, which is the one real stumble.

*Evidence.* RAN grep in the ZIP game/web/shell.html: line 22 title, line 35 '<p id="status">Loading...</p>'; no aria-label, role= or 'could not be loaded'. READ your-web-page.mdx:50-68, :347; troubleshooting.mdx:763-766; browser.mdx:796-800 (title includes 'or stays at Loading...', so that entry is partly matchable). Excerpt script: browser.mdx:647's shell.html excerpt '// A download that fails' is absent from the ZIP.

*Suggested.* Say 'if your shell.html has no aria-label, skip this' in your-name and your-web-page. Keep 'stays at Loading...' in the troubleshooting titles beside the new sentence.

**`skew-1`: Nothing on the starter route says the pages are newer than the ZIP, or points to the matching v0.3.0 pages** (confirmed, major; website/src/content/docs/docs/get-started/starter.mdx (whole page; step 2 via StarterDownload.astro); live https://labradorengine.com/docs/get-started/starter/; README.txt in labrador-starter-v0.3.0.z)

Nothing on the starter route (README.txt -> /docs/get-started/ -> Get the starter project) tells the reader that these pages describe master while the ZIP they are handed is v0.3.0, or that matching pages exist at /versions/v0.3.0/. The only signals are the collapsed sidebar box 'Documentation version Development' and the grey footer line. Already recorded as REV#62 (Open, owner decision); it still affects the novice today.

*Evidence.* RAN curl on the live starter page: sidebar is a collapsed <details> whose summary reads 'Documentation version Development' with one link 'All documentation versions'; step 2 reads 'The starter ZIP for Labrador v0.3.0 is labrador-starter-v0.3.0.zip'; no banner or sentence about the difference. READ: grep of get-started/starter.mdx for 'v0.3.0|version|Development' finds only vcpkg wording; ReleaseNote.astro 'On the development site this renders nothing'. READ ZIP README.txt: 'Labrador version: v0.3.0' and the link https://labradorengine.com/docs/get-started/. your-version is linked only from first-change.mdx:294, guides/index.mdx:143, troubleshooting.mdx:425, ask-for-help, move-pin and reference/index. docs/review/2026-11-10/STATUS.md:57 has REV#62 Open.

*Suggested.* Without reopening the default: have StarterDownload's download part add one sentence when the release has a published snapshot, such as 'These pages describe Labrador as it is being written; the pages that match this ZIP exactly are Documentation for v0.3.0', linking /versions/v0.3.0/docs/get-started/starter/. Add a one-line pointer to Which Labrador you have at the end of starter.mdx.

**`gameplay-3`: The download page hands out the v0.3.0 ZIP and then leads into Development pages; nothing tells a ZIP reader that matching v0.3.0 pages exist** (corrected, major; https://labradorengine.com/docs/get-started/starter/ (website/src/components/StarterDownload.astro) and every guide's footer)

The download page names the ZIP as v0.3.0 and the same site then teaches master with no note at the download or in the ZIP's README.txt that pages matching the ZIP exist. 'Never pointed to it' is slightly overstated: every live page carries a sidebar block 'Documentation version: Development / All documentation versions' linking /versions/, a footer 'Development documentation (unreleased). Built from Labrador 3067c67', the guide Which Labrador you have (linked from Make a first change), and Troubleshooting's C2039 entry. None of these says 'you took the v0.3.0 ZIP, so read the v0.3.0 pages', and a novice does not know 'Development' means 'not your ZIP'. This is REV#62 (Open, owner's decision), now with run evidence in pages other than the PNG one; it is not a new finding but its cost is understated in the record ('could').

*Evidence.* FETCHED live /docs/get-started/starter/: text contains 'Documentation version Development All documentation versions' (href="/versions/") and the footer line; the only v0.3.0 mentions are the ZIP's file name and release link. READ: StarterDownload.astro (no snapshot link); starter.mdx:14 'every later page applies to it in the same way'; ZIP README.txt ('Labrador version: v0.3.0' and the link https://labradorengine.com/docs/get-started/); guides/your-version.mdx:1-40; docs/review/2026-11-10/FINDINGS.md:431-434 (REV#62 lists the footer and your-version as mitigations 'not at the point of use'). RAN: gameplay-1 and gameplay-2 reproductions above.

*Suggested.* Without reopening the decision: beside the download button, add 'This ZIP is Labrador v0.3.0. The pages written for it are here: /versions/v0.3.0/docs/get-started/starter/. The pages you are reading describe a newer, unreleased engine.' Point the ZIP's README.txt at the versioned URL when the ZIP is made.

**`door-7`: The front door is the unreleased development documentation while the download is v0.3.0, and the novice is not told what that means** (corrected, minor; https://labradorengine.com/ footer; sidebar 'Documentation version Development'; https://labradorengine.com/versions/)

Confirmed that the site a novice reads is labelled 'Development documentation (unreleased). Built from Labrador 3067c67 of 2026-11-10' while the download is v0.3.0, and that no sentence on the Get Started index or starter page explains the difference; this is REV#62, open as the owner's decision. Overstated in two places: the starter page does say which version the download is ('The starter ZIP for Labrador v0.3.0 is labrador-starter-v0.3.0.zip'), and the path does point at the release copy, though not in plain words: every page has the sidebar 'Documentation version Development / All documentation versions' control and a footer 'All documentation versions' link to /versions/, and Make a first change ends with a 'Which Labrador you have' card. The version-notice banner on each page is empty unless a version switch failed, so it gives no explanation. Neither of us showed a place on the front-door path (home, index, starter page) where the development text actually differs from what the v0.3.0 ZIP does; that would settle whether this bites here or only in later guides.

*Evidence.* RAN: fetched live home, Get Started index, starter page, /versions/ and /versions/v0.3.0/docs/get-started/starter/ (all 200). Footer and sidebar strings as quoted; '<p class="version-notice" ... data-label="Development"></p>' is empty in the HTML and the Banner script fills it only from a 'version-missing' query parameter. READ docs/review/2026-11-10/FINDINGS.md:429-434 (REV#62, 'master is 85 commits past that tag'); first-change.mdx:294; starter-pristine README.txt ('Labrador version: v0.3.0', link to /docs/get-started/).

*Suggested.* Owner's decision per REV#62. The cheapest front-door step is one sentence on the Get Started index and starter page: 'The starter ZIP is Labrador v0.3.0. These pages follow the newest development version; if something here is not in your project, read the same page for v0.3.0 (link).'

**`skew-9`: 'Move to the engine the page describes' is a dead end today, and starter-update downloads 9 MB to find that out** (corrected, minor; guides/your-version.mdx 'When a page is ahead of your engine' (first bullet); guides/starter-update.mdx step 2)

your-version's first remedy for 'page ahead of your engine' is starter-update, which for a v0.3.0 reader ends at 'there is nothing newer ... stop here'. Correction: the page does name the tag before the download ('The starter ZIP for Labrador v0.3.0 is labrador-starter-v0.3.0.zip'), so an attentive reader can stop without downloading 9 MB; but the instruction order is download, then compare. The working remedy, the v0.3.0 pages, is the second bullet.

*Evidence.* RAN curl on live starter-update: step 2 text reads 'Download the newest starter ZIP into C:\dev. The starter ZIP for Labrador v0.3.0 is labrador-starter-v0.3.0.zip. This downloads it ...'. READ starter-update.mdx step 2 and your-version.mdx 'When a page is ahead of your engine' bullets.

*Suggested.* In starter-update step 2, say before the download 'The newest release with a starter ZIP is v0.3.0; if step 1 printed that, stop here' (StarterDownload already knows the tag). In your-version, put 'read the page as it was for your release' first, and say plainly that a name found on a Development page may be in no release yet.

### REV#63: No .vsconfig or one-command route for the Visual Studio prerequisite

<a id="rev63"></a>
The comparison with seven other first-run routes puts a number on the cost this row weighs.

**`peers-3`: The full Visual Studio IDE is the price of entry, and peers that need no compiler install set the reader's expectation** (confirmed, major; prerequisites.mdx:48-100; docs/review/2026-11-10/FINDINGS.md:436-443 (REV#63); TODO.md:205)

Visual Studio is the dominant first-run cost and the only large one; it is already recorded as REV#63 (open owner decision), so it is known, not new. Two corrections: (1) '1 min 41 s for the Labrador part' is a best case on the owner's fast desktop with a ~28 MB/s link and Git already installed; the page's own figure is 2 min 8 s plus a 60 MB private Git, and it says a slower machine 'can take much longer'. (2) The Build Tools variant is within REV#63's text only as an absence ('Build Tools appear only in tools/cloud_performance/README.md'); proposing it is new, and Microsoft's page gives it 2.3-60 GB against the IDE's 2.5-210 GB, so the saving is plausible but unmeasured. What would settle the real size: one Sandbox install of the single workload, as REV#63 already asks.

*Evidence.* READ prerequisites.mdx:48-74, :343-354 ('The installer was not run for these pages'). READ FINDINGS.md:436-443 and TODO.md:205 (REV#63). RAN curl on Microsoft's VS 2026 system requirements: 'Minimum of 2.5 GB up to 210 GB ... typical installations require 20-50 GB of free space'; Build Tools 'Requires 2.3 GB to 60 GB'. READ run2.log: curl 8.83M at 28.4M/s; 13:09:09.01 to 13:10:50.58; 'Configuring done (92.6s)'; no 'git was not found' line, so Git was present.

*Suggested.* Product options. (a) REV#63: one documented installer command with a configuration file, so nothing is ticked by hand. It uses Microsoft's installer and borders the 'no bundled installer' decision, so it is the owner's call. (b) The same with Build Tools in place of the IDE. It is smaller, but Edit and debug in Visual Studio then needs a second install, and the Build Tools licence terms for a hobbyist were not checked. Either way, measure the minimal selection once in Windows Sandbox and publish that figure instead of a five-workload 16 GB. Neither touches a PHILOSOPHY non-goal.

### REV#66: No prebuilt sample binaries

<a id="rev66"></a>
Nothing of the starter's own game can be tried before the install.

**`peers-5`: Nothing can be tried or changed before the multi-gigabyte install; peers offer a zero-install first look** (confirmed, minor; website/src/content/docs/index.mdx:62-68; release v0.3.0 assets; docs/review/2026-11-10/FINDINGS.md:526-531 (REV#66))

Nothing of the starter's own game can be seen or tried before the Visual Studio install; only LineSweeper is playable in a browser. Hosting the starter's own browser page as a preview is not recorded; REV#66 (prebuilt Windows binaries) is, as an open owner decision. A true edit-and-run playground is correctly marked as against the spirit of the permanent editor/scripting non-goals. This is a product idea, not a page defect.

*Evidence.* READ home index.mdx:62-68 (LineSweeper demo only); FINDINGS.md REV#66 ('Release v0.3.0 lists two assets'); PHILOSOPHY.md non-goals ('An editor. Permanent.', 'A scripting layer. Permanent.'). RAN ls: the v0.3.0 starter's game folder holds main_web.cpp and web. AGENTS.md states wasm-consumer builds the same project as a page and quits it in Chrome. Kaplay's playground claim not re-fetched by me.

*Suggested.* Cheapest product step: build the starter's own browser page in CI (wasm-consumer already builds and quits it in Chrome) and host it beside the docs as 'this is what you will have after step 2'. Optionally add a prebuilt MyGame Windows ZIP (REV#66), which costs SmartScreen friction and release work. A true edit-and-run browser playground would need a C++ compiler in the browser or on a server. It also sits against the spirit of PHILOSOPHY's permanent 'An editor' and 'A scripting layer' non-goals (PHILOSOPHY.md:882-886), and I do not recommend it.

## Not taken

**`ide-10`: Visual Studio selects wasm-debug on the starter's first open; the page carries the cost of a declined product fix.** Already weighed and declined: docs/beginner-guides.md records GUIDE#9's finding and GUIDE#10's refusal of a preset condition.

The verifiers refuted 0 findings outright. 33 notes graded as polish were not carried into rows:

- `door-3`: Release notes bury the novice's one sentence under migration and policy text
- `door-5`: Home 'Get started' card describes the Git route, not the three-step starter path
- `door-9`: Get Started index: the beginner reassurance sits below the Git route, and half the page is verification history
- `door-10`: The demo is on another site with no 'make your own' route back, and its home-page blurb is release-engineering text
- `tools-5`: Step 1's size paragraph buries the one number a novice needs
- `tools-7`: The quoted 'Git not found' message is not what Windows prints
- `tools-8`: A second Start folder named 'Visual Studio' holds only the wrong prompts
- `tools-x2`: Works well: step 4's check output and step 3's shortcut path match this machine exactly
- `starter-03`: game\README.md is the engine sample's README and describes a folder the novice does not have
- `starter-06`: Page lists lodepng among the libraries configure builds; the v0.3.0 starter does not install it
- `starter-07`: Second-project box makes a folder literally named %NAME% if the name line was not run in the same window
- `starter-08`: Steps 5 and 6 do not describe the normal output between the command and its last lines
- `starter-x01`: What works: the eight steps run exactly as the page says on the v0.3.0 ZIP, and every quoted output matches
- `change-1`: Code boxes are titled samples/minimal/... while the novice has just opened game\...
- `change-3`: The 'best next thing to read' is comments written for engine reviewers, not beginners
- `change-4`: Step 4 has the novice change a 'window class name' without saying what it is or that nothing visible changes
- `change-5`: Build-errors' missing-brace example reports line 67; removing only the brace character gives 68
- `change-x1`: What works: first-change's boxes match a real v0.3.0 build exactly
- `change-x2`: The GitHub link under each code box now shows a file that differs from the novice's
- `ide-8`: Alt+2, Alt+4 and F7 are not Visual Studio's General-profile keys per Microsoft, and the first-launch choice was never seen
- `ide-9`: Code captions on the debug page name the file the page says not to open
- `ide-11`: The page tells the reader its own check is still open, and a record says it is closed
- `trouble-4`: ctest before a successful build prints 'Not Run' and 30 lines of 'Looked in the following places', which no Get Started page describes
- `trouble-7`: Mistyped flags have no entry; one prints a 25-line usage dump
- `trouble-8`: The 'not recognized' entry does not name ctest, and Ask for help's findstr prints a line the page does not mention
- `trouble-x2`: At a middling folder depth the starter fails at configure with 'not able to compile a simple test program', filed under Building
- `content-6`: tones.json is 189 bytes in the released ZIP, not the 182 the sound guide says to check
- `content-7`: Whole-manifest replacement drops the second sound bank even after following the 'add it back' note
- `content-x1`: Step 1 says the picture 'must be a PNG', but the page's own texconv line converts a JPG as it is
- `skew-7`: Small mismatches: lodepng in the configure description, '(WaveBank)' in a failure title
- `skew-x1`: Scope check: only three guide excerpts of the template no longer match the v0.3.0 starter
- `peers-7`: The live starter page describes master's dependencies and timings, not the v0.3.0 ZIP it hands out
- `peers-x1`: The passing check on Install the tools shows a CMake that the very next page's configure calls unsuitable
