# September 26 review status

The findings in [README.md](README.md) describe revision
`03c3b96f58345191a340b7db664401510a09c1d4`. This file is the live ledger.
Nothing below has been implemented by the review itself.

The [extended review](SUPPLEMENT.md) rechecked these findings at `0cef654`
(unchanged production source), expanded validation to all ten presets, and
added R26-16 through R26-23. Its reproduction evidence is in
[EXTENDED_EVIDENCE.md](EXTENDED_EVIDENCE.md).

Update each row in the same commit as its resolution. Record the commit and
regression evidence, or a reasoned rejection/deferment with a re-entry condition.
Do not rewrite the historical finding to make it appear never to have existed.

| ID | Priority | Status | Required closure evidence |
|---|---|---|---|
| R26-01 | P1 | Open | Nested update/init/activation callbacks defer destruction to the outer safe boundary |
| R26-02 | P2 | Open | Explicit contact lifetime; retired and surviving participant cases |
| R26-03 | P2 | Open | Scene retains fractional sprites under translated/zoomed cameras |
| R26-04 | P2 | Open | Transformed UI drawn extents, setters, and container/camera interaction |
| R26-05 | P2 | Open | Thin separated shapes rejected by direct and full collision queries |
| R26-06 | P2 | Open | Recorded geometry preserves a non-square rotated rectangle and pivot |
| R26-07 | P2 | Open | Documented physical-key mapping holds across representative layouts |
| R26-08 | P2 | Open | Capture cancellation/transfer clears stuck state without false normal releases |
| R26-09 | P2 | Open | DDS/spritefont overflow inputs rejected before unsafe layout arithmetic |
| R26-10 | P2 | Open | Oversized valid texture refused before invalid Vulkan image creation |
| R26-11 | P2 | Open | Asset-only changes refresh deployed bytes; missing outputs are restored |
| R26-12 | P2 | Open | ConsoleUser omissions refused locally; owned early worker failures clean up |
| R26-13 | P2 | Open | Nonfinite geometry rejection and consistent overload/validator behavior |
| R26-14 | P3 | Open | Particle quads enclosed and visible edge views retained |
| R26-15 | P3 | Open | JSON strings preserve length; unsupported identifier bytes rejected explicitly |
| R26-16 | P2 | Open | Split-screen base and fullscreen overlay both draw in the intended order |
| R26-17 | P2 | Open | Action survives group mutation; mutable captured state persists across activations |
| R26-18 | P2 | Open | Same executable loads content in ASCII and non-ANSI installation directories |
| R26-19 | P2 | Open | Modern power notifications invoke suspend/resume once with correct minimize interaction |
| R26-20 | P2 | Open | Tunnelling contract states valid geometric assumptions and preserves diagonal counterexample |
| R26-21 | P3 | Open | Rejected GL texture uploads release allocated objects before retry |
| R26-22 | P2 | Open | CPU-work-limited tails do not receive an unsupported presentation-wait explanation |
| R26-23 | P3 | Open | Accepted frame-rate options always produce a positive timer period |

Suggested order: R26-01 first; then contact lifetime and common rendering/input
boundaries; then finite-shape and asset/build validation; finally the smaller
presentation/content cases. Group related fixes only when their tests and
contracts remain independently reviewable. The README's hardening candidates
and coverage limits are not automatically an implementation backlog.
