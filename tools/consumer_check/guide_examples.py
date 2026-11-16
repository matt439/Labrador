"""Read the three-guide pilot's source from the guides, refusing ambiguous recipes."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


GUIDE_DIRECTORY = Path("website/src/content/docs/docs/guides")


@dataclass(frozen=True)
class Fence:
    language: str
    title: str | None
    code: str


def fenced_blocks(text: str) -> list[Fence]:
    """Strip the Markdown fence's indentation, retaining the code's own indent."""
    result = []
    opening = None
    body: list[str] = []
    for line in text.splitlines():
        if opening is None:
            match = re.fullmatch(r'(\s*)```(\w+)(?:\s+title="([^"]*)")?\s*', line)
            if match:
                opening = match
                body = []
        elif line.strip() == "```":
            prefix, language, title = opening.groups()
            if any(line and not line.startswith(prefix) for line in body):
                raise ValueError("guide code block escapes its fence indentation")
            result.append(Fence(language, title, "\n".join(
                line[len(prefix):] if line else "" for line in body) + "\n"))
            opening = None
        else:
            body.append(line)
    if opening is not None:
        raise ValueError("guide has an unterminated code fence")
    return result


class Guide:
    def __init__(self, name: str, engine: Path):
        self.engine = engine
        self.path = GUIDE_DIRECTORY / f"{name}.mdx"
        self.text = (engine / self.path).read_text(encoding="utf-8")
        routes = re.findall(r"<Steps>(.*?)</Steps>", self.text, re.S)
        if len(routes) != 1:
            raise ValueError(f"{self.path}: expected one numbered main procedure, found {len(routes)}")
        self.recipe_text = routes[0]
        self.blocks = fenced_blocks(self.recipe_text)
        self.selections: list[str] = []

    def only(self, matches: list[str], what: str) -> str:
        if len(matches) != 1:
            raise ValueError(f"{self.path}: expected one {what}, found {len(matches)}; "
                             "update the compiled-guide recipe with the guide")
        self.selections.append(what)
        return matches[0]

    def file(self, title: str) -> str:
        return self.only([block.code for block in self.blocks if block.title == title],
                         f"file block titled {title!r}")

    def snippet(self, language: str, *contains: str) -> str:
        return self.only([block.code for block in self.blocks
                          if block.language == language and block.title is None
                          and all(text in block.code for text in contains)],
                         f"{language} block containing {contains!r}")

    def excerpt(self, from_text: str) -> str:
        attributes = [dict(re.findall(r'(\w+)="([^"]*)"', match.group(1)))
                      for match in re.finditer(r"<SourceFile\s+(.*?)/>", self.recipe_text, re.S)]
        matches = [value for value in attributes if value.get("from") == from_text]
        self.only([value["path"] for value in matches], f"SourceFile from {from_text!r}")
        selected = matches[0]
        relative = Path(selected["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"{self.path}: SourceFile must stay inside the engine tree")
        lines = (self.engine / relative).read_text(encoding="utf-8").splitlines()
        starts = [index for index, line in enumerate(lines) if line.strip().startswith(from_text)]
        self.only([str(index) for index in starts], f"source line {from_text!r}")
        start = starts[0]
        end_text = selected["to"]
        depth = len(lines[start]) - len(lines[start].lstrip())
        ends = [index for index in range(start, len(lines))
                if lines[index].strip().startswith(end_text)
                and (not end_text.startswith("}")
                     or len(lines[index]) - len(lines[index].lstrip()) <= depth)]
        if not ends:
            raise ValueError(f"{self.path}: missing SourceFile end {end_text!r}")
        return "\n".join(lines[start:ends[0] + 1]) + "\n"


def apply_text(path: Path, before: str, after: str, expected: int = 1) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(before)
    if count != expected:
        raise ValueError(f"{path}: expected {expected} copies of recipe anchor {before!r}, found {count}")
    path.write_text(text.replace(before, after), encoding="utf-8", newline="\n")


def prepare_rule(project: Path, guide: Guide) -> None:
    for title in (r"game\rules\bounds.h", r"game\rules\bounds.cpp", r"game\tests\bounds_tests.cpp"):
        path = project / title.replace("\\", "/")
        path.write_text(guide.file(title), encoding="utf-8", newline="\n")
    cmake = project / "game/CMakeLists.txt"
    for target, added in (("add_library(MyGameRules STATIC", "rules/bounds.cpp"),
                          ("add_executable(MyGameTests", "tests/bounds_tests.cpp")):
        after = guide.snippet("cmake", target, added)
        before = "\n".join(line for line in after.splitlines() if added not in line) + "\n"
        # Both the before and after listing are part of the guide's procedure.
        if guide.snippet("cmake", before.strip()) != before:
            raise ValueError(f"{guide.path}: rule build recipe changed")
        apply_text(cmake, before, after)
    source = project / "game/states/hello_state.cpp"
    includes = guide.snippet("cpp", '#include "game/rules/bounds.h"')
    apply_text(source, includes.splitlines()[-1] + "\n", includes)
    motion = guide.excerpt("this->motion_ = step_motion(this->motion_,")
    addition = guide.snippet("cpp", "// Keep the greeting inside the window")
    apply_text(source, motion, motion + "\n" + addition)
