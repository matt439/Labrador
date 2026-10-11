"""Pack an MS-ADPCM WAV into the one-entry wave bank AudioTests reads.

ping_adpcm.xwb is samples/audio/source/ping.wav encoded by FFmpeg's MS-ADPCM
encoder (FFmpeg 8.1.2, 256-byte blocks, so 500 samples a block) and packed here
in the layout `xwbtool -nc -f` writes: header, bank data, standard entries, an
empty seek table, the name table and the wave data, 4-byte aligned. xwbtool
does not encode, it packs a WAV that is already ADPCM; this
script stands in for xwbtool so that regenerating the fixture needs one tool
rather than two. Python standard library only.

To regenerate, from the repository root:

    ffmpeg -y -i samples/audio/source/ping.wav -c:a adpcm_ms -block_size 256 \
        tests/audio/content/ping_adpcm.wav
    python tests/audio/content/make_adpcm_bank.py
    (then delete ping_adpcm.wav; only the bank is checked in)

The bank is a test fixture with its decode pinned. AudioTests holds what the
engine decodes from it to the source WAV within a bound, and exactly to an
FNV-1a hash of what Windows' MS-ADPCM codec (msacm32) decodes from these same
blocks and to two pinned samples (500 and 503). A regenerated bank - from a
different FFmpeg, say - has different blocks, so that hash and those samples
in xwb_file_tests.cpp have to be re-derived from msacm32's decode of the new
bank, not from the engine's own output.
"""

from pathlib import Path
import struct


DIRECTORY = Path(__file__).resolve().parent
STANDARD_COEFFICIENTS = [(256, 0), (512, -256), (0, 0), (192, 64), (240, 0),
                         (460, -208), (392, -232)]


def chunks(data: bytes) -> dict[bytes, bytes]:
    assert data[0:4] == b"RIFF" and data[8:12] == b"WAVE", "not a WAV"
    found = {}
    at = 12
    while at + 8 <= len(data):
        tag, size = data[at:at + 4], struct.unpack_from("<I", data, at + 4)[0]
        found[tag] = data[at + 8:at + 8 + size]
        at += 8 + size + (size & 1)
    return found


def main() -> None:
    source = chunks((DIRECTORY / "ping_adpcm.wav").read_bytes())
    fmt = source[b"fmt "]
    tag, channels, rate, _, block_align, bits = struct.unpack_from("<HHIIHH", fmt)
    assert (tag, channels, bits) == (2, 1, 4), "expected mono MS-ADPCM"
    samples_per_block, coefficient_count = struct.unpack_from("<HH", fmt, 18)
    coefficients = [struct.unpack_from("<hh", fmt, 22 + 4 * i)
                    for i in range(coefficient_count)]
    assert coefficients == STANDARD_COEFFICIENTS, "non-standard coefficients"
    assert samples_per_block == block_align * 2 // channels - 12
    stored_align = block_align // channels - 22
    assert 0 <= stored_align <= 255, "block too large for the mini format"
    duration = struct.unpack_from("<I", source[b"fact"])[0]
    payload = source[b"data"]
    assert len(payload) % block_align == 0, "part block"

    mini_format = (2 | (channels << 2) | (rate << 5) | (stored_align << 23))
    names = b"ping".ljust(64, b"\0")
    header_size, bank_size, entry_size = 52, 96, 24
    entries_at = header_size + bank_size
    names_at = entries_at + entry_size
    data_at = (names_at + len(names) + 3) & ~3

    out = bytearray(data_at)
    struct.pack_into("<3I", out, 0, 0x444E4257, 46, 44)
    struct.pack_into("<10I", out, 12,
                     header_size, bank_size,
                     entries_at, entry_size,
                     names_at, 0,
                     names_at, len(names),
                     data_at, len(payload))
    struct.pack_into("<II64sIIIIQ", out, header_size,
                     0x00010000, 1, b"adpcm", entry_size, 64, 4, 0, 0)
    struct.pack_into("<6I", out, entries_at,
                     duration << 4, mini_format, 0, len(payload), 0, 0)
    out[names_at:names_at + len(names)] = names
    out += payload
    (DIRECTORY / "ping_adpcm.xwb").write_bytes(bytes(out))
    print(f"ping_adpcm.xwb: {duration} samples in {len(payload) // block_align} blocks")


if __name__ == "__main__":
    main()
