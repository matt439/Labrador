"""Write the two-entry wave bank whose first wave carries a loop region.

loop_region.xwb is what AudioTests' Web Audio cases open to see a loop region
travel from the container to the browser's source node. It holds two mono,
signed 16-bit PCM waves at 8,000 Hz, both a 200 Hz triangle made with integer
arithmetic so that every machine writes the same bytes:

    looped  4,000 frames (0.5 s), looping frames 1,000 to 3,000
            (0.125 s to 0.375 s)
    whole   2,000 frames (0.25 s), with no loop region

The rate and the frame counts are chosen so that every time the tests compare
is an exact binary fraction of a second.

The bank is written here in the layout `xwbtool -f -nc` writes, as
make_adpcm_bank.py beside this file does, so that regenerating the fixture
needs Python's standard library and nothing else:

    python tests/audio/content/make_loop_bank.py

THE SAME BANK FROM XWBTOOL, which is the authoring check. `--wavs` writes the
two waves as WAV files, `looped.wav` with a `smpl` chunk holding one forward
loop from sample 1,000 to sample 2,999 inclusive, which is how a WAV spells
the region above. Packing them with XWBTool and comparing shows whether the
tool turned that chunk into the entry's loop region:

    python tests/audio/content/make_loop_bank.py --wavs <folder>
    xwbtool -f -nc -y -o <folder>\\loop_region.xwb <folder>\\looped.wav <folder>\\whole.wav
    python tests/audio/content/make_loop_bank.py --compare <folder>\\loop_region.xwb

`--compare` exits with 0 when the two banks differ only in the build time
XWBTool stamps, and otherwise names the first byte that differs.
"""

from pathlib import Path
import struct
import sys


DIRECTORY = Path(__file__).resolve().parent

SAMPLE_RATE = 8000
TRIANGLE_PERIOD = 40
TRIANGLE_PEAK = 8000

# name, frames, loop start, loop length; a length of zero is no region.
WAVES = [("looped", 4000, 1000, 2000), ("whole", 2000, 0, 0)]

# The wave bank header: its signature ("WBND" little-endian), the content
# version xwbtool writes, and the header version engine/audio/xwb_file.cpp
# reads.
WAVE_BANK_SIGNATURE = 0x444E4257
CONTENT_VERSION = 46
HEADER_VERSION = 44

# The bank data record: an in-memory bank (type 0) with a name table, entry
# names 64 bytes each, entries 4-byte aligned and standard (not compact).
BANK_NAME = b"loop_region"
BANK_FLAG_ENTRY_NAMES = 0x00010000
ENTRY_NAME_LENGTH = 64
ENTRY_ALIGNMENT = 4
COMPACT_FORMAT = 0
BUILD_TIME_AT = 52 + 88
BUILD_TIME_LENGTH = 8

# The mini wave format: PCM is tag 0, then channels, rate, the block
# alignment in bytes and one bit that says 16-bit.
MINI_FORMAT_PCM16_MONO = 0 | (1 << 2) | (SAMPLE_RATE << 5) | (2 << 23) | (1 << 31)


