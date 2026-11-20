# Browser CI investigation, November 20, 2026

The two recurring browser failure classes were Firefox context setup before navigation and Safari input delivery or event assertions after navigation. Rank by elapsed time of failed browser jobs, counting each attempt once. This measures the cost of the affected jobs including build and earlier passing steps. It cannot isolate wasted compute, billing or the duration of the fault itself.

| Class | Failed jobs | Sum of job durations |
| --- | ---: | ---: |
| Firefox BiDi context setup on Windows | 4 | 40 min 52 s |
| Safari classic WebDriver input on macOS | 7 | 13 min 54 s |

## Cohort

The original run creation window was November 16 at 12:00 UTC inclusive to November 20 at 03:30 UTC exclusive. It includes the v0.4.0 release and the final November 20 integration checks. The all-attempt API read returned 19 build CI runs and 24 attempts in 49 GETs with zero errors or reported data gaps. All 13 failed browser job logs were inspected. Returned wasm jobs comprised 15 passes, five failures, three skips and one cancellation. Safari comprised seven passes, eight failures, eight skips and one cancellation. Each attempt contributes its returned jobs, including partial reruns. To repeat the read from the repository root:

```text
python tools/ci_failure_report.py --repository matt439/Labrador --since 2026-11-16T12:00:00Z --until 2026-11-20T03:30:00Z --attempts all --workflow-path .github/workflows/ci.yml --max-runs 150 --max-requests 350 --format json
```

## Firefox context setup

The Windows Firefox 159.0.1 jobs reached BiDi session creation, then timed out in `browsingContext.getTree` or `browsingContext.create`, or reported a discarded context from create. Individual page CTest processes failed before opening the test URL. The v0.4.0 master run failed two sample entries and passed on attempt two. The November 20 integration encountered the discarded-context error and then two consecutive attempts with setup timeouts. [Release failure](https://github.com/matt439/Labrador/actions/runs/44002989378/job/133771443269), [release retry](https://github.com/matt439/Labrador/actions/runs/44002989378/job/133775345642), [discarded context](https://github.com/matt439/Labrador/actions/runs/44643009642/job/135978817058), [first timeout attempt](https://github.com/matt439/Labrador/actions/runs/44654046165/job/136015860853), [second timeout attempt](https://github.com/matt439/Labrador/actions/runs/44654046165/job/136020380571).

Commit `9561c67d` permits one fresh owned Firefox session after successful teardown for those specific Windows setup errors. Navigation, assertions, page console failures and cleanup errors still terminate the test. Firefox's underlying reason for losing the context remains unknown. The last two wasm jobs passed. [Integration](https://github.com/matt439/Labrador/actions/runs/44659869667/job/136035224662), [master](https://github.com/matt439/Labrador/actions/runs/44664778496/job/136051333787).

## Safari input and fullscreen

Safari 27.2 on macOS reached live pages in all seven jobs. The release-period collision test lost the O key release. November 20 WebWindow failures then moved through Space, wheel routing, a short click and a pending click plus held press. The runner fixes used Safari's virtual Space, isolated wheel sources, released at the current pointer and removed redundant pre-press moves. These are distinct action sequences under the same driver, with no evidence of one browser defect causing them all. [Collision](https://github.com/matt439/Labrador/actions/runs/43968802656/job/133662399035), [Space](https://github.com/matt439/Labrador/actions/runs/44643009642/job/135990847357), [wheel](https://github.com/matt439/Labrador/actions/runs/44647265966/job/135997818410), [short click](https://github.com/matt439/Labrador/actions/runs/44649385430/job/136007285526), [held press](https://github.com/matt439/Labrador/actions/runs/44652200557/job/136013941757).

The last two failures reached fullscreen exit. Step 52 expected one `fullscreen 0` and first received three `mouse_move 10 10` entries before it. The next run received `fullscreen 0`, `keyboard_lost`, `keyboard_gained`. Commits `c2929a2e` and `86ab9ccd` accommodated pointer updates and a balanced focus transition, retaining checks on the exit event, final canvas focus and geometry. The master job passed step 52 at 03:15:52 UTC on November 20. [Pointer trace](https://github.com/matt439/Labrador/actions/runs/44656571476/job/136028281225), [focus trace](https://github.com/matt439/Labrador/actions/runs/44658409826/job/136033308165), [passing Safari job](https://github.com/matt439/Labrador/actions/runs/44664778496/job/136056068258).

Two other failed jobs lay outside these classes. SafariDriver disconnected during bundle session creation before navigation. Chrome failed the LineSweeper pointer page while other readbacks competed for resources; `9f4e8c98` changed scheduling. Neither contributed to the totals above. [Safari setup](https://github.com/matt439/Labrador/actions/runs/43981191139/job/133703585386), [Chromium pointer route](https://github.com/matt439/Labrador/actions/runs/44649385430/job/136000208933).

## Regression evidence and observation window

On Windows the existing wasm-release pages passed four focused CTest entries with the current runner: Chrome 157.0.8103.80 WebWindow and LineSweeper bundle, and Firefox 160.0.1 collision and WebWindow. The two Safari traces also have deterministic replay coverage, including rejection of wrong final focus, geometry, missing or duplicate exit, unmatched focus and unrelated input. Hosted desktop Safari supplies the Safari platform evidence. Physical touch, display pacing and mobile Safari remain separate checks.

Historical source replay injected the hosted Firefox getTree timeout at the protocol boundary. At `9561c67d^` one session closed and the page never ran. At `9561c67d` both sessions closed and the page ran once. A page warning still failed after one session and one page call. The retained local harness runs with `node out/browser-investigation/replay-firefox-local.mjs`. The Safari callback at `9561c67d` rejected the recorded pointer trace, and at `c2929a2e` rejected the focus trace. At `86ab9ccd` the latter trace passed. Both now run in the checked-in fixtures alongside malformed traces and lost final focus. Run the regression suites and real browser routes from the repository root:

```text
node --test tests/render/web/firefox_protocol.test.mjs tests/render/web/safari_protocol.test.mjs
ctest --test-dir out/build/wasm-release -R "^(FirefoxWebWindowTests|WebWindowTests|FirefoxCollisionPageTests|LineSweeperBundlePageTests)$" --output-on-failure
```

The [hosted comparison record](web-to-be-tested.md#browser-comparison-and-window-checks-2026-11-20) closes REV#151. REV#159 stays open for the bundle-session observation window. Review every executed attempt through November 24 at 09:54 Brisbane time, including retries. The November 20 WebWindow failures happened after session creation. Skipped jobs and local replays provide no hosted observation time.
