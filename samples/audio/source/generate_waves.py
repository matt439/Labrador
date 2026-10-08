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


def tone(frequency: float, seconds: float, amplitude: float) -> list[int]:
    count = round(seconds * SAMPLE_RATE)
    fade = round(0.012 * SAMPLE_RATE)
    return [
        round(
            32767 * amplitude
            * min(1.0, index / fade, (count - 1 - index) / fade)
            * math.sin(2 * math.pi * frequency * index / SAMPLE_RATE)
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
    # Whole cycles make the loop boundary continuous, including its slope.
    drone = [
        round(32767 * (0.15 * math.sin(2 * math.pi * 220 * index / SAMPLE_RATE)
                       + 0.075 * math.sin(2 * math.pi * 330 * index / SAMPLE_RATE)))
        for index in range(SAMPLE_RATE)
    ]
    write_wave("drone", drone)


if __name__ == "__main__":
    main()