def triangle(frames: int) -> bytes:
    quarter = TRIANGLE_PERIOD // 4
    samples = []
    for frame in range(frames):
        phase = frame % TRIANGLE_PERIOD
        if phase < quarter:
            level = phase
        elif phase < 3 * quarter:
            level = 2 * quarter - phase
        else:
            level = phase - TRIANGLE_PERIOD
        samples.append(level * TRIANGLE_PEAK // quarter)
    return struct.pack(f"<{frames}h", *samples)


def align(size: int) -> int:
    return (size + ENTRY_ALIGNMENT - 1) & ~(ENTRY_ALIGNMENT - 1)


def bank() -> bytes:
    header_size, bank_size, entry_size = 52, 96, 24
    entries_at = header_size + bank_size
    names_at = entries_at + entry_size * len(WAVES)
    names_size = ENTRY_NAME_LENGTH * len(WAVES)
    data_at = align(names_at + names_size)

    payload = bytearray()
    entries = []
    for _, frames, loop_start, loop_length in WAVES:
        pcm = triangle(frames)
        entries.append((frames << 4, MINI_FORMAT_PCM16_MONO, len(payload),
                        len(pcm), loop_start, loop_length))
        payload += pcm
        payload += bytes(align(len(payload)) - len(payload))

    out = bytearray(data_at)
    struct.pack_into("<3I", out, 0,
                     WAVE_BANK_SIGNATURE, CONTENT_VERSION, HEADER_VERSION)
    struct.pack_into("<10I", out, 12,
                     header_size, bank_size,
                     entries_at, entry_size * len(WAVES),
                     names_at, 0,
                     names_at, names_size,
                     data_at, len(payload))
    struct.pack_into("<II64sIIIIQ", out, header_size,
                     BANK_FLAG_ENTRY_NAMES, len(WAVES), BANK_NAME, entry_size,
                     ENTRY_NAME_LENGTH, ENTRY_ALIGNMENT, COMPACT_FORMAT, 0)
    for index, entry in enumerate(entries):
        struct.pack_into("<6I", out, entries_at + entry_size * index, *entry)
        name = WAVES[index][0].encode("ascii")
        struct.pack_into(f"<{ENTRY_NAME_LENGTH}s", out,
                         names_at + ENTRY_NAME_LENGTH * index, name)
    return bytes(out + payload)


def wav(frames: int, loop_start: int, loop_length: int) -> bytes:
    pcm = triangle(frames)
    chunks = struct.pack("<4sIHHIIHH", b"fmt ", 16, 1, 1, SAMPLE_RATE,
                         SAMPLE_RATE * 2, 2, 16)
    chunks += struct.pack("<4sI", b"data", len(pcm)) + pcm
    if loop_length > 0:
        # The sampler chunk: nine header fields, then one loop of six. The
        # period is nanoseconds a sample, the unity note middle C, and the
        # loop is forward (type 0) with an inclusive last sample.
        chunks += struct.pack("<4sI9I6I", b"smpl", 36 + 24,
                              0, 0, 1000000000 // SAMPLE_RATE, 60, 0, 0, 0,
                              1, 0,
                              0, 0, loop_start, loop_start + loop_length - 1,
                              0, 0)
    return struct.pack("<4sI4s", b"RIFF", 4 + len(chunks), b"WAVE") + chunks


def compare(path: Path) -> int:
    ours = bytearray(bank())
    theirs = bytearray(path.read_bytes())
    for data in (ours, theirs):
        if len(data) >= BUILD_TIME_AT + BUILD_TIME_LENGTH:
            data[BUILD_TIME_AT:BUILD_TIME_AT + BUILD_TIME_LENGTH] = bytes(
                BUILD_TIME_LENGTH)
    if ours == theirs:
        print(f"{path.name}: matches this script's bank apart from the "
              "build time; the looped entry loops frames 1000 to 3000")
        return 0
    if len(ours) != len(theirs):
        print(f"{path.name}: {len(theirs)} bytes, and this script writes "
              f"{len(ours)}")
        return 1
    at = next(i for i in range(len(ours)) if ours[i] != theirs[i])
    print(f"{path.name}: byte {at} is {theirs[at]:#04x}, and this script "
          f"writes {ours[at]:#04x}")
    return 1


def main(arguments: list[str]) -> int:
    if len(arguments) == 2 and arguments[0] == "--wavs":
        folder = Path(arguments[1])
        for name, frames, loop_start, loop_length in WAVES:
            (folder / f"{name}.wav").write_bytes(
                wav(frames, loop_start, loop_length))
        return 0
    if len(arguments) == 2 and arguments[0] == "--compare":
        return compare(Path(arguments[1]))
    if arguments:
        print("usage: make_loop_bank.py [--wavs <folder> | --compare <bank>]")
        return 2
    (DIRECTORY / "loop_region.xwb").write_bytes(bank())
    print("loop_region.xwb: " + ", ".join(
        f"{name} {frames} frames" for name, frames, _, _ in WAVES))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
