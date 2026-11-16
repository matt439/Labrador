"""Apply the numbered Add a screen guide to an assembled starter project."""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from guide_examples import Guide


def prepare(project: Path, guide: Guide) -> None:
    """Use the guide's complete files and before/after excerpts, without optional edits."""
    # Imported on use because the runner owns Guide and dispatches this recipe.
    from guide_examples import apply_text

    header = guide.file(r"game\states\title_state.h")
    source = guide.file(r"game\states\title_state.cpp")
    new_source = "states/title_state.cpp"
    new_files = (("title_state.h", header), ("title_state.cpp", source))
    for name, _ in new_files:
        target = project / "game/states" / name
        if target.exists():
            raise ValueError(f"Add a screen: refusing to replace existing {target}")

    # The guide lists the source in both application branches. Removing the
    # newly listed line from its final block must recover its quoted old block.
    cmake_edits: list[tuple[str, str]] = []
    for entry_point in ("main_web.cpp", "main.cpp"):
        after = guide.snippet("cmake", entry_point, new_source)
        before = "".join(line for line in after.splitlines(keepends=True)
                         if line.strip() != new_source)
        before = guide.snippet("cmake", before)
        cmake_edits.append((before, after))

    old_include = '#include "game/states/hello_state.h"'
    new_include = source.splitlines()[0]
    if new_include != '#include "game/states/title_state.h"':
        raise ValueError("Add a screen: update the recipe for the title state's own include")
    native_before = guide.excerpt("std::unique_ptr<State> first_state").splitlines()[0].strip()
    native_after = guide.snippet("cpp", "std::unique_ptr<State> first_state =").strip()
    web_before = guide.excerpt("return app.run(std::make_unique<HelloState>(&app));").strip()
    web_after = guide.snippet("cpp", "return app.run(std::make_unique<TitleState>(&app));").strip()

    for before, after in cmake_edits:
        apply_text(project / "game/CMakeLists.txt", before, after)
    for name, before, after in (("main.cpp", native_before, native_after),
                                ("main_web.cpp", web_before, web_after)):
        entry = project / "game" / name
        apply_text(entry, old_include, new_include)
        apply_text(entry, before, after)

    for name, contents in new_files:
        target = project / "game/states" / name
        target.write_text(contents, encoding="utf-8", newline="\n")
