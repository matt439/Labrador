# EC2 reference-performance runner

This package runs `LineSweeperFrameBench` on one declared Windows EC2 host and
retains the raw evidence. It establishes a repeatable **EC2 reference lane**;
it does not turn an NVIDIA datacentre GPU or Windows Server into Labrador's
consumer minimum specification.

Local packaging, validation, CloudFormation rendering and analysis require
Python 3.10 or newer and otherwise use only the standard library. `boto3` is
imported only by commands that read or change AWS state:

```powershell
python -m pip install -r tools/cloud_performance/requirements.txt
```

## One-time AWS prerequisites

Prepare a Sysprepped, account-owned Windows Server 2022 AMI with:

- the supported NVIDIA GRID driver for the selected instance type;
- Visual Studio Build Tools only if builds will be performed on the image;
- the Vulkan SDK only when the image will build binaries or run the validation
  layers; a prebuilt Vulkan payload needs the installed display driver and
  loader, not the SDK toolchain;
- the Microsoft Visual C++ 2015–2022 x64 Redistributable — the staged binaries
  import `MSVCP140.dll`, `VCRUNTIME140.dll` and `VCRUNTIME140_1.dll`;
- current EC2Launch v2, SSM Agent, `AWS.Tools.S3` and
  `AWS.Tools.SecurityToken` for Windows PowerShell 5.1, with both modules
  installed for `AllUsers` so the SSM `SYSTEM` process can import them — and
  the legacy monolithic `AWSPowerShell` module removed, because the worker
  relies on command auto-loading and two modules exporting `Write-S3Object`
  make that ambiguous;
- **a local, non-administrator user that Windows logs on to the console
  automatically**, named in the image's `ConsoleUser` tag. The benchmark
  cannot run where the worker runs: SSM executes the worker as `SYSTEM` in
  session 0, whose window station has no display, and there DXGI refuses a
  swap chain with `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE` — so neither Direct3D
  backend can start — while OpenGL and Vulkan start and present into nothing
  at a throttled rate. The worker starts every repetition in that user's
  console session through a scheduled task with an interactive logon type,
  which is what a player's game gets. Write the logon into EC2Launch v2's
  Sysprep answer file as `<AutoLogon>` rather than into Winlogon's registry
  values: generalisation strips those, and the first image built here came
  back with nobody on the console. Setup keeps the answer file's password as
  an LSA secret, and Winlogon prefers a registry `DefaultPassword` to that
  secret when one exists — the second image failed its logon on a stale
  registry value, so make sure none is left behind. The account's password
  is only ever used by Winlogon; nothing in this lane needs it afterwards;
- **the Nitro "Microsoft Basic Display Adapter" disabled**, so the NVIDIA
  adapter owns the only display. With both enabled the basic adapter's
  1024x768 phantom monitor is primary: a window placed by default lands on
  it, every Direct3D frame is copied across adapters through DWM at roughly
  125 ms each, and WGL binds the GDI fallback instead of the NVIDIA ICD;
- no source, AWS credentials or previous benchmark evidence.

