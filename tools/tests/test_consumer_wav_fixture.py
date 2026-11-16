"""The direct-WAV guide's copied consumer edits and refusal boundaries, without a compiler."""

from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from tools.tests.tool_fixtures import load_tool


fixture = load_tool("tools/consumer_check/wav_fixture.py")
REPOSITORY = Path(__file__).resolve().parents[2]


class WavConsumerTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-wav-consumer-")
        self.addCleanup(temporary.cleanup)
        self.project = Path(temporary.name)
        self.source = self.project / "external/labrador"
        for source, destination in (
            ("website/src/walkthrough/root-CMakeLists.txt", "CMakeLists.txt"),
            ("website/src/walkthrough/game-CMakeLists.txt", "game/CMakeLists.txt"),
            ("samples/minimal/states/hello_state.h", "game/states/hello_state.h"),
            ("samples/minimal/states/hello_state.cpp", "game/states/hello_state.cpp"),
            ("samples/minimal/content/manifest.json", "game/content/manifest.json"),
            (str(fixture.GUIDE), "external/labrador/" + fixture.GUIDE.as_posix()),
            ("samples/audio/source/ping.wav", "external/labrador/samples/audio/source/ping.wav"),
        ):
            path = self.project / destination
            path.parent.mkdir(parents=True, exist_ok=True)
            data = (REPOSITORY / source).read_bytes()
            if path.suffix in {".cpp", ".h"}:
                data = data.replace(b'#include "samples/minimal/', b'#include "game/')
            path.write_bytes(data)
        # A capability preflight fixture, not a substitute for compiling the
        # actual source and consumer together in the coordinated check.
        header = self.source / "engine/audio/audio_device.h"
        header.parent.mkdir(parents=True)
        header.write_text("WaveBankHandle open_memory_bank(WaveBankData data);\n", encoding="utf-8")
        loader = self.source / "engine/assets/resource_loader.cpp"
        loader.parent.mkdir(parents=True)
        loader.write_text('register_kind("wave", {});\n', encoding="utf-8")

    def contents(self):
        return {path.relative_to(self.project): path.read_bytes()
                for path in self.project.rglob("*") if path.is_file()}

    def test_prepares_the_guide_code_and_real_source_wav_without_a_bank_packer(self):
        source_pcm = (self.source / "samples/audio/source/ping.wav").read_bytes()
        fixture.prepare(self.project)
        wave = self.project / "game/content/waves/ping_wav.wav"
        self.assertEqual(wave.read_bytes(), source_pcm)
        manifest = json.loads((self.project / "game/content/manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["assets"][-1],
                         {"kind": "wave", "directory": "./waves/", "names": ["ping_wav"]})
        header = (self.project / "game/states/hello_state.h").read_text(encoding="utf-8")
        state = (self.project / "game/states/hello_state.cpp").read_text(encoding="utf-8")
        guide = (self.source / fixture.GUIDE).read_text(encoding="utf-8")
        self.assertIn(fixture.fragment(guide, "Add the sound-bank include"), header)
        self.assertIn('sound_bank("ping_wav")', state)
        self.assertLess(state.index('resolve_wave("ping_wav")'), state.index("void HelloState::update"))
        self.assertIn("keyboard.pressed(Key::space)", state)
        self.assertIn("play_wave(this->ping_wav_, 0.4f)", state)
        self.assertNotIn("xwbtool", header + state)
        cmake = (self.project / "game/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn(fixture.WAVE_DEPENDENCY, cmake)
        self.assertIn(fixture.COPY_WAVES.rstrip(), cmake)
        self.assertIn("install(DIRECTORY content/waves DESTINATION .)", cmake)

    def test_extra_consumer_test_is_opt_in_and_reads_the_source_wav_on_native_and_node(self):
        fixture.prepare(self.project)
        cmake = (self.project / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertLess(cmake.index("add_subdirectory(game)"), cmake.index("add_executable(MyGameWaveTests"))
        self.assertIn("-sNODERAWFS=1 -sENVIRONMENT=node", cmake)
        self.assertIn("PROPERTIES TIMEOUT 60", cmake)
        self.assertEqual((self.project / "game/content/waves/consumer_broken_wave.wav").read_bytes(), b"RIFF")
        self.assertIn("consumer_missing_wave", (self.project / "game/wav_consumer_tests.cpp").read_text())
        plain = (REPOSITORY / "website/src/walkthrough/root-CMakeLists.txt").read_text()
        self.assertNotIn("MyGameWaveTests", plain)

    def test_missing_state_anchor_changes_nothing(self):
        path = self.project / "game/states/hello_state.cpp"
        path.write_text("starter changed\n", encoding="utf-8")
        before = self.contents()
        with self.assertRaisesRegex(ValueError, "exactly one"):
            fixture.prepare(self.project)
        self.assertEqual(self.contents(), before)

    def test_old_engine_is_refused_before_any_consumer_edit(self):
        (self.source / "engine/audio/audio_device.h").write_text("old API\n", encoding="utf-8")
        before = self.contents()
        with self.assertRaisesRegex(ValueError, "v0.3.0"):
            fixture.prepare(self.project)
        self.assertEqual(self.contents(), before)

    def test_existing_payload_is_not_overwritten(self):
        path = self.project / "game/content/waves/ping_wav.wav"
        path.parent.mkdir(parents=True)
        path.write_bytes(b"a user's existing WAV")
        before = self.contents()
        with self.assertRaisesRegex(ValueError, "already exists"):
            fixture.prepare(self.project)
        self.assertEqual(self.contents(), before)

    def test_second_application_is_refused_without_changing_the_prepared_project(self):
        fixture.prepare(self.project)
        before = self.contents()
        with self.assertRaisesRegex(ValueError, "already has the WAV fixture"):
            fixture.prepare(self.project)
        self.assertEqual(self.contents(), before)

    def test_drifted_guide_paste_block_is_not_replaced_by_a_handwritten_alternative(self):
        path = self.source / fixture.GUIDE
        path.write_text(path.read_text().replace('title="Play once on Space"', 'title="Changed"'), encoding="utf-8")
        before = self.contents()
        with self.assertRaisesRegex(ValueError, "Play once on Space"):
            fixture.prepare(self.project)
        self.assertEqual(self.contents(), before)


if __name__ == "__main__":
    unittest.main()
