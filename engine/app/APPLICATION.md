# The application shell

Why [`application.h`](application.h) has the shape it does. That file states
what a game can rely on; this one holds the reasons that outgrew a declaration,
cited from it by section number.

It sits beside the code for the reason [`SEAM.md`](../render/SEAM.md) does: a
reader in this folder should trip over it. **It changes by amendment in the same
commit as the change that fights it.**

---

## 1. Threads come from the machine, views from the layout

`default_thread_count` answers how many logical processors the machine reports,
and it sizes the thread pool and nothing else. The shell asks the machine two
questions, and this is one of them; the other is where the executable is
(`content_root.h`). One constant answering for the pool, the renderer's view
capacity and the partition count would conflate three different things.

**The pool's ceiling is a property of the machine**, so `max_threads` defaults
to what the machine reports rather than to a number written in the header. A
fan-out wider than the box has hardware for is slower than not fanning out at
all, and the box least able to absorb that mistake is the one this engine most
wants to run on: a fixed sixteen would make four tasks plus a blocked submitter
out of a four-view frame on a two-core part.

**`view_capacity` is a property of the layout.** Taking it from a thread count
is not merely untidy: every view costs a deferred context and a sprite batch's
dynamic vertex buffer, built eagerly at `create_device` and rebuilt after every
device restore, whether or not a frame ever uses it. A sixteen-thread default
would build sixteen of them to draw one pane, out of the same memory the game
runs in.

`ApplicationOptions::validate` refuses a zero in either count, and in
`target_fps`, before a window opens. `max_threads` reaches `Partitioner` as a
divisor on every frame of every view and `target_fps` reaches `StepTimer` as
one, so a zero would be a hang or a crash on the first frame. `view_capacity`
is the same shape: `create_device` throws below one, and throwing in `validate`
instead names the field rather than the parameter.

## 2. The layout size follows the window

`on_window_size_changed` re-points `ResolutionManager` at the new client size
before the renderer resizes its back buffer, whichever way the size changed:
`set_resolution`, `set_fullscreen`, or a user dragging an edge.

Without that, neither holds. A `set_fullscreen` that restyles to `WS_POPUP` and
shows maximised while the resolution manager still reports the last requested
preset has `ViewportManager` lay out every viewport and divider for 1280x720
inside a 1440p back buffer, and the game renders into the top-left corner of its
own window with the rest cleared black. A dragged window edge is the same door.

## 3. Why the state stack is drained first

The services are borrowed by the states, and `application.h` says the
Application outlives the states it runs. That is true because `~Application`
calls `StateContext::clear()` as its first statement. The states live in the
`StateContext` base and every service is a member, so member destruction would
otherwise run first and each service would be gone by the time the states
holding it were destroyed. That first statement is the whole of what makes a
state safe to release a service in its destructor.

The tidier arrangement is for `StateContext` to be a member declared last rather
than a base, so that the ordering needs no first statement. It is not taken
because it removes `push`, `pop`, `transition_to` and `depth` from every
`Application*`, and PHILOSOPHY batches source breaks. `~StateContext` is virtual
only because it is inherited from.

## 4. Why the members are declared in the order they are

**The audio device first.** What order the audio objects die in is not the
shell's business, which is what the audio seam bought. DirectXTK requires its
`AudioEngine` to outlive every `WaveBank` and `SoundEffectInstance`; that rule
sits beside the library that imposes it, in
`engine/audio/xaudio2/audio_device.cpp`, where another backend can have a
different one without the shell learning it. All the shell has to know is that
the device owns what was opened against it, so it dies last.

**The texture table before the renderer.** The table holds whatever a backend
calls a texture, and on a backend that owns its own synchronisation that is a
resource the GPU may still be reading, an `ID3D12Resource` on D3D12 or a
`VkImage` on Vulkan, whose only wait is in `~Renderer::Impl`. Declared the other
way round, which is the order they are created in, every texture would be
released ahead of that wait on every normal exit, and only those backends would
show it. `render_resources.h` states it as a term of the render seam.

## 5. Why the window callbacks stay on Application

They stay on `Application` rather than moving into `Window` with the rest of
the Win32 code, and that is a choice about layering rather than a constraint:
nothing below the seam needs telling. Publishing a `Renderer::display_changed()`
would free them, at the price of a permanent seam method serving a call that
changes nothing.

`set_input_focus` tells the keyboard and the mouse together, because every
reason to tell one is a reason to tell the other. `settle_presence`, the two
keyboard handlers and `initialize` call it, and the alternative is the same pair
of lines written four times.

## 6. Presence as three facts

The platforms do not agree on which of foreground, shown and has-keyboard
arrive together, so the shell keeps them as three facts and the services follow
from them rather than from whichever message came last. The player is here when
the window is in the foreground and shown: the pads are read and the stack is
told it is active. The keyboard and the mouse need that and the keyboard as
well. So a tab shown again while the canvas does not have the keyboard gives
the pads back and not the keys, and a click beside a canvas takes the keys and
not the pads.
