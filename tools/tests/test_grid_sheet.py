"""Grid authoring uses explicit metadata only; no image decoder or renderer."""

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.tests.tool_fixtures import load_tool


grid_sheet = load_tool("tools/grid_sheet.py")
ROOT = Path(__file__).resolve().parents[2]
COMMAND = ROOT / "tools/grid_sheet.py"
FIXTURE = ROOT / "tests/assets/grid_sheet/grid.json"
ANIMATION = ROOT / "samples/components/content/textures/animation_preview.json"

GRID_FRAMES = [["top_left", "0", "0"], ["bottom_right", "3", "1"]]
GRID_STRIPS = [["row_once", "0", "1", "4", "0.125", "false"],
               ["row_loop", "1", "0", "3", "0.25", "true"],
               ["empty", "1000", "1000", "0", "0.5", "false"]]


def sheet(*, image=("64", "32"), grid=("4", "2"), frames=(), strips=()):
    return grid_sheet.build_sheet(list(image), list(grid), list(frames), list(strips))


class GridSheetTests(unittest.TestCase):
    def test_checked_in_fixture_is_actual_deterministic_command_output(self):
        definition = sheet(frames=GRID_FRAMES, strips=GRID_STRIPS)
        self.assertEqual(grid_sheet.encoded_sheet(definition), FIXTURE.read_text(encoding="utf-8").encode("utf-8"))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "grid.json"
            arguments = [sys.executable, str(COMMAND), "--image", "64", "32", "--grid", "4", "2"]
            for frame in GRID_FRAMES:
                arguments.extend(["--frame", *frame])
            for strip in GRID_STRIPS:
                arguments.extend(["--strip", *strip])
            subprocess.run([*arguments, "--output", str(output)], check=True, capture_output=True)
            self.assertEqual(output.read_bytes(), FIXTURE.read_text(encoding="utf-8").encode("utf-8"))
        self.assertEqual(definition["sprite_frames"][1], {
            "name": "bottom_right", "position": {"x": 48, "y": 16}, "size": {"w": 16, "h": 16}})
        self.assertEqual(definition["animation_strips"][1]["first_frame"], {"x": 16, "y": 0, "w": 16, "h": 16})
        self.assertFalse(definition["animation_strips"][0]["looping"])

    def test_reproduces_the_delivered_animation_example_without_changing_it(self):
        definition = sheet(image=("64", "16"), grid=("4", "1"), strips=[
            ["pulse_loop", "0", "0", "4", "0.25", "true"],
            ["pulse_once", "0", "0", "4", "0.25", "false"]])
        self.assertEqual(grid_sheet.encoded_sheet(definition),
                         ANIMATION.read_text(encoding="utf-8").encode("utf-8"))

    def test_separate_tables_keep_their_own_explicit_input_order(self):
        definition = sheet(frames=[["second", "2", "0"], ["first", "0", "0"]], strips=[
            ["second", "0", "0", "1", "0.2", "false"],
            ["first", "1", "0", "1", "0.1", "true"]])
        self.assertEqual([frame["name"] for frame in definition["sprite_frames"]], ["second", "first"])
        self.assertEqual([strip["name"] for strip in definition["animation_strips"]], ["second", "first"])

    def test_duplicate_names_are_refused_within_each_table(self):
        with self.assertRaisesRegex(grid_sheet.DefinitionError, "duplicate frame name 'same'"):
            sheet(frames=[["same", "0", "0"], ["same", "1", "0"]])
        with self.assertRaisesRegex(grid_sheet.DefinitionError, "duplicate strip name 'same'"):
            sheet(strips=[["same", "0", "0", "1", "1", "true"]] * 2)

    def test_names_are_nonempty_utf8_without_nul(self):
        for invalid in ("", "bad\0name", "bad\ud800name"):
            with self.subTest(name=repr(invalid)), self.assertRaisesRegex(grid_sheet.DefinitionError, "frame name"):
                sheet(frames=[[invalid, "0", "0"]])
        definition = sheet(frames=[["caf\u00e9", "0", "0"]])
        self.assertIn("caf\u00e9".encode("utf-8"), grid_sheet.encoded_sheet(definition))

    def test_image_and_grid_dimensions_refuse_zero_negative_huge_and_partial_cells(self):
        for image, grid in [(("0", "16"), ("4", "1")), (("64", "-1"), ("4", "1")),
                            (("2147483648", "16"), ("4", "1")), (("64", "16"), ("0", "1")),
                            (("64", "16"), ("4", "-1")), (("65", "16"), ("4", "1")),
                            (("64", "17"), ("4", "2")), (("1", "1"), ("2", "1"))]:
            with self.subTest(image=image, grid=grid), self.assertRaises(grid_sheet.DefinitionError):
                sheet(image=image, grid=grid)

    def test_invalid_frame_coordinates_name_the_frame_and_field(self):
        for column, row, message in [("-1", "0", "frame 'coin' column"),
                                     ("x", "0", "frame 'coin' column"),
                                     ("0", "-1", "frame 'coin' row"),
                                     ("4", "0", "frame 'coin'.*outside"),
                                     ("0", "2", "frame 'coin'.*outside"),
                                     ("2147483647", "0", "pixel coordinate.*signed int")]:
            with self.subTest(column=column, row=row), self.assertRaisesRegex(grid_sheet.DefinitionError, message):
                sheet(frames=[["coin", column, row]])

    def test_a_strip_must_fit_one_row_without_wrapping(self):
        for column, row, count in [("3", "0", "2"), ("0", "2", "1"), ("4", "0", "1")]:
            with self.subTest(column=column, row=row), self.assertRaisesRegex(
                    grid_sheet.DefinitionError, "strip 'walk'.*outside"):
                sheet(strips=[["walk", column, row, count, "0.125", "true"]])

    def test_empty_strips_do_not_check_unused_atlas_geometry(self):
        definition = sheet(strips=[["empty", "1000", "1000", "0", "0", "false"]])
        empty = definition["animation_strips"][0]
        self.assertEqual(empty["frame_count"], 0)
        self.assertEqual(empty["first_frame"], {"x": 16000, "y": 16000, "w": 16, "h": 16})
        with self.assertRaisesRegex(grid_sheet.DefinitionError, "pixel coordinate.*signed int"):
            sheet(strips=[["empty", "2147483647", "0", "0", "0", "false"]])

    def test_count_and_integer_boundaries_do_not_enumerate_the_grid(self):
        maximum = str(grid_sheet.INT_MAX)
        definition = sheet(image=(maximum, "1"), grid=(maximum, "1"),
                           frames=[["edge", str(grid_sheet.INT_MAX - 1), "0"]],
                           strips=[["maximum", str(grid_sheet.INT_MAX - grid_sheet.MAX_STRIP_FRAMES),
                                    "0", str(grid_sheet.MAX_STRIP_FRAMES), "1", "false"]])
        self.assertEqual(definition["sprite_frames"][0]["position"]["x"], grid_sheet.INT_MAX - 1)
        self.assertEqual(definition["animation_strips"][0]["frame_count"], 16384)
        for count in ("-1", "16385", "2147483648"):
            with self.subTest(count=count), self.assertRaisesRegex(grid_sheet.DefinitionError, "strip 'walk' frame_count"):
                sheet(strips=[["walk", "0", "0", count, "1", "true"]])
        with self.assertRaisesRegex(grid_sheet.DefinitionError, "last frame x.*signed int"):
            sheet(image=(maximum, "1"), grid=(maximum, "1"),
                  strips=[["overflow", maximum, "0", "2", "1", "true"]])

    def test_count_limit_tracks_the_loaders_texture_bound(self):
        header = (ROOT / "engine/render/texture_data.h").read_text(encoding="utf-8")
        maximum = re.search(r"max_texture_dimension\s*=\s*(\d+)\s*;", header)
        self.assertIsNotNone(maximum)
        self.assertEqual(grid_sheet.MAX_STRIP_FRAMES, int(maximum[1]))

    def test_time_retains_finite_loader_values_and_rejects_overflow(self):
        for duration in ("0", "-0.25", "1e-50", str(grid_sheet.FLOAT_MAX)):
            with self.subTest(duration=duration):
                definition = sheet(strips=[["timing", "0", "0", "1", duration, "true"]])
                self.assertEqual(definition["animation_strips"][0]["frame_time"], float(duration))
        for duration in ("NaN", "Infinity", "-Infinity", "1e999", "3.5e38", "soon"):
            with self.subTest(duration=duration), self.assertRaisesRegex(grid_sheet.DefinitionError, "strip 'timing' frame_time"):
                sheet(strips=[["timing", "0", "0", "1", duration, "true"]])
        with self.assertRaisesRegex(grid_sheet.DefinitionError, "strip 'timing' looping"):
            sheet(strips=[["timing", "0", "0", "1", "1", "yes"]])

    def test_repeated_cli_runs_write_identical_utf8_lf_json(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "sheet.json"
            arguments = [sys.executable, str(COMMAND), "--image", "64", "16", "--grid", "4", "1",
                         "--frame", "red", "0", "0", "--strip", "cycle", "0", "0", "4", "0.25", "true",
                         "--output", str(output)]
            subprocess.run(arguments, check=True, capture_output=True)
            first = output.read_bytes()
            subprocess.run(arguments, check=True, capture_output=True)
            self.assertEqual(output.read_bytes(), first)
            self.assertNotIn(b"\r", first)
            self.assertTrue(first.endswith(b"\n"))
            self.assertEqual(set(json.loads(first)), {"sprite_frames", "animation_strips"})

    def test_cli_refusal_preserves_existing_output_and_names_the_problem(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "sheet.json"
            output.write_bytes(b"previous authored definition")
            result = subprocess.run([sys.executable, str(COMMAND), "--image", "64", "16", "--grid", "4", "1",
                                     "--strip", "overflow", "3", "0", "2", "1", "true", "--output", str(output)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn("strip 'overflow' last frame", result.stderr)
            self.assertEqual(output.read_bytes(), b"previous authored definition")
            self.assertEqual(list(Path(directory).iterdir()), [output])

    def test_a_standalone_copy_uses_no_engine_checkout_or_extra_package(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "grid_sheet.py"
            shutil.copyfile(COMMAND, script)
            subprocess.run([sys.executable, str(script), "--image", "16", "16", "--grid", "1", "1",
                            "--frame", "only", "0", "0", "--output", "sheet.json"],
                           cwd=directory, check=True, capture_output=True)
            definition = json.loads((Path(directory) / "sheet.json").read_bytes())
            self.assertEqual(definition["sprite_frames"][0]["size"], {"w": 16, "h": 16})

    def test_failed_replacement_preserves_output_and_cleans_its_temporary(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "sheet.json"
            output.write_bytes(b"previous")
            with patch.object(grid_sheet.os, "replace", side_effect=PermissionError("destination denied")):
                with self.assertRaises(PermissionError):
                    grid_sheet.write_sheet(output, b"new")
            self.assertEqual(output.read_bytes(), b"previous")
            self.assertEqual(list(Path(directory).iterdir()), [output])


if __name__ == "__main__":
    unittest.main()
