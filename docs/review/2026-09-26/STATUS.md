# September 26 review status

The findings in [README.md](README.md) describe revision
`03c3b96f58345191a340b7db664401510a09c1d4`. This file is the live ledger.
The review itself made no product changes. All 23 findings are implemented
and verified by the follow-up based on `cbe3dad`. [FIXES.md](FIXES.md) maps
each change to its regressions and records
compatibility changes, all 138 passing CTest entries across ten presets, 50
offline tooling tests, Unicode installation checks, and Vulkan synchronization
validation. The resolution commit is the commit introducing `FIXES.md`, titled
`Fix all September 26 review findings`.

The [extended review](SUPPLEMENT.md) rechecked these findings at `0cef654`
(unchanged production source), expanded validation to all ten presets, and
added R26-16 through R26-23. Its reproduction evidence is in
[EXTENDED_EVIDENCE.md](EXTENDED_EVIDENCE.md).

Update each row in the same commit as its resolution. Record the commit and
regression evidence, or a reasoned rejection/deferment with a re-entry condition.
Do not rewrite the historical finding to make it appear never to have existed.

| ID | Priority | Status | Verified closure (details in FIXES.md) |
|---|---|---|---|
| R26-01 | P1 | Implemented and verified | Nested update/init/activation callbacks defer destruction to the outer safe boundary |
| R26-02 | P2 | Implemented and verified | Explicit contact lifetime; retired and surviving participant cases |
| R26-03 | P2 | Implemented and verified | Scene retains fractional sprites under translated/zoomed cameras |
| R26-04 | P2 | Implemented and verified | Transformed UI drawn extents, setters, and container/camera interaction |
| R26-05 | P2 | Implemented and verified | Thin separated shapes rejected by direct and full collision queries |
| R26-06 | P2 | Implemented and verified | Recorded geometry preserves a non-square rotated rectangle and pivot |
| R26-07 | P2 | Implemented and verified | Documented physical-key mapping holds across representative layouts |
| R26-08 | P2 | Implemented and verified | Capture cancellation/transfer clears stuck state without false normal releases |
| R26-09 | P2 | Implemented and verified | DDS/spritefont overflow inputs rejected before unsafe layout arithmetic |
| R26-10 | P2 | Implemented and verified | Oversized valid texture refused before invalid Vulkan image creation |
| R26-11 | P2 | Implemented and verified | Asset-only changes refresh deployed bytes; missing outputs are restored |
| R26-12 | P2 | Implemented and verified | ConsoleUser omissions refused locally; owned early worker failures clean up |
| R26-13 | P2 | Implemented and verified | Nonfinite geometry rejection and consistent overload/validator behavior |
| R26-14 | P3 | Implemented and verified | Particle quads enclosed and visible edge views retained |
| R26-15 | P3 | Implemented and verified | JSON strings preserve length; unsupported identifier bytes rejected explicitly |
| R26-16 | P2 | Implemented and verified | Split-screen base and fullscreen overlay both draw in the intended order |
| R26-17 | P2 | Implemented and verified | Action survives group mutation; mutable captured state persists across activations |
| R26-18 | P2 | Implemented and verified | Same executable loads content in ASCII and non-ANSI installation directories |
| R26-19 | P2 | Implemented and verified | Modern power notifications invoke suspend/resume once with correct minimize interaction |
| R26-20 | P2 | Implemented and verified | Tunnelling contract states valid geometric assumptions and preserves diagonal counterexample |
| R26-21 | P3 | Implemented and verified | Rejected GL texture uploads release allocated objects before retry |
| R26-22 | P2 | Implemented and verified | CPU-work-limited tails do not receive an unsupported presentation-wait explanation |
| R26-23 | P3 | Implemented and verified | Accepted frame-rate options always produce a positive timer period |

All numbered findings are resolved; the verification scope and remaining
hardware/manual coverage limits are recorded in [FIXES.md](FIXES.md). The
README's hardening candidates and coverage limits are not automatically an
implementation backlog.
