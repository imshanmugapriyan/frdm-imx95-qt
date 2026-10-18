# LAN-XI Measurement Studio

Implementation status, 18 October 2026: the application includes the native API
and thirteen connected workflow pages, including full-recording processing,
UFF interchange, seeded multi-mode fitting, spectral ODS, a surface sound-power
worksheet, order analysis, acceptance and repeated tests. The redesigned
workbench has a project/channel browser, larger displays and collapsible properties.
See [LANXI_WORKBENCH.md](LANXI_WORKBENCH.md) for operation, optional audio and limits.
See [recording/interchange/modal methods](LANXI_INTERCHANGE_MODAL.md) for the new
operations, file limits and measurement qualification boundaries.
See [LANXI_FEATURES.md](LANXI_FEATURES.md) for exact coverage and unimplemented
BK Connect workflows. Full BK Connect parity is not complete.
This is source implementation pending an approved-kit
Qt build, UI checks and qualification against the installed modules. The native
core and shared-session tests run without Qt. No physical module was exercised
from the development laptop.

## Using the tool

1. Open **LAN-XI Measurement Studio** from Workspace. Choose Acoustic measurement,
   Microphone under test or Amplifier under test. New profiles use V inputs with
   CCLD and polarization off. Enter both module addresses as `http://IP` and select
   **Inspect both modules**. This reads their identity, capabilities and state;
   both must be available and idle. Record firmware/front-panel details before
   qualifying a test chain.
2. On **Setup**, choose each device and physical channel. Configure enabled inputs,
   range, filter, sample rate, engineering unit and sensitivity in V/unit. Values
   such as 50 mV/Pa must be entered as `0.05` V/Pa. A 10:1 voltage attenuator uses
   `0.1` V/V. Set a local measured-reference channel on each module for FRF;
   select None only for capture/level analysis without stimulus.
3. For microphone calibration, connect the known calibrator, set stimulus Off,
   and capture a stable tone. Stop, then use **Calibrate** to enter its level and
   frequency and apply sensitivity to the selected channel. The calculation uses
   the captured channel configuration, requires at least two stable seconds,
   and rejects poor tone fits or quality flags. Save the profile, then verify
   its level with another capture. Manual sensitivity entry is also supported.
4. For excitation, use **Stimulus** to select sine, logarithmic stepped sine or
   band-limited noise. Set the DUT voltage limit, output level and relevant
   frequency/settling values, verify the physical reference and DUT connections,
   then arm output. Setup edits and profile loading disarm it. Starting a page
   or reconnecting never starts excitation. The 3677 generator is configured and
   ramped only after both input streams are receiving valid data.
5. **Start measurement** opens Measure. Select any enabled channel to see its
   waveform, RMS spectrum, local H1 magnitude, and phase or coherence. The channel
   list reports RMS and unweighted SPL for Pa channels. Pause display leaves
   acquisition and recording running. Duration zero means manual stop; stepped
   sine ends after its frequency list. **Output off / Stop** ends the whole run.
6. **Stop** mutes the generator, stops and drains both streams, closes the owned
   recorders and finalizes files. A missing acknowledgement enters Recovery and
   retains the shared session lock. Restore access and select **Retry stop**.
   A timeout is never treated as proof that a remote output is off.
7. On **Review**, select a session to replay or export. Replay loads the recorded
   data through the same decoder and analysis; switch to Measure to inspect it.
   CSV and float WAV are separate per physical channel. Profiles and recordings
   survive application restarts. The desktop Results utility shows a text summary
   pointing to the full recording; replay/export live in this tool's Review page.