The supplied profile uses `g6f.2xlarge` with four cores and SMT disabled, plus
one quarter of an NVIDIA L4. Check the current AWS
[accelerated-instance specification](https://docs.aws.amazon.com/ec2/latest/instancetypes/ac.html),
[valid CPU options](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/cpu-options-supported-instances-values.html)
and [GRID driver requirements](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/nvidia-GRID-driver.html)
when qualifying or replacing the image.

Tag it `Project=Labrador`, `ImageRole=performance-runner-v1`, `ConsoleUser`
with the auto-logon account's name, and with the exact numeric `WindowsBuild`
and `NvidiaDriver` that qualification observed. The worker checks the live OS,
driver and console session against those tags. Qualify that the four raster
backends produce `hardware_raster` results on the NVIDIA adapter **when a
scheduled task started from the SSM service session runs them in the console
session of an instance launched from the finished image** — Sysprep is the
step most likely to undo the auto-logon or re-enable the basic adapter, so a
builder that passed before imaging proves nothing about the image. The
deployment rejects an AMI that is not owned, available, x86-64, Windows,
encrypted and tagged as declared.

Supply an existing same-Region S3 bucket with default SSE-S3 encryption,
versioning and all four
public-access blocks enabled. The run stack creates no NAT gateway and opens no
inbound port. A public subnet with an ephemeral public IPv4 address is the
cheapest simple route to the required services. A private subnet instead needs
working egress or endpoints for S3, regional STS, `ssm`, `ssmmessages` and —
where the AMI/Region still uses it — `ec2messages`. Storage, requests and public
IPv4 are outside the configuration's compute-only ceiling.

Before the first run, check the regional G/VT On-Demand vCPU quota and confirm
the declared instance type is offered in the selected Availability Zone.

## Build and bundle

Build each selected backend in Release and stage the complete `bench` output,
including the copied content beside the executable:

```powershell
$ErrorActionPreference = 'Stop'
cmake --preset x64-release
if ($LASTEXITCODE) { throw 'x64-release configure failed' }
cmake --build --preset x64-release --target LineSweeperFrameBench
if ($LASTEXITCODE) { throw 'x64-release build failed' }
cmake --preset x64-release-d3d12
if ($LASTEXITCODE) { throw 'x64-release-d3d12 configure failed' }
cmake --build --preset x64-release-d3d12 --target LineSweeperFrameBench
if ($LASTEXITCODE) { throw 'x64-release-d3d12 build failed' }
cmake --preset x64-release-gl
if ($LASTEXITCODE) { throw 'x64-release-gl configure failed' }
cmake --build --preset x64-release-gl --target LineSweeperFrameBench
if ($LASTEXITCODE) { throw 'x64-release-gl build failed' }
cmake --preset x64-release-vulkan
if ($LASTEXITCODE) { throw 'x64-release-vulkan configure failed' }
cmake --build --preset x64-release-vulkan --target LineSweeperFrameBench
if ($LASTEXITCODE) { throw 'x64-release-vulkan build failed' }

$payload = 'out/cloud/payload'
if (Test-Path -LiteralPath $payload) { throw "Refusing stale payload: $payload" }
$builds = [ordered]@{
    d3d11 = 'out/build/x64-release/bench'
    d3d12 = 'out/build/x64-release-d3d12/bench'
    gl = 'out/build/x64-release-gl/bench'
    vulkan = 'out/build/x64-release-vulkan/bench'
}
foreach ($backend in $builds.Keys) {
    $destination = New-Item -ItemType Directory "$payload/$backend"
    Copy-Item -LiteralPath "$($builds[$backend])/LineSweeperFrameBench.exe" $destination
    Copy-Item -LiteralPath "$($builds[$backend])/fonts" $destination -Recurse
    Copy-Item -LiteralPath "$($builds[$backend])/textures" $destination -Recurse
}

python -m tools.cloud_performance bundle `
    --payload out/cloud/payload `
    --out out/cloud/labrador-performance.zip
```

The bundle includes the staged binaries and a snapshot of current relevant
tracked and untracked source bytes. It records the current commit, dirty state,
per-file hashes, archive hash and release-manifest hash. Copy the two reported
hashes into a copy of `config.example.json`. Never reuse those values after a
source or payload change.

Those hashes bind the source snapshot and payload bytes independently; they do
not prove that an arbitrary prebuilt executable was compiled from that snapshot.
Stage immediately after the checked builds above and retain their logs with the
review. A reproducible or signed build pipeline would be needed to turn that
operating rule into cryptographic compiler provenance.

## Review and launch

Set a near-term absolute UTC deadline, the reviewed hourly price ceiling and
compute allowance. `deadline + two watchdog minutes` must fit the allowance and
must not exceed `max_elapsed_hours`. This is arithmetic plus a best-effort
control-plane watchdog, not an AWS billing hard cap; delayed AWS control-plane
execution and non-compute charges can exceed it. Leave `allow_launch` false
while reviewing.

The example gives each repetition thirty seconds of warm-up and sixty seconds
of retained software-paced frame-start intervals. The worker separately attests
the active display mode and the benchmark retains present-call time; it does not
rename the scheduled interval as scan-out. `whole_frame_ns` is CPU-observed wall
time across update and renderer calls, including any waits they perform, not a
GPU timestamp. Five repetitions over four backends therefore occupy about
thirty minutes before startup and collection overhead.

The benchmark uses a high-resolution Windows waitable timer (Windows 10 1803
or later), with absolute deadlines and immediate catch-up after a late frame.
It does not spin or request a process timer-resolution change. Each retained
frame includes `pacing_wait_ns` and `start_lateness_ns`, outside the whole-frame
work measurement, and the JSON names the pacer and deadline policy. OS scheduling
and renderer waits can still cause late or alternating frame starts. Use mean
interval to derive the realized average rate; reciprocal median is not average
Hz. Older captures lack these two fields and analysis labels them as legacy.

Software pacing against the backends' synchronised present is two clocks, and
on a host whose presentation cadence equals the pacer's rate that is a one-way
ratchet: once a stall fills the present queue, every later wait lands in the
renderer call instead of the pacer, `whole_frame_ns` reads one period for the
rest of the process, and nothing drains it. Both reference runs did this in
five of forty repetitions and the desktop reproduces it on demand;
`docs/performance/2026-09-19-g6f-ratchet.md` is the finding and its §5 says
which clock a reference capture should keep. The analyzer applies that
document's signature: each repetition carries a `presentation_lock` verdict —
`locked_to_presentation_cadence` when, from `tail_first_sample` to the end of
the retained minute, the pacer never waits (under 1 % of a period, from
`pacing_wait_ns` where recorded and otherwise from the gap between one frame's
end and the next start, which cannot see the first sample), the tail lasts at
least a second, and the whole frame and the interval both average the period
within 2 % — and each backend lists its `presentation_locked_repetitions`.
The mean is used rather than the median because an alternating lock never puts
the median at the period. The long-call counts beside the verdict are
unchanged and still count every locked frame; the verdict is what says those
frames are the period. Until the benchmark keeps one clock, read
`record_submit_ns` and `update_ns` as the work, and do not quote a pooled
`whole_frame_ns` across a locked repetition.

The selected device record also carries `present_mode` and nullable requested
and reported swap intervals. GL requires `WGL_EXT_swap_control`, requests one,
and records the getter's answer. Direct3D requests one without a corresponding
query; Vulkan selects FIFO. These values describe API state, not whether a
driver, compositor or virtual display obeyed it. Existing binaries and frozen
bundles retain their old behavior; rebuild and create a new bundle to use these
changes.

```powershell
python -m tools.cloud_performance render `
    --config out/cloud/run.json --out out/cloud/template.json
python -m tools.cloud_performance launch --config out/cloud/run.json
```

Both commands above are local; `launch` prints the exact uploads, instance and
stack without contacting AWS. To launch, set `allow_launch` true and add the
explicit mutation switch:

```powershell
python -m tools.cloud_performance launch --config out/cloud/run.json --execute
```

The command uploads the immutable inputs, creates a new stack (never updates an
old one), waits for its one On-Demand instance to register with SSM, then uses
`AWS-RunRemoteScript` to start `worker.ps1`. SSM is the control plane, not a
self-hosted GitHub runner. The exact canonical CloudFormation template deployed
is uploaded too, copied into the terminal-hashed evidence and retained for
future analysis; old evidence never depends on regenerating a template with
newer tooling.

SSM registration has a five-minute startup allowance. A registration, command
dispatch or launch-record failure terminates the instance immediately; the
identity-checked stack remains for `status`, evidence inspection and `stop`, and
the independent deadline watchdog remains the second stop path.

The worker rechecks its own hash, configuration, bundle, release manifest, AMI,
instance type, Region, Availability Zone, CPU topology, GPU and the console
session — the declared `ConsoleUser` must be logged on there with a desktop
shell. It runs the fixed benchmark repetitions in that session, one scheduled
task each, and uploads configuration, host identity, raw JSON, stdout and
stderr. `success.json` or `failure.json` is uploaded last. It then
initiates OS shutdown; the EC2 setting turns that into termination. An
independent EventBridge/Lambda watchdog terminates the instance at the absolute
deadline even if the worker, SSM or invoking workstation disappears.

## Inspect, collect and stop

```powershell
python -m tools.cloud_performance status `
    --stack labrador-performance-RUN_ID --region ap-southeast-2
python -m tools.cloud_performance collect `
    --stack labrador-performance-RUN_ID --region ap-southeast-2 `
    --out out/cloud/RUN_ID
python -m tools.cloud_performance analyze `
    --input out/cloud/RUN_ID --out out/cloud/RUN_ID-analysis.json
```

Analysis refuses to report percentiles unless the terminal marker, launch,
configuration, host, release, adapter, workload and complete repetition matrix
all agree. Recorded presentation metadata must be complete and match the
backend's synchronization contract. A run cannot mix captures with recorded
pacing or presentation policy and legacy captures without it. Entire legacy
runs remain readable and explicitly labelled `unrecorded_legacy`; missing
measurements are never synthesized. Pacing and presentation metadata entered
the producer together, so a capture must include both groups or neither.
Raw samples remain the authority.

Read `repetitions` and `repetition_ranges` before the pooled `summary`.
Per-repetition diagnostics show mean cadence, counts over the frame budget,
short/long intervals, and long begin/present calls including alternation. They
also retain the first and last affected sample and the longest consecutive and
alternating spans, with zero-based sample indices local to each repetition.
An alternating span includes both its long and short samples; ties retain the
first span. Pacing wait and start lateness appear in the pooled summaries and
repetition ranges when recorded, with separate episodes of start lateness over
the target period. These locate a mid-run onset or a persistent late schedule
that a pooled p99 cannot describe.
The long-call threshold is half the declared frame budget and is reported in
the output; it does not prove that a call waited or identify what it waited on.
On Vulkan, begin includes the frame-slot timeline wait and present includes
image acquisition and submission. All repetitions remain in the report,
including a slow first process. A complete artifact is an evidence-integrity
result, not a claim that a timing target passed.

Stopping is also dry-run by default. The preview names the exact requested
stack without contacting AWS; `--execute` then refuses unless that stack's
namespace, tags, outputs and single logical `Runner` all agree. It never scans
broad tags. A Runner already terminated by itself or the watchdog is treated as
fenced — including after EC2 has purged its instance record — so the exact stack
and its minute watchdog can still be deleted:

```powershell
python -m tools.cloud_performance stop `
    --stack labrador-performance-RUN_ID --region ap-southeast-2
python -m tools.cloud_performance stop `
    --stack labrador-performance-RUN_ID --region ap-southeast-2 --execute
```

Collect before deleting the stack. Evidence in the external S3 bucket is not
deleted by `stop` and continues to incur storage charges until retired under
the bucket's reviewed lifecycle policy.

## Offline verification

The tests create no AWS resource and make no network call:

```powershell
python -m unittest tools.tests.test_cloud_performance
```

When `cfn-lint` and AWS credentials are available, also validate a rendered
template with those tools before the first real launch. The first real
acceptance remains a manual image/session qualification plus a bounded run;
passing the offline suite cannot establish GPU availability or price.

## Local GPU trace diagnostics

`trace_benchmark.ps1` captures one bounded benchmark process with the installed
Windows Performance Recorder `GPU` profile. Run it from an elevated PowerShell
in the interactive desktop session. It uses an already extracted, reviewed
bundle, verifies its release hash and every manifest member, and refuses an
existing output directory. It creates no AWS resources.

```powershell
powershell -NoProfile -File tools/cloud_performance/trace_benchmark.ps1 `
    -BundleRoot out/cloud/extracted-bundle `
    -ReleaseSHA256 <the-reviewed-release-manifest-sha256> `
    -Backend vulkan -OutputDirectory out/cloud/local-vulkan-gpu-trace
```

The defaults retain the reference workload: 1,800 warm-up frames, 3,600 measured
frames, software pacing at nominal 60 Hz, and a 210-second benchmark deadline.
For a separately declared pacing experiment, pass `-Pacing presentation` with a
freshly rebuilt bundle and a different output directory. That removes the
software pacer while retaining the backend's configured presentation mode;
`-Refresh` remains the comparison budget. `-Warmup`, `-Sample`, `-Refresh` and
`-TimeoutSeconds` are bounded and recorded; the deadline is at most ten minutes.
Tracing changes the measured workload, so compare these captures as diagnostics,
not as another untraced reference repetition.

Each capture retains the exact arguments and executable hash, release manifest,
WPR status/profile/start/stop logs, benchmark stdout/stderr and raw result, and
`gpu.etl`. `capture.json` records completion or failure and file hashes. A failed
benchmark or its deadline still triggers a trace save. WPR commands have their
own finite deadlines, so trace finalization can take up to two additional minutes.
The wrapper starts a unique named WPR instance and stops only that instance;
it never cancels an existing recording. A failed start does not launch the
benchmark; it still attempts a stop of its own name to clean up any partial
start. If start or cleanup fails, preserve the directory and inspect the
logs; `request.json` contains the exact named stop command for recovery.

The default WPR status is recorded for context, not treated as a complete list
of named recordings. Session ownership uses Microsoft's documented
[`-instancename` isolation](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options#instancename),
with the name last on start and stop. Open the ETL in Windows Performance
Analyzer and correlate the benchmark process's renderer waits with GPU queue,
DXGI/presentation and CPU scheduling events. A successful recording alone does
not establish which dependency caused a stall.

The wrapper's failure and ownership paths can be exercised without administrator
rights or an ETW session using fake subprocesses:

```powershell
python -m unittest tools.tests.test_trace_benchmark
```
