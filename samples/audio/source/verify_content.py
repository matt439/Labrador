"""Check this sample's named, standard PCM wave bank against its source WAVs.

This deliberately accepts only the -f -nc output used by this sample. It is an
authoring check, not an engine container reader. Format reference:
https://github.com/microsoft/DirectXTK/blob/may2026/Audio/WaveBankReader.cpp
"""

from pathlib import Path
import json
import struct
import wave


DIRECTORY = Path(__file__).resolve().parent


def verify() -> None:
    bank_path = DIRECTORY.parent / "content/sounds/tones.xwb"
    data = bank_path.read_bytes()
    header = struct.unpack_from("<13I", data)
    assert header[:3] == (0x444E4257, 46, 44), "unexpected XWB header"
    segments = list(zip(header[3::2], header[4::2]))
    for offset, length in segments:
        assert offset + length <= len(data), "truncated XWB segment"
    bank_offset, bank_size = segments[0]
    assert bank_size == 96, "unexpected bank metadata size"
    flags, count, name, entry_size, name_size, alignment, _, _ = struct.unpack_from(
        "<II64sIIIIQ", data, bank_offset
    )
    assert flags == 0x10000, "requires named, in-memory, non-compact bank"
    assert name.rstrip(b"\0") == b"tones"
    assert (count, entry_size, name_size, alignment) == (3, 24, 64, 4)
    definition = json.loads((bank_path.with_suffix(".json")).read_text())
    assert len(definition["waves"]) == count
    entries_offset, entries_length = segments[1]
    names_offset, names_length = segments[3]
    pcm_offset, pcm_length = segments[4]
    assert entries_length == count * entry_size
    assert names_length == count * name_size
    for index, expected in enumerate(definition["waves"]):
        entry_name = data[names_offset + index * name_size:
                          names_offset + (index + 1) * name_size]
        assert entry_name.rstrip(b"\0").decode("ascii") == expected
        duration, mini_format, start, length, loop_start, loop_length = struct.unpack_from(
            "<6I", data, entries_offset + index * entry_size
        )
        # PCM, mono, 22050 Hz, block-align 2, signed 16-bit.
        assert mini_format == (1 << 31) | (2 << 23) | (22050 << 5) | (1 << 2)
        assert (loop_start, loop_length) == (0, 0)
        assert start % alignment == 0 and start + length <= pcm_length
        with wave.open(str(DIRECTORY / f"{expected}.wav"), "rb") as source:
            assert (source.getnchannels(), source.getsampwidth(), source.getframerate()) == (
                1, 2, 22050
            )
            assert duration >> 4 == source.getnframes()
            pcm = source.readframes(source.getnframes())
        assert data[pcm_offset + start:pcm_offset + start + length] == pcm
        values = struct.unpack(f"<{len(pcm) // 2}h", pcm)
        assert 1000 < max(abs(value) for value in values) < 12000
        assert abs(sum(values) / len(values)) < 50, "unexpected DC offset"
        print(f"{expected}: {len(values)} samples; source PCM matches named XWB entry")


if __name__ == "__main__":
    verify()
