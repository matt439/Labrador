"""Generate Labrador's existing unrotated sprite-sheet JSON from a uniform grid.

Dimensions are explicit; no image is opened or changed. Frame and strip flags
retain their input order. Coordinates are zero-based grid columns and rows;
strips advance horizontally and never wrap to the next row.
"""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import sys
import tempfile


INT_MAX = 2_147_483_647
MAX_STRIP_FRAMES = 16_384
FLOAT_MAX = float.fromhex("0x1.fffffep+127")


class DefinitionError(ValueError):
    """An authoring input that cannot produce the requested grid definition."""


def integer(value: str, field: str, minimum: int = 0, maximum: int = INT_MAX) -> int:
    try:
        result = int(value, 10)
    except ValueError:
        raise DefinitionError(f"{field} must be a whole number, got {value!r}") from None
    if not minimum <= result <= maximum:
        raise DefinitionError(f"{field} must be between {minimum} and {maximum}, got {result}")
    return result


def name(value: str, table: str, names: set[str]) -> str:
    if not value or "\0" in value:
        raise DefinitionError(f"{table} name must be nonempty and contain no NUL")
    try:
        value.encode("utf-8")
    except UnicodeEncodeError:
        raise DefinitionError(f"{table} name must be valid UTF-8") from None
    if value in names:
        raise DefinitionError(f"duplicate {table} name {value!r}")
    names.add(value)
    return value


def frame_time(value: str, context: str) -> float:
    try:
        result = float(value)
    except ValueError:
        raise DefinitionError(f"{context} frame_time must be a finite float, got {value!r}") from None
    if not math.isfinite(result) or abs(result) > FLOAT_MAX:
        raise DefinitionError(f"{context} frame_time must fit a finite float, got {value!r}")
    return result


def coordinate(cell: str, extent: int, field: str) -> int:
    result = integer(cell, field) * extent
    if result > INT_MAX:
        raise DefinitionError(f"{field} produces pixel coordinate {result}, outside the signed int range")
    return result


def rectangle_in_atlas(x: int, y: int, width: int, height: int,
                       image_width: int, image_height: int, context: str) -> None:
    if x + width > image_width or y + height > image_height:
        raise DefinitionError(
            f"{context} samples rectangle ({x}, {y}, {width}, {height}) "
            f"outside the {image_width} by {image_height} image")


def build_sheet(image: list[str], grid: list[str], frames: list[list[str]],
                strips: list[list[str]]) -> dict:
    image_width = integer(image[0], "image width", 1)
    image_height = integer(image[1], "image height", 1)
    columns = integer(grid[0], "grid columns", 1)
    rows = integer(grid[1], "grid rows", 1)
    if columns > image_width or rows > image_height:
        raise DefinitionError("grid columns and rows cannot exceed image dimensions; cells must have positive area")
    if image_width % columns or image_height % rows:
        raise DefinitionError("image dimensions must divide evenly into the grid columns and rows")
    width = image_width // columns
    height = image_height // rows

    output_frames = []
    output_strips = []
    frame_names: set[str] = set()
    strip_names: set[str] = set()
    for supplied_name, column, row in frames:
        frame_name = name(supplied_name, "frame", frame_names)
        context = f"frame {frame_name!r}"
        x = coordinate(column, width, f"{context} column")
        y = coordinate(row, height, f"{context} row")
        rectangle_in_atlas(x, y, width, height, image_width, image_height, context)
        output_frames.append({"name": frame_name, "position": {"x": x, "y": y},
                              "size": {"w": width, "h": height}})

    for supplied_name, column, row, count, seconds, looping in strips:
        strip_name = name(supplied_name, "strip", strip_names)
        context = f"strip {strip_name!r}"
        frame_count = integer(count, f"{context} frame_count", maximum=MAX_STRIP_FRAMES)
        x = coordinate(column, width, f"{context} column")
        y = coordinate(row, height, f"{context} row")
        duration = frame_time(seconds, context)
        if looping not in ("true", "false"):
            raise DefinitionError(f"{context} looping must be 'true' or 'false', got {looping!r}")
        if frame_count:
            # Endpoint checks use Python's unbounded integers, before any
            # output is written. A horizontal strip cannot wrap a grid row.
            last_x = x + (frame_count - 1) * width
            if last_x > INT_MAX:
                raise DefinitionError(f"{context} last frame x {last_x} is outside the signed int range")
            rectangle_in_atlas(x, y, width, height, image_width, image_height,
                               f"{context} first frame")
            rectangle_in_atlas(last_x, y, width, height, image_width, image_height,
                               f"{context} last frame")
        output_strips.append({"name": strip_name,
                              "first_frame": {"x": x, "y": y, "w": width, "h": height},
                              "frame_count": frame_count, "frame_time": duration,
                              "looping": looping == "true"})
    return {"sprite_frames": output_frames, "animation_strips": output_strips}


def encoded_sheet(sheet: dict) -> bytes:
    return (json.dumps(sheet, ensure_ascii=False, allow_nan=False, indent=4) + "\n").encode("utf-8")


def write_sheet(output: Path, contents: bytes) -> None:
    # Replace only after validation and a complete write in the destination
    # folder; a refusal leaves an existing authored file untouched.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=f".{output.name}.",
                                         suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(contents)
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", nargs=2, required=True, metavar=("WIDTH", "HEIGHT"))
    parser.add_argument("--grid", nargs=2, required=True, metavar=("COLUMNS", "ROWS"))
    parser.add_argument("--frame", nargs=3, action="append", default=[],
                        metavar=("NAME", "COLUMN", "ROW"))
    parser.add_argument("--strip", nargs=6, action="append", default=[],
                        metavar=("NAME", "COLUMN", "ROW", "COUNT", "SECONDS", "LOOPING"))
    parser.add_argument("--output", required=True, type=Path)
    options = parser.parse_args(arguments)
    try:
        contents = encoded_sheet(build_sheet(options.image, options.grid,
                                             options.frame, options.strip))
    except DefinitionError as error:
        parser.error(str(error))
    try:
        write_sheet(options.output, contents)
    except OSError as error:
        print(f"grid_sheet: could not write '{options.output}': {error}", file=sys.stderr)
        return 1
    frame_label = "frame" if len(options.frame) == 1 else "frames"
    strip_label = "strip" if len(options.strip) == 1 else "strips"
    print(f"grid_sheet: wrote {options.output} "
          f"({len(options.frame)} {frame_label}, {len(options.strip)} {strip_label})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
