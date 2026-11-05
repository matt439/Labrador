"""Check this sample's named, standard PCM wave bank against its source WAVs.

This deliberately accepts only the -f -nc output used by this sample. It is an
authoring check, not an engine container reader. Format reference:
https://github.com/microsoft/DirectXTK/blob/may2026/Audio/WaveBankReader.cpp

tests/audio/content/make_adpcm_bank.py writes the same layout for its ADPCM
fixture; the constants below name what both scripts spell.
"""

from pathlib import Path
import json
import struct
import wave


DIRECTORY = Path(__file__).resolve().parent

# The file header: signature "WBND", content version 46, header version 44,
# then five (offset, length) segments in this order.
XWB_SIGNATURE = 0x444E4257
XWB_VERSIONS = (46, 44)
HEADER_FORMAT = "<13I"
BANK_DATA, ENTRY_METADATA, SEEK_TABLES, ENTRY_NAMES, ENTRY_WAVE_DATA = range(5)

# The bank-data segment: flags, entry count, bank name, entry size, entry-name
# size, alignment, compact format, build time.
BANK_DATA_FORMAT = "<II64sIIIIQ"
BANK_DATA_SIZE = 96
FLAGS_NAMED_IN_MEMORY = 0x10000
ENTRY_SIZE, NAME_SIZE, ALIGNMENT = 24, 64, 4

# One standard entry: duration and flags, mini-format, play region, loop region.
ENTRY_FORMAT = "<6I"
# PCM, mono, 22050 Hz, block-align 2, signed 16-bit.
PCM_MONO_22050 = (1 << 31) | (2 << 23) | (22050 << 5) | (1 << 2)

# The README promises a peak below 0.3 full scale, and a tone well above
# silence; the generator's FULL_SCALE is 32767.
PEAK_LIMIT = 0.3 * 32767
PEAK_FLOOR = 1000
DC_OFFSET_LIMIT = 50


def require(condition: bool, message: str) -> None:
    """An assert that python -O does not strip: the check is the point."""
    if not condition:
        raise SystemExit(f"verify_content: {message}")


def verify() -> None:
    bank_path = DIRECTORY.parent / "content/sounds/tones.xwb"
    data = bank_path.read_bytes()
    header = struct.unpack_from(HEADER_FORMAT, data)
    require(header[:3] == (XWB_SIGNATURE, *XWB_VERSIONS), "unexpected XWB header")
    segments = list(zip(header[3::2], header[4::2]))
    for offset, length in segments:
        require(offset + length <= len(data), "truncated XWB segment")
    # -nc writes no seek tables, and the segments follow one another in order
    # without overlapping.
    require(segments[SEEK_TABLES][1] == 0, "unexpected seek tables")
    present = [segment for segment in segments if segment[1] > 0]
    for (offset, length), (next_offset, _) in zip(present, present[1:]):
        require(offset + length <= next_offset, "XWB segments out of order or overlapping")

    bank_offset, bank_size = segments[BANK_DATA]
    require(bank_size == BANK_DATA_SIZE, "unexpected bank metadata size")
    flags, count, name, entry_size, name_size, alignment, _, _ = struct.unpack_from(
        BANK_DATA_FORMAT, data, bank_offset
    )
    require(flags == FLAGS_NAMED_IN_MEMORY, "requires named, in-memory, non-compact bank")
    require(name.rstrip(b"\0") == b"tones", "the bank is not named tones")
    require((entry_size, name_size, alignment) == (ENTRY_SIZE, NAME_SIZE, ALIGNMENT),
            "unexpected entry, name or alignment size")
    definition = json.loads((bank_path.with_suffix(".json")).read_text())
    require(len(definition["waves"]) == count, "tones.json and the bank list different counts")
    entries_offset, entries_length = segments[ENTRY_METADATA]
    names_offset, names_length = segments[ENTRY_NAMES]
    pcm_offset, pcm_length = segments[ENTRY_WAVE_DATA]
    require(entries_length == count * entry_size, "unexpected entry metadata length")
    require(names_length == count * name_size, "unexpected entry name length")
    for index, expected in enumerate(definition["waves"]):
        entry_name = data[names_offset + index * name_size:
                          names_offset + (index + 1) * name_size]
        require(entry_name.rstrip(b"\0").decode("ascii") == expected,
                f"entry {index} is not named {expected}")
        duration, mini_format, start, length, loop_start, loop_length = struct.unpack_from(
            ENTRY_FORMAT, data, entries_offset + index * entry_size
        )
        require(mini_format == PCM_MONO_22050, f"{expected} is not 16-bit mono PCM at 22050 Hz")
        require((loop_start, loop_length) == (0, 0), f"{expected} has a loop region")
        require(start % alignment == 0 and start + length <= pcm_length,
                f"{expected}'s play region is misaligned or outside the wave data")
        with wave.open(str(DIRECTORY / f"{expected}.wav"), "rb") as source:
            require((source.getnchannels(), source.getsampwidth(), source.getframerate()) ==
                    (1, 2, 22050), f"{expected}.wav is not 16-bit mono at 22050 Hz")
            require(duration >> 4 == source.getnframes(),
                    f"{expected}'s duration differs from its WAV")
            pcm = source.readframes(source.getnframes())
        require(data[pcm_offset + start:pcm_offset + start + length] == pcm,
                f"{expected}'s bank PCM differs from its WAV")
        values = struct.unpack(f"<{len(pcm) // 2}h", pcm)
        peak = max(abs(value) for value in values)
        require(PEAK_FLOOR < peak < PEAK_LIMIT,
                f"{expected}'s peak {peak} is not between {PEAK_FLOOR} and 0.3 full scale")
        require(abs(sum(values) / len(values)) < DC_OFFSET_LIMIT, "unexpected DC offset")
        print(f"{expected}: {len(values)} samples; source PCM matches named XWB entry")


if __name__ == "__main__":
    verify()
