# Night Dispatch starter migration, 20 November 2026

Night Dispatch is a separate, customized consumer of the published Windows and browser starter. A courier collects a parcel at the depot, takes it through the gate and reaches a recipient in the arcade. The game has its own art, movement and collision rules, HUD, tests, CMake targets and browser input script. It exercises a source break in a game call and a dependency merge in the same move.

## Release inputs

| Starter | Engine commit in the ZIP | GitHub release asset SHA-256 |
|---|---|---|
| [v0.3.0](https://github.com/matt439/Labrador/releases/tag/v0.3.0) | `0f4f5033b317a334d89e3cf14ab98684e0598ddd` | `25db98b280e50648831ca896dad5bbd7b51788a8051dd29089ce3faa410f5869` |
| [v0.4.0](https://github.com/matt439/Labrador/releases/tag/v0.4.0) | `f525bc6cac7029e3e4575308620a9b6126f95af9` | `bc5367c26bec9035467000093922067919e4cfde84e8cabc0be576148b12e2ae` |

The older checkout is `C:\Projects\LabradorCourier-v030`; the upgraded game is `C:\Projects\LabradorCourier`. Both are local Git projects. The engine repository holds this record. The release downloads contain the engine and the starter, while the customized game belongs to its own local history.

## What to carry across

Use [the starter update guide](../../website/src/content/docs/docs/guides/starter-update.mdx) for the folder inventory and swap. The operations that matter for this game are:

- Replace the new starter's example game with the old project's game. Preserve its hidden Git history, top-level game instructions and `tools/` alongside the new `external/labrador`.
- Retain the project's `NightDispatch` CMake name and game targets. Keep the game's rules, state, test, texture, browser relink dependencies and install notices. Port the new starter's doctest lookup and optional Windows icon block to that target, and copy the block's companion `game_icon.rc` from the new starter.
- Keep the v0.4.0 manifest's `lodepng` plus the game's `fmt` and package name. The HUD uses fmt for its clock. Leaving a built package in an old build directory would hide a missing dependency.
- Change `runner.AABB_intersects(cart_bounds())` to `runner.bounding_boxes_intersect(cart_bounds())`. Both arguments represent rectangles. A parked sorting cart blocks movement in the depot's lower aisle. The release's positional application-options change needs no edit here because both entry points already assign options by name.

The first inventory compared top-level names only. Both releases of the *game project* had a `tools/` directory, for different reasons. The first browser replay stopped with `ERR_MODULE_NOT_FOUND` for `tools/play_route.mjs`, after both CTest presets had passed. Listing relative paths inside both directories restored the game-owned `play_route.mjs` and `make_dispatch.py` beside the release's `starter_commands.ps1`. The six starter comparisons found release changes in `vcpkg.json` and the template game CMake file. The published v0.4.0 ZIP also supplies `build.cmd`, `play.cmd`, `test.cmd` and `labrador.vsconfig`.

## What ran

The v0.3.0 starter import is `0c8c5d2`, the customized game is `0247b32`, and the migration is its direct child `1931264`. The upgraded project's vendored engine compared equal to the fresh v0.4.0 extraction. Its game atlas, manifest, state, rules tests and route runner stayed identical to their v0.3.0 Git blobs. Before the collision edit, MSVC stopped at `route.cpp(45): error C2039: 'AABB_intersects': is not a member of 'labrador::RectangleF'`.

On Windows 11 Pro with MSVC, Emscripten 6.0.12 and a GeForce RTX 5080, both versions passed:

| Check | Observed result |
|---|---|
| Native Debug CTest | 2/2 including the hidden application's startup smoke. The rules suite passed 17 assertions in three cases, including a blocked cart, a locked gate, held-key cooldown, delivery and restart. |
| Browser Debug CTest | 1/1 under Emscripten's Node. |
| Native game window | A process-targeted Win32 message script sent WASD, crossed into the arcade, returned to the depot, crossed again, delivered, reset with R and quit with Escape. The game clock showed 4.1 s before and 4.0 s after the move. The upgraded executable reported the RTX 5080. |
| Browser game | The project's own `tools/play_route.mjs` used Chrome 157.0.8103.80 through DevTools. Both runs reported pickup, room 2, delivery at 1.9 s, restart and quit. WebGL2 reported ANGLE on the RTX 5080 via D3D11. |
| Browser delivery screen | The two 764 × 485 PNGs were byte-identical on this run, SHA-256 `b58b178a87c49e106a211ef427d6070fa3177ab919d10aa91ec15aa7aa33368b`. The clock on this screen depends on input scheduling. |

For replay, follow the two build blocks and the Chrome command in the game's `README.md` from each checkout. The local rehearsal directory `C:\Projects\CourierMigration-ef8d2d71` holds both screenshots, the downloaded release, the build logs and `native-route.py`, which takes the native executable's path. The migration used existing compiler/tool installations and vcpkg's binary cache with separate native and wasm install directories. Its final configurations ran manifest installation with both fmt and lodepng declared.

## Playable Release builds

The migrated consumer at `1931264` also produced native and optimized WebAssembly Release packages. From the upgraded project, `out/play/windows/NightDispatch.exe` starts the installed game. `out/play/PLAY.txt` gives the loopback server command for `out/play/browser/index.html`. The native Release tests passed 2/2 and the installed smoke ran from outside the build tree. The WebAssembly rules passed under Node. Both packaged games completed delivery, restart and quit with keyboard input. The Windows check used the development machine with the packaged runtime DLLs. A clean-machine install remains unverified.

## Going back

A separate clone of the customized v0.3.0 game was built and passed its native tests at its working path. In that directory's parent, the old folder was renamed aside, a clone of the migration was renamed into its place, and Git reported `1931264`. Renaming the upgrade aside and the original back restored `0247b32` under its previous path. Configure/build reused the original objects, both native tests passed, and a new native playthrough delivered in 4.1 s, restarted and quit. The main upgraded checkout stayed available for play.

## Independent replay of the guide

A second reader started with the customized v0.3.0 game and the published v0.4.0 ZIP. The revised file comparison found the art generator and the browser playthrough script inside `tools/`. A separate fixture exercised game files inside both `tools/` and `.github/`, including edits at paths the starter also supplied. The rendered extraction command ran in an ordinary Windows command prompt. It extracted the release into an empty destination and preserved an edited README when repeated.

The reader reproduced MSVC's missing collision method before changing the call. The first build also exposed a weakness in the rehearsal itself. Reusing installed packages let a copy of the old manifest build while it still omitted `lodepng`. After merging the manifests, each preset installed the declared packages in a separate empty package directory. A fresh configure resolved both `fmt_DIR` and `lodepng_DIR` inside that preset's directory. The game built and passed 2/2 native tests and 1/1 wasm test. Chrome completed the delivery, restart and quit route.

The second reader compared all 1,600 vendored engine files against the new release archive and restored the clean v0.3.0 game at its original path. Both native tests, the wasm test and the Chrome route passed again after rollback. The two delivery screenshots matched the digest above. The local logs and images are under `out/courier-review/`, including `clean-x64-debug.log`, `clean-wasm-debug.log`, `clean-play.log` and `rollback-final.log`.

## Optional Windows icon

A follow-up game commit (`d0c4c72`) passes `L"GAME_ICON"` to `ApplicationOptions::window_icon_resource` when CMake defines `GAME_HAS_ICON_RESOURCE`. A separate x64 Debug build selected the release's `tests/app/window_icon_fixture.ico` with `GAME_ICON_FILE` and passed 2/2 tests. During a full delivery, `GetClassLongPtrW(hwnd, GCLP_HICON)` found an icon on the configured game window and no icon on the default game window. Both runs restarted and quit. The distributed default uses the build without an icon.
