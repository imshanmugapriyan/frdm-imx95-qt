# Acquisition boundary for the M7 integration

LAN-XI uses a separate native Ethernet backend documented in
[LANXI_INTERFACE.md](LANXI_INTERFACE.md). `setAvailableModes()` below reports
M7 capabilities only; `DiagnosticSession` combines these with LAN-XI readiness.
LAN-XI start/stop requests are routed internally and are not emitted to the M7
adapter. Both paths share one acquisition ownership lock.

The user has specified two acquisition paths:

| Diagnostic mode | M7 acquisition responsibility |
| --- | --- |
| `dsp-calibration` / `automatic-mic` | The Acoustic Analytics Studio two-UART path |
| `live-electrodogram` | FlexIO acquisition from the specified AD7606 evaluation hardware |

This document defines an **application-side interface**, not an M7 wire protocol.
UART pins/baud/framing, FlexIO timing, ADC part/configuration, channel mapping,
sample formats and Linux–M7 IPC have not been supplied. No RPMsg, shared-memory,
socket or device-driver implementation is assumed or installed.

`src/services/diagnostics/qml/DiagnosticSession.qml` (object name `diagnosticSession`) is the
integration point. The adapter must marshal callbacks onto the Qt GUI thread;
it must not access QML objects from a capture thread. Firmware acquisition and
lossless raw/WAV recording belong outside the UI update loop.

## Control and ownership

1. Connect the session's `startRequested(ticket, mode, settings)` and
   `stopRequested(ticket)` signals to the acquisition controller.
2. Call `setAvailableModes([...])` only after the entire mode can operate.
   Acoustic mode readiness includes both required sources and the selected
   workflow's output/recording dependencies. Default capability list is empty.
3. A user start snapshots the settings, creates a new integer ticket and enters
   `starting`. Call `started(ticket)` only after the backend has accepted the
   configuration and started acquisition. No frames are accepted before that.
4. A stop or Auto Mic Finish enters `stopping`. Drain remaining data and finalize
   evidence, then call `stopped(ticket)`. Navigation remains locked until this
   acknowledgement. The desktop window's close action follows the same path.
5. Call `fail(ticket, reason)` only when the backend confirms the run is stopped
   or quiesced. Malformed data requests a stop and retains the session lock.
   Losing capabilities also requests stop; it does not silently release hardware.
6. Every callback must carry its original ticket. Late frames/acknowledgements
   from an earlier run are ignored. Never change the meaning of an active ticket.

The UI displays a delayed-acknowledgement message after ten seconds; elapsed
time alone does not acknowledge a start/stop or permit switching acquisition modes.

## Live electrodogram data

The tool's `startRequested` settings use this reference profile:

```javascript
{
    profile: "live-electrodogram-v1",
    plotSource: "STIM_EVALUE", // or "ASK_EVALUE"; a requested logical source
    mcl: 4095, thl: 1, pulseWidth: 40, gain: 1, pps: 500, tolerance: 20,
    ifg: 3, ipg: 8, biPolar: false, baseToApex: false,
    resistances: [ /* exactly 16 integer ohm values; defaults are 2000 */ ]
}
```

These defaults and ranges come from the Java Live tab; see
[LIVE_ELECTRODOGRAM.md](LIVE_ELECTRODOGRAM.md). Graph visibility and point count
are local display settings and are not sent as acquisition channel filters.
The adapter must validate the complete request against the actual hardware path,
accept it before `started(ticket)`, or reject it using `fail(ticket, reason)`
once quiescent. Do not silently ignore unsupported settings or label a different
source as the requested source. The new UI has no mid-capture signal-settings
update command. Pulse Width, IFG and IPG were UI-only values in the old live
processor and require an explicit hardware definition before they can be applied.

`receiveElectrodogram(ticket, batch)` accepts:

```javascript
{
    sequence: 0,       // Sequence of the first row; monotonically increasing.
    elapsedMs: 0,      // Monotonic batch timestamp, not a fabricated per-row time.
    // optional: plotSource: "STIM_EVALUE"; must match the accepted start settings
    rows: [ /* each row has exactly 16 finite numbers or nulls */ ]
}
```

Rows represent mapped, normalized electrode amplitudes suitable for the live
plot. They are **not raw AD7606 codes**, legacy STIM/ASK bytes or a statement
about the ADC's number of physical inputs. The adapter must define and validate
that conversion. Use `null` for unavailable electrodes rather than inventing
zero measurements. Display magnitudes are bounded to 0..1 as in the reference;
CSV preserves the supplied signed numeric values. A batch may contain 1..8000
rows. Sequences count rows; gaps are counted and duplicate/reordered batches
are rejected atomically. Sequence numbers must fit JavaScript's safe integers.
An explicitly supplied mismatched `plotSource` also rejects the whole batch.
When omitted, source identity is determined by the configuration accepted for
the ticket. Do not mix sources in one ticket. The UI never computes legacy
STIM/ASK E-values, resistance/gain normalization or bipolar packet selection
from already normalized input.

