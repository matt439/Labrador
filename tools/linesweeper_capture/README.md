# LineSweeperCapture

Exact, repeatable PNG images of the LineSweeper sample, made for the website.
It replays a checked-in script through the sample's own rules and draws the
frames the script names with the sample's own presentation on a real device.
It then reads each frame back with `Renderer::read_back_buffer`. Nothing in it
reads a clock, a key or a pad, so the same script, build and device give the
same bytes.

## Why it works this way

The tool does not drive the running sample by posting key messages. That was
tried, and the input stopped registering partway through a run
(`docs/website-inventory.md`, *Media*). It is also
the wrong dependency: what a posted key does depends on which frames the window
had the keyboard. The sample's rules are a library that links nothing
(`samples/linesweeper/README.md`, *The rules are a static library that links
nothing*), so a match is the bytes handed to `tick()`. The presentation only
reads the match.

So the tool owns the `World` and assembles the scene `PlayState::init` builds.
That means the board, the particle field, the top-out banner and the two hint
labels, registered in the same order, the way `bench/linesweeper_frame_bench.cpp`
already does. The hint labels' text is the one thing copied from
`play_state.cpp`. If that file changes them, change `main.cpp` to match. The pause screen is
the sample's own `PauseState`, built once and drawn over the frame. It needs
only the shell's services, so the tool constructs an `Application`, initialises
it on a hidden window, loads the sample's manifest and never calls `run()`.

Every tick, the scene is updated once with a fixed `dt` of 1/60 s, exactly as
`PlayState::update` does at the sample's pinned 60 ticks a second. The particle
field draws from its own counter-based random stream, which starts at zero, so
the sparks repeat as exactly as the match does.

## Building and running

The tool needs a device, so it is not a ctest entry. It is built in every
preset with a rasterising backend. The `-null` presets do not build it at all,
because the null backend refuses `read_back_buffer`. Use the D3D11 Release
build for the website's images:

```
cmake --preset x64-release
cmake --build --preset x64-release --target LineSweeperCapture
out\build\x64-release\tools\linesweeper_capture\LineSweeperCapture.exe website\src\assets\captures
```

```
LineSweeperCapture <output-directory> [<script>]
```

The output directory is created if it is missing. The script defaults to
[website.txt](website.txt) in this folder, read from the source tree rather
than from a copy. An edit to it takes effect on the next run with no rebuild.
The tool prints the device it drew on, then one line per capture: the tick,
the score, lines, level and the number of live particles. It exits non-zero
with a message naming the script line when anything is not as the script says.

The images are 1280x720, the size both samples ask for. The tool refuses to run
if the back buffer is any other size.

## The script

One command a line. `#` starts a comment.

| Command | What it does |
|---|---|
| `place <piece> <turns> <column>` | `steer`, then `drop` |
| `steer <piece> <turns> <column>` | Turn the falling piece clockwise `<turns>` times (0-3) from its spawn orientation, then slide it until its leftmost cell is in `<column>` (0-9) |
| `drop` | Hard drop the falling piece |
| `hold <piece>` | Put the falling piece in the hold slot |
| `wait <ticks>` | Let ticks pass with nothing held |
| `capture <file> [paused]` | Draw the current frame and write it to `<output-directory>/<file>`. `paused` draws the pause menu over it |

Pieces are `i j l o s t z`. Every command becomes the bytes `tick()` takes,
one byte a tick. A press is one tick with the button down and the next tick
with it up, which is what a key held for one frame produces. So every turn,
slide step, drop and hold is two ticks, and the sparks move on each of them. Three turns are made as one anticlockwise press. A piece is dealt, when
none is falling, by one tick with nothing held.

`place`, `steer` and `hold` name the piece they expect to be falling, and the
tool checks it. A script that has drifted from the deal fails on the line that
drifted. Otherwise it would quietly draw a different game. The tool also
refuses a slide the well blocks, a piece that locks before it is steered,
a hold of a piece that has just come out of the hold slot, any command after
the match is over, and
`capture ... paused` over a finished match. The sample never opens its menu
there.

### Changing it

Edit [website.txt](website.txt) and run the tool. Its section comments say what
each part of the match is for. To move a capture earlier or later, change the
`wait` before it. A burst's particles are integrated on the tick that threw
them, so `drop` followed by `wait 6` reads back the eighth frame of that
burst. Changing a placement changes the rest of the match, and the tool says
on which line the script stops matching it.

The deal is fixed by the rules' seed (`World::rng`, zero from `World{}`). For
that seed it begins `s i o t z l j, j i s l t z o, l j i z o s t, j l o t i s
z`. Holding a piece changes which piece is falling, not what is dealt.

The current script was found by running the rules with no device: a
throwaway greedy planner placed each piece, and every candidate was checked by
replaying the script through `ScriptedMatch`. `ScriptedMatch` and the script
reader need only the rules library, not the engine, so a short console program
can do the same again.

## Repeatability

The output is meant to be byte-for-byte repeatable on one machine and one
backend. That has been checked on an NVIDIA GeForce RTX 5080. Repeated runs of
the x64-release D3D11 build wrote four files with identical SHA-256 hashes, and
the x64-debug build wrote the same four files. The checked-in images are from
x64-release on D3D11.

The other backends were run on the same machine once each. D3D12 and Vulkan
wrote the same bytes as D3D11 for all four images. GL wrote the same bytes for
the play and clear images. It differed on two text rows, the pause menu's
RESTART and the top-out banner: 149 and 388 pixels, by up to 125 and 240 in a
channel. Both strings are centred by measuring them, and the likely cause is a
half-pixel position that GL samples differently. That was not investigated.

It is not a claim across machines. A regeneration on other hardware may change
some bytes. Look at every image a regeneration changes before committing it.

The PNGs are RGB with no alpha channel. The back buffer's alpha is whatever
the blend left there, and a screenshot is what the display shows.