The two units are LAN-XI Light devices with independent clocks. Do not connect a
reference on one module and expect sample-synchronous transfer analysis against
the other. Each reference/response pair uses channels on the same unit; a paired
session coordinates control and records both time bases. This follows the
[HBK Open API hardware limitations](https://www.bksv.com/-/media/literature/Product-Data/bp2630.ashx).

## Implemented control boundary

`DiagnosticSession.lanxi` exposes `LanXiBackend`, a GUI-thread proxy. All HTTP,
TCP, processing and storage work runs on `LanXiWorker`'s separate thread. The
application links `Qt6::Network`; no network operation is performed by QML.

| Operation | Contract |
| --- | --- |
| `inspect(settings)` | Read both identities/capabilities; require 3676 with four inputs and 3677 with four inputs/one output, distinct serials and Idle state |
| `session.start("lanxi-measurement", settings)` | Snapshot settings, obtain the shared ticket, recheck identities, configure/start both inputs, then start explicitly armed stimulus |
| `session.finish()` | Stop the active run or cancel inspection/replay/export |
| `outputOff()` | End the active paired run, generator first |
| `recover()` | Retry unresolved stop; never restart acquisition or output |
| `selectChannel(device, channel)` | Device 0/1; physical input 1–4 |
| `pauseDisplay(paused)` | Freeze only the display snapshot |
| `saveProfile`, `loadProfile`, `resetProfile` | Persist versioned settings; new/reset profiles invalidate readiness; saved output is disarmed |
| `calibrate(device, channel, levelDb, frequency)` | Calculate sensitivity from the last stable capture, preserving calibration metadata |
| `refreshRuns`, `review`, `exportRun` | List sessions, replay raw streams and export `csv` or `wav` |
| `processRecording(runId, options)` | Two-pass complete-record validation and stateful processing; immutable source; automatic CSV/UFF/JSON |
| `importUff(path)`, `selectUff(index)`, `exportUff(options)` | Bounded ASCII function workspace; original complex data and metadata; no hardware commands |
| `structuralAnalysis(options)` | Same-reference common-pole fit or captured spectral ODS; independent clock domains retained |
| `soundPower(options)` | Surface pressure worksheet with background/environment corrections; engineering qualification only |

The native proxy reports `state`, `settings`, `busy`, `ready`, `runStarted(ticket)`,
`runEnded(ticket, success, message)` and `recoveryRequired(ticket, message)`.
Opening, inspection, start, stop and replay never clear another backend's
capabilities. GUI command IDs reject queued idle snapshots from an older operation.
Worker generations reject late HTTP callbacks after stop/recovery. Startup needs
valid samples from every enabled input on both modules before its acknowledgement.

| Stage | Implemented HTTP operations under `/rest/rec` |
| --- | --- |
| Identify | `GET /module/info` |
| Open/configure | `PUT /open`, `PUT /create`, `GET /channels/input/default`, `PUT /channels/input` |
| Monitor | `GET /onchange?last=0` |
| Stream | `GET /destination/socket`, TCP connection to returned port, `POST /measurements` |
| Stimulus | `PUT /generator/prepare`, `/generator/output`, `/generator/start`, `/generator/stop` |
| Finish/recover | `PUT /measurements/stop`, `/finish`, `/cancel`, `/close`, according to observed state |

These commands and stream formats come from HBK's
[Open API reference](https://github.com/hbkworld/open-api-tutorials/blob/master/doc/be187215.pdf)
and [single-module acquisition tutorial](https://github.com/hbkworld/open-api-tutorials/blob/master/tutorials/streaming_single_module.md).
The program intentionally opens each Light unit as a separate single-module
recorder. It does not issue PTP synchronization commands. Addresses can be entered
manually or selected from a user-initiated /24 discovery scan. TEDS detection and
reading are available on inspected idle modules. Firmware updating is not included.
Actual firmware responses and generator state transitions still need bench tests.

Settings use schema 1. `devices` has exactly two entries; each has `url`, `serial`,
`sampleRate`, local `reference` (0–4), and four physical-channel entries. Channels
carry `enabled`, `name`, `unit` (V/Pa), `sensitivity`, `range`, `filter`, `ccld` and
`polarization`. Amplifier mode requires voltage units with sensor power off.
Polarization requires the verified UA-3102-041 front panel and consistent settings
on all four channels. Rate/range/filter choices are checked against returned
capabilities before applying configuration.

The generator settings are `mode` (off/sine/steps/noise), `armed`, `levelVp`,
`limitVp`, `frequency`, `startHz`, `stopHz`, `steps`, `settleSeconds`,
`dwellSeconds`, and `rampSeconds`. Frequencies must fit both measurement spans.
The amplitude limit is capped below the 3677's nominal 10 Vpeak full scale;
the software limit does not replace a suitable DUT interface or input attenuator.
Noise uses a full-scale amplitude setting; its measured RMS comes from the local
reference. Hardware output specifications are in the
[Type 3676/3677 data sheet](https://www.bksv.com/doc/bp2586.pdf).

## Decoder and analysis

- The original C++ decoder consumes arbitrary TCP splits/coalescing, interprets
  signed 24-bit samples and applies the reported scale/offset once. It handles
  interpretation and quality messages, validates lengths/types and rejects data
  with missing descriptors, unknown signal IDs, changed interpretation or timing
  discontinuities. No unsupported measurement payload is silently discarded.
- Timestamp family/count values stay in native integers; JSON stores 64-bit
  counts as strings. Differences are computed before floating-point conversion.
  Each device retains its own origin and sample grid.
- Input matching and queues are bounded. HTTP replies are capped at 2 MiB,
  socket receive buffers at 4 MiB, parser payloads at 1 MiB, per-channel unmatched
  samples at 262144, and waveform history at 8192 samples. A stalled/gapped peer
  stops the group. Display snapshots are coalesced to at most 10 updates/second.
- Analysis uses an original radix-2 FFT, selectable windows and cumulative or
  exponential non-overlapping averages. It calculates RMS amplitude spectrum, PSD, overall
  RMS, unweighted SPL (20 µPa reference), H1 magnitude/phase and magnitude-squared
  coherence. FRF requires a local reference, at least two averages and a valid
  per-bin reference floor. Quality flags invalidate affected windows; overruns
  stop live acquisition. There is no resampling between modules.
- Stepped sine uses logarithmic frequencies, settling and sample-count-based
  dwell on both devices. It fits sine/cosine/DC directly, including non-bin-centred
  frequencies. At least two complete windows and a stable, valid reference are
  required. The FFT window may grow to cover four cycles at the lowest step,
  capped at 32768. Completed steps persist with physical channel/reference,
  magnitude, phase, units, reference RMS and fit residual.

## Storage and recovery

Default root: `Documents/SunoDiagnostics/LanXi`. The optional absolute
`SUNO_LANXI_DATA_DIR` variable selects a writable data root for tests or the Yocto
service. When overridden, text summaries stay under its `Summaries` subfolder.
Normal desktop operation does not require an environment variable.

```text
LanXi/
  Profiles/<uuid>.json
  Runs/<uuid>/manifest.json
              3676-0000.stream
              3677-0000.stream
              ...
              exports/<uuid>/manifest.json
                             3676-ch1.csv      (CSV export)
                             3676-ch1-0000.wav (WAV export)
                             ...
```

The atomic manifest contains accepted settings, device information, actual FFT
size, independent timestamps, sample counts, units, calibration, raw segments,
quality events, completed steps and completion/error state. Raw files rotate at
512 MiB and checkpoints update during capture. Segment boundaries may divide a
protocol message; replay retains decoder state across them. A 64 MiB start
reserve is required; failed writes/flushes request paired stop.

An interrupted/partial archive can recover its valid prefix. Replay stops that
device at the first invalid frame or gap and reports the omission; it never
joins samples across missing data. Exports report actual exported counts and
recovery notes. A supposedly complete but corrupt archive produces an error.
Export write failures remain errors, with a partial-export manifest where possible.
Previously finalized segments/files are retained.

CSV includes sample index, seconds from that channel's first sample, engineering
value, unit and quality. WAV is mono IEEE float32 in the recorded engineering
unit, with a `fact` chunk and 512 MiB segmentation; it is not normalized audio
and does not combine modules into a falsely synchronized multichannel file.
The export manifest supplies calibration and clock/quality metadata. Turning raw
recording off retains the run manifest/step results but prevents waveform replay.

## Validation

The Analysis page selects FFT window/averaging, high/low-pass filters, acoustic
weighting, band fraction, harmonic-fit fundamental and event capture. Use
Measure > Acoustics / events for levels, bands, spectrogram and trigger waveform.
Fast/Slow levels operate on calibrated samples before the optional analysis
filters. A/C levels require a sample rate above 2 kHz. FFT-integrated bands below
the available frequency resolution are marked unresolved. No IEC instrument or
filter-class conformance is asserted.

Setup > Network and transducers scans the entered IPv4 /24 on port 80 only when
Find modules is pressed. TEDS uses POST
`/rest/rec/channels/input/all/transducers/detect` followed by GET
`/rest/rec/channels/input/all/transducers`. Applying a sensor changes calibration
metadata, not sensor powering; inspect CCLD/polarization separately. The local
sensor library is stored under `Sensors/` in the existing data root.

Review > Reprocess uses the Analysis page's settings on recorded raw data. Pin
a curve before replaying another session for overlays. Matching frequency grids
and units are required. Export report writes HTML, JSON, spectral CSV and any
completed trigger-event CSV under the source run's `reports/` directory. HTML is
self-contained and printable; JSON retains configuration and recovery provenance.
Choose Batch beside recordings and Batch reports to process them together.
Stop cancels batch work; completed files remain available. Check
`results.json.reportOutcome` for `complete` after any interrupted report write.

Run the original C++ calculations and new analysis suite on this licensed MSVC
installation with `scripts/test_lanxi_core.cmd`. This does not use Qt. The CMake
test target is portable to other approved C++17 toolchains.

Run source/domain checks without Qt:

```powershell
python scripts/check_structure.py
cmake -P cmake/VerifySources.cmake
node tests/lanxi/session.test.cjs
```

The standalone native core target has no Qt dependency. From an approved C++17
compiler environment, configure `tests/lanxi` with `LANXI_QT_TESTS=OFF`, build
and run CTest. The 11 cases cover wire fragmentation/scaling, malformed data,
64-bit clocks, RMS/SPL, FFT/PSD normalization, H1 gain/phase/coherence, weak
references, non-bin-centred tone fitting, logarithmic steps and stop ownership.
They were compiled and passed with the licensed MSVC compiler on this laptop.

In the **approved Qt environment**, using its matching compiler and Ninja on
PATH (the kit's build environment is sufficient), run:

```powershell
cmake -S . -B E:/b/suno-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=E:/Linux/Qt/6.12.0/mingw_64
cmake --build E:/b/suno-debug --parallel
cmake --build E:/b/suno-debug --target all_qmllint
cmake -S tests/lanxi -B E:/b/lanxi-tests -G Ninja -DLANXI_QT_TESTS=ON -DCMAKE_PREFIX_PATH=E:/Linux/Qt/6.12.0/mingw_64
cmake --build E:/b/lanxi-tests --parallel
ctest --test-dir E:/b/lanxi-tests --output-on-failure
```

Adjust SDK paths to the installed kit; keep MinGW build roots short. Python 3.9+
and Qt's runtime DLL directory must be available to the integration test process.
The test harness uses only ephemeral loopback ports and temporary recordings.
It runs the real `LanXiBackend` against original Python fixture modules for
normal capture, stepped sine, second-module start failure, unacknowledged
generator stop, stream disconnection, and a truncated recording with recovery.
It also checks replay and per-channel CSV/WAV structure. The fixture is not
hardware certification and these Qt integration scenarios have not run here.

Before release, qualify all five pages at 1920 × 1080 and resizing/fullscreen,
saved profiles, calibration accuracy, local loopback gain/phase on both units,
stimulus levels/stop states, actual firmware/panels, disconnected cables, partial
startup, stop during startup, disk-full/read-only storage, interrupted recordings,
and one-hour concurrent capture at the agreed rate. Repeat on the selected Yocto
BSP. Board deployment, its image recipe and startup service remain dependent on
the BSP/Qt version that has not yet been selected.

## Dependencies and provenance

The controller, decoder, FFT, fitting code, tests and UI are original project
code, written against the public protocol documentation. No vendor example
implementation, BK Connect software, FFT package, new font or artwork is bundled.
Qt Network is the only newly linked Qt component. Qt build and distribution use
the company's approved licensing route; a Visual Studio licence does not grant
Qt distribution rights. See [Qt Network licensing](https://doc.qt.io/qt-6/qtnetwork-index.html#licenses-and-attributions)
and verify the module's installed Open API access using its firmware/licence page.
