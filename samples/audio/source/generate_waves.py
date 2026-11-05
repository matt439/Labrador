"""Author the sample's original PCM tones; Python standard library only.

Run from any directory. No recording, sample pack, or external audio is used.
The generated WAV files and their source are covered by the repository MIT licence.
"""

from pathlib import Path
import math
import struct
import wave


SAMPLE_RATE = 22050
DIRECTORY = Path(__file__).resolve().parent

# The largest signed 16-bit sample: an amplitude of 1.0. The README's peak
# figure, and verify_content.py's bound on it, are fractions of this.
FULL_SCALE = 32767

# How long a tone takes to rise from silence and to fall back to it, so that
# neither end clicks.
FADE_SECONDS = 0.012


def sine(frequency: float, index: int) -> float:
    """The unit sine at a frequency, at one sample index."""
    return math.sin(2 * math.pi * frequency * index / SAMPLE_RATE)


def tone(frequency: float, seconds: float, amplitude: float) -> list[int]:
    count = round(seconds * SAMPLE_RATE)
    fade = round(FADE_SECONDS * SAMPLE_RATE)
    return [
        round(
            FULL_SCALE * amplitude
            * min(1.0, index / fade, (count - 1 - index) / fade)
            * sine(frequency, index)
        )
        for index in range(count)
    ]


def write_wave(name: str, samples: list[int]) -> None:
    with wave.open(str(DIRECTORY / f"{name}.wav"), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(struct.pack(f"<{len(samples)}h", *samples))


def main() -> None:
    write_wave("ping", tone(880, 0.16, 0.30))
    chime = []
    for frequency in (523.25, 659.25, 783.99):
        chime.extend(tone(frequency, 0.14, 0.25))
    write_wave("chime", chime)
    # Two partials with no fade, because the loop has no ends: whole cycles
    # make the loop boundary continuous, including its slope.
    drone = [
        round(FULL_SCALE * (0.15 * sine(220, index) + 0.075 * sine(330, index)))
        for index in range(SAMPLE_RATE)
    ]
    write_wave("drone", drone)


if __name__ == "__main__":
    main()
