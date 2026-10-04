# The Get Started project files

The files a reader creates in *Start your own project*
(`src/content/docs/docs/get-started/your-project.mdx`), kept as files rather
than typed into the page so that the page shows exactly what was built.

Every one of them was used, unchanged, to build a project from the public
repository with the x64 Native Tools Command Prompt: Labrador added as a
submodule at `external/labrador`, `samples/minimal` copied to `game/`, the
four includes the page lists edited, and these files put in place. The
`vcpkg.json` is not here because the page derives it from Labrador's own
(`src/components/ConsumerManifest.astro`).

| File | Goes to |
| --- | --- |
| `root-CMakeLists.txt` | `CMakeLists.txt` |
| `game-CMakeLists.txt` | `game/CMakeLists.txt` |
| `CMakePresets.json` | `CMakePresets.json` |
| `gitignore.txt` | `.gitignore` |

Change one, and build the walkthrough again before publishing the change.