The displayed x-axis uses accepted-row ordinals, not transport sequence numbers
or an inferred sample clock. The service keeps the displayed total with the
displayed rows so that pausing across ring-buffer rollover preserves the axis
origin. Live source/configuration and batch-time metadata survive visits to
another tool and reset on the next live start.

Recording writes accepted rows even while the display is paused. CSV timestamp
column `batch_elapsed_ms` deliberately identifies the timestamp's granularity.
Lossless original ADC recording and accurate sample timestamps belong to the
acquisition service when its data contract is settled.

## DSP observations

`receiveDsp(ticket, observation)` accepts:

```javascript
{
    sequence: 0, band: 1, frequencyHz: 1000,
    measuredY: 316.227766, measuredSpl: 60,
    // optional: appliedLevelDb: actual stimulus setting used for this capture,
    // optional: yValues: [ ... decoded electrical Y observations ... ],
    // optional: electrodeRows: [ ... 16-column normalized display rows ... ]
}
```

The values above illustrate the schema; the application does not generate them.
`measuredSpl` must be a calibrated acoustic measurement. If `yValues` is supplied,
the reference exponential averaging calculation produces measured Y using the
session's frame/rate/time-constant settings. Those timing settings must match the
new decoded data. A measured Y must not be a raw ADC value with an unrelated unit.

The UI emits `calibrationEvaluated(ticket, evaluation)` with target, absolute
error, convergence, EQ coefficient, iteration number and controller decision.
One observation represents one completed measurement iteration, not a preview frame.
Each band keeps its own iteration/controller history; its frequency must stay
fixed within a session. The new profile defaults match the reference adaptive
controller: P 0.8, I 0.05, D 0, correction 0.01..15 dB, integral limit 10 dB,
stimulus 30..110 dB and 30 iterations per band. Zero iterations means unlimited.

`appliedLevelDb` is required for an adaptive decision and must be the actual
stimulus setting used to obtain that observation. It must never be filled from
`measuredSpl`. Without it, measurement analysis remains available but no next
stimulus is requested. `nextLevelDb` is null on convergence, during stopping,
without an applied setting, or at the iteration limit. `decision` contains
the strategy, P/I/D terms and limit/bracket audit data; the logarithmic error
is null for zero-signal recovery because zero cannot be represented in dB.

Reaching the iteration limit without convergence requests stop; the normal stop
acknowledgement is still required. The coordinator must check session ownership
and phase before applying a decision, and must issue no new playback/capture
after a stop request. It remains responsible for active-band configuration,
initial level selection, selecting/playing stimulus, capture timing, retries,
applying `nextLevelDb` and advancing after convergence. Do not advertise
`dsp-calibration` as available until that coordinator is connected and verified.

## Automatic microphone samples

`receiveMicrophone(ticket, block)` accepts:

```javascript
{
    channel: "Front",   // "Front" or "Back"; logical identity, not a UART number.
    sequence: 0,        // Consecutive block sequence per channel.
    elapsedMs: 0,       // Monotonic acquisition time in this session.
    sampleRate: 16000,  // Actual rate supplied by the source, never inferred from baud.
    samples: [ /* finite normalized PCM in [-1, 1] */ ],
    // optional: splOffsetDb: verified calibration offset for this signal path
}
```

Blocks may contain 1..65536 samples with an actual rate in 1000..192000 Hz.
Conversion from signed/unsigned ADC/UART samples and their full-scale meaning
must occur in the adapter. A sample-rate change, sequence gap or stale channel
clears that channel's window/hold; duplicate and out-of-order blocks are errors.
RMS analysis waits for both the requested window and FFT size to be available.
The UI invalidates hold after no new samples for `max(1000 ms, 2 × RMS window)`.

Do not reuse the Java microphone's nominal SPL offset just because it is present
in its source. Supply `splOffsetDb` only when valid for the new signal path; dBFS
remains available without it. Tone validation checks each received channel.
Auto mode prefers Front if present; Both requires both selected channels.
Target achievement is a quality state, not a stop request. Capture continues
until Finish and backend evidence finalization.

Full PCM32 WAV/raw recordings, sample continuity evidence and output-path manifests
remain acquisition-service work. The current native file service writes live
electrodogram CSV, DSP result CSV, Auto Mic measurement summaries and Qt profiles.
Their defaults are the platform's Documents/SunoDiagnostics and application
configuration locations; configure appropriate writable locations for the Yocto service.
