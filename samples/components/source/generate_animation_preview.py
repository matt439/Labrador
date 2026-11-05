"""Write the original MIT geometric atlas and its paired Labrador strip data.

Uses only Python's standard library. Run from any directory; output paths are
relative to this script, never the current directory. No external image tool,
download, shader compiler or display is needed.
"""

import hashlib
import json
from pathlib import Path
import struct

# One square frame per radius, side by side in a single row. A frame has an
# even number of texels, so it has no middle texel; the diamond is centred on
# CENTRE in both axes, half a texel up and left of the frame's true middle.
FRAME = 16
CENTRE = 7
RADII = (2, 4, 6, 4)
WIDTH = FRAME * len(RADII)
HEIGHT = FRAME


def atlas_bytes():
    pixels = bytearray(WIDTH * HEIGHT * 4)
    for frame, radius in enumerate(RADII):
        for y in range(HEIGHT):
            for x in range(FRAME):
                distance = abs(x - CENTRE) + abs(y - CENTRE)
                if distance > radius:
                    continue
                rgba = (224, 247, 250, 255) if distance == radius else (38, 198, 218, 255)
                offset = (y * WIDTH + frame * FRAME + x) * 4
                pixels[offset:offset + 4] = bytes(rgba)
    # Legacy DDS: one uncompressed RGBA8 level, no DX10 header or mip chain.
    header = [124, 0x100F, HEIGHT, WIDTH, WIDTH * 4, 0, 1]
    header += [0] * 11
    header += [32, 0x41, 0, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000]
    header += [0x1000, 0, 0, 0, 0]
    return b"DDS " + struct.pack("<31I", *header) + pixels


def sheet_text():
    return json.dumps({
        "sprite_frames": [],
        "animation_strips": [
            {"name": name, "first_frame": {"x": 0, "y": 0, "w": FRAME, "h": FRAME},
             "frame_count": len(RADII), "frame_time": 0.25, "looping": looping}
            for name, looping in [("pulse_loop", True), ("pulse_once", False)]
        ]
    }, indent=4) + "\n"


def main():
    output = Path(__file__).resolve().parents[1] / "content/textures"
    output.mkdir(parents=True, exist_ok=True)
    for name, contents in [("animation_preview.dds", atlas_bytes()),
                           ("animation_preview.json", sheet_text().encode("ascii"))]:
        target = output / name
        target.write_bytes(contents)
        print(f"{name}: {len(contents)} bytes, sha256={hashlib.sha256(contents).hexdigest()}")


if __name__ == "__main__":
    main()
