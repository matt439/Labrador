"""Add an opt-in same-library pad fixture after an assembled starter's baseline checks."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

CMAKE = """
# A consumer fixture; the distributed starter retains its own two tests.
add_executable(ScriptedPadConsumerTests
    external/labrador/tools/consumer_check/scripted_pad.cpp)
target_link_libraries(ScriptedPadConsumerTests PRIVATE LabradorEngine labrador_settings)
if(EMSCRIPTEN)
    target_link_options(ScriptedPadConsumerTests PRIVATE "-sENVIRONMENT=node")
endif()
add_test(NAME ScriptedPadConsumerTests COMMAND ScriptedPadConsumerTests)
set_tests_properties(ScriptedPadConsumerTests PROPERTIES TIMEOUT 60)
"""


def prepare(project: Path) -> None:
    """Check the assembled project and fixture before changing its CMake file."""
    root = project / "CMakeLists.txt"
    text = root.read_text(encoding="utf-8")
    if text.count("add_subdirectory(game)") != 1:
        raise ValueError("Expected one add_subdirectory(game); assemble a fresh starter first.")
    if "ScriptedPadConsumerTests" in text:
        raise ValueError("The scripted pad fixture is already present; use a fresh starter.")
    fixture = project / "external/labrador/tools/consumer_check/scripted_pad.cpp"
    if not fixture.is_file():
        raise ValueError(f"Missing consumer fixture: {fixture}")
    root.write_text(text + CMAKE, encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path, help="an assembled starter after its baseline checks")
    options = parser.parse_args()
    prepare(options.project.resolve())
    print("Added ScriptedPadConsumerTests; build and run it against the configured engine.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"scripted pad fixture: {error}", file=sys.stderr)
        sys.exit(2)
