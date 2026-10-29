# Original animation-preview content

`../content/textures/animation_preview.dds` is original geometric pixel data
created for Labrador's components sample from an integer-distance rule. The
four 16x16 frames are a centred cyan diamond with an opaque pale outline; radii are
2, 4, 6 and 4 texels. Other texels are transparent black. Every visible texel
is opaque, so RGB/alpha satisfy the engine's premultiplied blending contract.

The 64x16 atlas is a single-level legacy DDS with RGBA8 channel masks. The
paired JSON offers `pulse_loop` and `pulse_once` over those same rectangles,
at 0.25 seconds per frame. One-shot behavior is supplied by copied sample
code, not by a file player in the engine.

The data, JSON and generator are distributed under Labrador's MIT licence.
Keep `../content/textures/animation_preview.LICENSE.txt` when copying them.

Regenerate both files from any working directory with Python 3:

```text
python samples/components/source/generate_animation_preview.py
```

The script writes only those two adjacent content files and prints their
byte counts and SHA-256 hashes. It uses the standard library and does not
need an image application, network, graphics device or build tool. All source
and JSON text is ASCII; Unicode-source policy remains untouched.

The asset's arithmetic provenance does not prove that the gallery layout looks
correct. A change to the asset or the layout is reviewed in the native and
browser rendered previews.
