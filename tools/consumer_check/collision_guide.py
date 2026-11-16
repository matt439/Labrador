"""Apply the collision guide's numbered steps to an assembled starter copy."""

from __future__ import annotations

from pathlib import Path
import textwrap
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from guide_examples import Guide


def _replace(source: str, before: str, after: str, path: Path) -> str:
    """Match an excerpt at its source indentation, retaining relative depth."""
    before = textwrap.dedent(before).strip("\n")
    after = textwrap.dedent(after).strip("\n")
    first = before.splitlines()[0]
    matches = []
    for line in source.splitlines():
        if line.lstrip() != first:
            continue
        prefix = line[:len(line) - len(line.lstrip())]
        old = textwrap.indent(before, prefix)
        if source.count(old) == 1:
            matches.append((old, textwrap.indent(after, prefix)))
    if len(matches) != 1:
        raise ValueError(f"{path}: expected one collision-guide anchor: {first}")
    old, new = matches[0]
    return source.replace(old, new, 1)


def prepare(project: Path, guide: Guide) -> None:
    """Build the guide's wall-and-player example, excluding optional variants."""
    from guide_examples import apply_text

    def plain(value: str) -> str:
        return textwrap.dedent(value).strip("\n")

    new_files = {
        project / "game/states/block.h": guide.file(r"game\states\block.h"),
        project / "game/states/block.cpp": guide.file(r"game\states\block.cpp"),
    }
    for path in new_files:
        if path.exists():
            raise ValueError(f"{path}: collision guide requires a fresh starter")

    paths = {
        "cmake": project / "game/CMakeLists.txt",
        "header": project / "game/states/hello_state.h",
        "source": project / "game/states/hello_state.cpp",
    }
    original = {name: path.read_text(encoding="utf-8") for name, path in paths.items()}
    changed = original.copy()

    def edit(name: str, before: str, after: str) -> None:
        changed[name] = _replace(changed[name], before, after, paths[name])

    # Both enumerated source lists come from the guide's completed CMake blocks.
    for entry in ("main_web.cpp", "main.cpp"):
        after = guide.snippet("cmake", entry, "states/block.cpp")
        before = "\n".join(line for line in after.splitlines()
                           if line.strip() != "states/block.cpp")
        edit("cmake", before, after)

    includes = guide.snippet("cpp", '#include "game/rules/motion.h"',
                             '#include "game/states/block.h"')
    edit("header", includes.splitlines()[0], includes)
    member = guide.snippet("cpp", "labrador::Label* hint_ = nullptr;", "Block* player_ = nullptr;")
    edit("header", plain(member).splitlines()[0], member)

    includes = guide.snippet("cpp", '#include "engine/math/vector2f.h"',
                             '#include "engine/render/resource_factory.h"')
    edit("source", includes.splitlines()[0], includes)

    hint = guide.excerpt("this->hint_ = this->scene_->add(")
    blocks = guide.snippet("cpp", "const TextureHandle white =", "this->player_ =")
    edit("source", hint, plain(blocks) + "\n\n" + plain(hint))

    greeting = guide.excerpt("this->greeting_->set_position(greeting_position(this->motion_));")
    velocity = guide.snippet("cpp", "this->player_->set_velocity(combined_direction(stick_direction,")
    edit("source", greeting, plain(greeting) + "\n" + plain(velocity))

    update = guide.excerpt("this->scene_->update(dt);")
    resolve = guide.snippet("cpp", "this->scene_->update(dt);", "this->scene_->resolve();",
                            "this->scene_->end_tick();")
    edit("source", update, resolve)

    # Every guide selector and starter anchor is checked before any file changes.
    for name, path in paths.items():
        apply_text(path, original[name], changed[name], expected=1)
    for path, contents in new_files.items():
        path.write_text(contents, encoding="utf-8", newline="\n")
