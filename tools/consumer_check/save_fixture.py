"""Copy the opt-in game-owned save/recovery example into an assembled starter."""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

FILES = ("recovery_settings.h", "recovery_settings.cpp", "recovery_io.h", "recovery_io.cpp",
         "recovery_tests.cpp", "recovery_main.cpp", "recovery_persist.js", "recovery_shell.html")
CMAKE = """
# Optional save/recovery example; the ordinary game remains unchanged.
add_library(MyGameSaveExample STATIC
    game/save_example/recovery_settings.cpp
    game/save_example/recovery_io.cpp)
target_link_libraries(MyGameSaveExample PUBLIC LabradorEngine labrador_settings)
target_include_directories(MyGameSaveExample PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(MyGameSaveTests game/save_example/recovery_tests.cpp)
target_link_libraries(MyGameSaveTests PRIVATE MyGameSaveExample)
if(EMSCRIPTEN)
    target_link_options(MyGameSaveTests PRIVATE -sENVIRONMENT=node -sNODERAWFS=1)
    add_executable(MyGameSavePage game/save_example/recovery_main.cpp)
    target_link_libraries(MyGameSavePage PRIVATE MyGameSaveExample)
    set_target_properties(MyGameSavePage PROPERTIES SUFFIX ".html")
    target_link_options(MyGameSavePage PRIVATE
        -sENVIRONMENT=web -sNODERAWFS=0 -sEXIT_RUNTIME=0 -lidbfs.js
        "-sEXPORTED_RUNTIME_METHODS=['FS']"
        "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/game/save_example/recovery_persist.js"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/game/save_example/recovery_shell.html")
    set_property(TARGET MyGameSavePage APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/game/save_example/recovery_persist.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/save_example/recovery_shell.html")
endif()
add_test(NAME MyGameSaveTests COMMAND MyGameSaveTests)
set_tests_properties(MyGameSaveTests PROPERTIES TIMEOUT 60)
"""


def prepare(project: Path) -> None:
    root = project / "CMakeLists.txt"
    original = root.read_text(encoding="utf-8")
    if original.count("add_subdirectory(game)") != 1:
        raise ValueError("Expected one add_subdirectory(game); assemble a fresh starter first.")
    if "MyGameSaveExample" in original:
        raise ValueError("Save fixture is already present; use a fresh starter.")
    destination = project / "game/save_example"
    if destination.exists():
        raise ValueError(f"Refusing to overwrite game-owned files at {destination}")
    source = project / "external/labrador/tools/save_check"
    contents = {}
    for name in FILES:
        data = (source / name).read_bytes()
        if name.endswith((".cpp", ".h")):
            data = data.replace(b'#include "tools/save_check/', b'#include "game/save_example/')
        contents[name] = data
    destination.mkdir()
    for name, data in contents.items():
        (destination / name).write_bytes(data)
    root.write_text(original + CMAKE, encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    options = parser.parse_args()
    prepare(options.project.resolve())
    print("Added game/save_example and MyGameSaveTests; no user save was opened.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"save fixture: {error}", file=sys.stderr)
        sys.exit(2)
