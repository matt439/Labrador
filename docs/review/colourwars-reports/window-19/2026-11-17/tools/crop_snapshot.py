"""Keep only the sample client pixels from a bounded CUA window snapshot."""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    metadata = json.loads((args.directory / "snapshot.json").read_text())
    events = [json.loads(line) for line in (args.directory / "events.jsonl").read_text().splitlines()]
    event = [event for event in events if event.get("event") == "client_redraw_exported"][-1]
    screenshot = metadata["screenshot"]
    image = Image.open(args.directory / "snapshot-window.png")
    x = screenshot.get("originX")
    y = screenshot.get("originY")
    if x is None or y is None:
        candidates = [event["frame_rect"], event["window_rect"]]
        matches = [bounds for bounds in candidates if (bounds[2] - bounds[0], bounds[3] - bounds[1]) == image.size]
        if len(matches) != 1:
            raise ValueError("Snapshot has no origin and no unique matching measured window/frame bounds")
        x, y = matches[0][:2]
    left = event["client_origin"][0] - x
    top = event["client_origin"][1] - y
    width, height = event["client_size"]
    box = (left, top, left + width, top + height)
    if min(left, top) < 0 or box[2] > image.width or box[3] > image.height:
        raise ValueError(f"Client rectangle {box} is outside captured window {image.size}; no crop saved")
    target = args.directory / "client-visible.png"
    image.crop(box).save(target)
    metadata["client_crop"] = {
        "box_in_snapshot": box,
        "size": [width, height],
        "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "geometry_event": event,
    }
    (args.directory / "snapshot.json").write_text(json.dumps(metadata, indent=2))
    # The intermediate is bounded to the sample window; retain only its client crop.
    (args.directory / "snapshot-window.png").unlink()
    print(target)


if __name__ == "__main__":
    main()
