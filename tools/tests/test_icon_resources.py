"""Named PE resources and icon-group metadata, without executing a program."""

from __future__ import annotations

import struct
import unittest

from tools.tests import pe_fixtures as images
from tools.tests.tool_fixtures import load_tool

checker = load_tool("tools/check_dpi_manifest.py")
release = load_tool("tools/consumer_check/check_release.py")


def group(*entries: tuple[int, int], reserved=0, resource_type=1, entry_reserved=0) -> bytes:
    result = struct.pack("<HHH", reserved, resource_type, len(entries))
    for image_id, size in entries:
        result += struct.pack("<BBBBHHIH", 16, 16, 0, entry_reserved, 1, 32, size, image_id)
    return result


def named_string_offset(data: bytes) -> int:
    """Name string of the first entry under a fixture's only numeric type."""
    root = images.SECTION_FILE
    _, target = struct.unpack_from("<II", data, root + 16)
    name_directory = root + (target & 0x7FFFFFFF)
    identifier, _ = struct.unpack_from("<II", data, name_directory + 16)
    return root + (identifier & 0x7FFFFFFF)


class NamedResourceTests(unittest.TestCase):
    def test_numeric_and_named_resources_are_distinct_in_both_pe_formats(self):
        for plus in (True, False):
            data = images.image([(14, 1, b"numeric"), (14, "GAME_ICON", b"named"),
                                 ("CUSTOM_TYPE", "CUSTOM_NAME", b"custom"),
                                 (24, 1, images.TEMPLATE)], plus=plus)
            with self.subTest(plus=plus):
                image = checker.PortableExecutable(data)
                self.assertEqual(image.resource(14, 1), b"numeric")
                self.assertEqual(image.resource(14, "GAME_ICON"), b"named")
                self.assertEqual(image.resource("CUSTOM_TYPE", "CUSTOM_NAME"), b"custom")
                self.assertIsNone(image.resource(14, "1"))
                self.assertIsNone(image.resource(14, "missing"))
                self.assertIsNone(image.resource("CUSTOM_TYPE", "missing"))
                self.assertTrue(checker.check_program(data).startswith("PerMonitorV2"))

    def test_names_use_utf16_code_units_and_exact_spelling(self):
        name = "ICON_\U0001f600"
        image = checker.PortableExecutable(images.image([(14, name, b"pixels")]))
        self.assertEqual(image.resource(14, name), b"pixels")
        self.assertIsNone(image.resource(14, name.lower()))

    def test_name_length_and_pointer_are_checked_before_decoding(self):
        original = images.image([(14, "GAME_ICON", b"pixels")])
        oversized = bytearray(original)
        struct.pack_into("<H", oversized, named_string_offset(original), 0xFFFF)
        with self.assertRaisesRegex(checker.ManifestError, "truncated resource name"):
            checker.PortableExecutable(bytes(oversized)).resource(14, "GAME_ICON")
        outside = bytearray(original)
        _, target = struct.unpack_from("<II", outside, images.SECTION_FILE + 16)
        name_directory = images.SECTION_FILE + (target & 0x7FFFFFFF)
        struct.pack_into("<I", outside, name_directory + 16, 0xFFFFFFFF)
        with self.assertRaisesRegex(checker.ManifestError, "truncated resource name"):
            checker.PortableExecutable(bytes(outside)).resource(14, "GAME_ICON")

    def test_invalid_utf16_is_a_named_parser_error(self):
        original = images.image([(14, "GAME_ICON", b"pixels")])
        invalid = bytearray(original)
        struct.pack_into("<H", invalid, named_string_offset(original) + 2, 0xD800)
        with self.assertRaisesRegex(checker.ManifestError, "not valid UTF-16"):
            checker.PortableExecutable(bytes(invalid)).resource(14, "GAME_ICON")

    def test_bogus_directory_count_does_not_walk_beyond_its_section(self):
        invalid = bytearray(images.image([(14, "GAME_ICON", b"pixels")]))
        struct.pack_into("<HH", invalid, images.SECTION_FILE + 12, 0xFFFF, 0)
        with self.assertRaisesRegex(checker.ManifestError, "truncated resource directory entries"):
            checker.PortableExecutable(bytes(invalid)).resource(14, "GAME_ICON")


class IconGroupTests(unittest.TestCase):
    def test_named_group_checks_each_opaque_image_reference(self):
        # These bytes are intentionally not decoded as an image: this test
        # proves metadata consistency, not LoadIcon or appearance acceptance.
        data = images.image([(24, 1, images.TEMPLATE),
                             (14, "GAME_ICON", group((7, 8), (8, 12))),
                             (3, 7, b"12345678"), (3, 8, b"abcdefghijkl")])
        self.assertEqual(release.icon_image_count(data, "GAME_ICON"), 2)

    def test_zero_dimension_bytes_are_the_256_pixel_encoding(self):
        header = struct.pack("<HHH", 0, 1, 1)
        entry = struct.pack("<BBBBHHIH", 0, 0, 0, 0, 1, 32, 8, 7)
        data = images.image([(14, "GAME_ICON", header + entry), (3, 7, b"12345678")])
        self.assertEqual(release.icon_image_count(data, "GAME_ICON"), 1)

    def test_missing_name_and_numeric_group_do_not_pass_named_lookup(self):
        for entries in ([(3, 7, b"12345678")],
                        [(14, 1, group((7, 8))), (3, 7, b"12345678")],
                        [(14, "OTHER_ICON", group((7, 8))), (3, 7, b"12345678")]):
            with self.subTest(entries=entries), self.assertRaisesRegex(checker.ManifestError, "no named RT_GROUP_ICON 'GAME_ICON'"):
                release.icon_image_count(images.image(entries), "GAME_ICON")

    def test_group_header_and_declared_entries_are_validated(self):
        cases = [
            (b"\0\0", "truncated group header"),
            (group((7, 8), reserved=1), "invalid icon-group header"),
            (group((7, 8), resource_type=2), "invalid icon-group header"),
            (group(), "no image entries"),
            (struct.pack("<HHH", 0, 1, 2) + group((7, 8))[6:], "truncated image entries"),
            (group((7, 8), entry_reserved=1), "invalid reserved/size"),
            (group((7, 0)), "invalid reserved/size"),
        ]
        for contents, expected in cases:
            with self.subTest(expected=expected), self.assertRaisesRegex(checker.ManifestError, expected):
                release.icon_image_count(images.image([(14, "GAME_ICON", contents), (3, 7, b"12345678")]), "GAME_ICON")

    def test_missing_empty_or_size_mismatched_image_is_refused(self):
        cases = [([], "missing RT_ICON 7"), ([(3, 7, b"")], "empty RT_ICON 7"),
                 ([(3, 7, b"short")], "declares 8 bytes.*found 5")]
        for payloads, expected in cases:
            with self.subTest(expected=expected), self.assertRaisesRegex(checker.ManifestError, expected):
                release.icon_image_count(images.image([(14, "GAME_ICON", group((7, 8))), *payloads]), "GAME_ICON")

    def test_image_payload_range_cannot_run_into_missing_file_bytes(self):
        entries = [(14, "GAME_ICON", group((7, 8))), (3, 7, b"12345678")]
        data = images.image(entries)
        truncated = data[:images.SECTION_FILE + len(images.resource_section(entries)) - 1]
        with self.assertRaisesRegex(checker.ManifestError, "truncated resource data"):
            release.icon_image_count(truncated, "GAME_ICON")


if __name__ == "__main__":
    unittest.main()
