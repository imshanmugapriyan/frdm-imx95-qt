# LAN-XI workbench

The UI uses a project browser, measurement displays and property inspector
inspired by BK Connect, with original Suno styling. No vendor artwork, binaries
or commercial SDK is included. This describes implemented source, not full
BK Connect equivalence.

## Operation

The left browser selects workflow pages and physical channels. The right pane
shows unit, sensitivity, rate, RMS, averages, quality and generator status. Hide
properties to enlarge the display area. The top commands keep acquisition and
stop controls available. The Measure page has four plots and a channel monitor;
switch to acoustic bands, levels, spectrogram and events from the ribbon.

Save workspace stores the processing recipe, geometry, limits and inspector
visibility in `Documents/SunoDiagnostics/LanXi/workspace.json`. Save profile
stores acquisition settings separately. Loading settings never starts a sequence,
arms a generator or starts acquisition.

## Time data processing

Capture or replay, stop, select a channel and calculate. Advanced time operations
use the final contiguous window, at most 262144 samples (four seconds at 65536 Hz).
Quality changes discard that window. **Entire recording** selects a separate
two-pass, stateful path for complete recordings. Its methods and supported stages
differ from window analysis; see [recording/interchange details](LANXI_INTERCHANGE_MODAL.md).
Ordinary FFT/FRF and level replay also processes the full session.

Stages execute top to bottom: mean removal, gain, second-order Butterworth HP/LP,
central-difference differentiation, band-limited zero-DC integration and analytic
Hilbert envelope. Integration requires a positive cutoff. Finite-window edge
transients apply. Change the output-unit annotation for integration/differentiation;
dimensional units are not inferred automatically.

Correlation removes both means and uses total-energy normalization. Positive lag
means the response follows the reference. Equal-length aligned channels must be
on the same module.

## Order analysis

Select a physical tachometer input on the response module. Set pulses per
revolution, threshold, positive hysteresis, angular samples/revolution, revolutions
per FFT block, maximum order and extracted order. Phase interpolates between
rising edges; windowed-sinc interpolation reduces aliasing during angular
resampling. Qualify missing/extra pulses and within-pulse speed changes using the
actual sensor.

Displays show RMS order spectrum, RPM per block, extracted order and an
energy-averaged RPM/order colour map. Empty RPM bins remain empty; colour is dB
relative to the map peak. CSV preserves each block's time, RPM, order and RMS.
Run-up/down analysis applies within the selected window; long continuous history
requires the future streaming order implementation.

## Structural response

The reference must be calibrated in N. Response units must match accelerance
(`m/s2`), mobility (`m/s`) or receptance (`m`). The first two convert to receptance
before fitting. Choose an isolated resonance band with 8–2048 valid bins meeting
the coherence threshold.

The complex single-pole fitter estimates natural frequency, damping and complex
residue. Results include relative residual, FRAC and synthesized FRF. A high
residual or poor FRAC indicates an unsuitable fit. This isolated-mode page remains
SDOF. The separate **Modal / ODS** page adds seeded common-pole multi-mode fitting,
spectral ODS, XYZ animation and auto-MAC. Polyreference solvers remain absent.
See [methods and limitations](LANXI_INTERCHANGE_MODAL.md).

Edit the four nodes' coordinates; row order defines a polyline. Animation moves
nodes along Z using normalized measured FRFs at the fitted frequency. Only valid
channels with matching response units participate. Drag rotates; the wheel
changes motion scale. This visualization is not a full geometry/DOF or qualified
ODS workflow. Microphone tone calibration accepts V/Pa only; enter calibrated
force/motion sensitivities in the sensor library.

## Limits and repeated tests

Breakpoints hold frequency, lower and upper limits in increasing frequency order.
Limits interpolate linearly. Spectrum is dB re 20 µPa for pressure, otherwise dB
re one engineering unit. Gain is dB of the response/reference ratio; coherence is
dimensionless. Missing bins or incomplete coverage produce INVALID, never PASS.

Start test sequence explicitly uses current acquisition settings, duration and
armed generator settings. Repetitions: 1–100; delay: 0–3600 seconds. Multiple runs
require finite duration or a terminating stepped-sine plan. Each repetition stops
output and closes both modules before acceptance and the next run. Ownership
remains reserved during the interval. Stop cancels remaining repetitions; recovery
or acquisition failure prevents automatic continuation.

`Sequences/<id>.json` records each run and acceptance result. Interrupted records
retain an in-progress marker instead of claiming completion. Capture manifests
remain separate. Stop-on-fail also stops on invalid acceptance.

## Optional audio

QAudioSink playback is implemented but **disabled by default**, avoiding an
implicit multimedia dependency. In the separately approved Qt environment, use
`-DSUNO_LANXI_AUDIO=ON` with its approved Qt Multimedia package. No Qt tool,
installer or runtime was invoked on this laptop.

Select a stopped/replayed channel, set full scale in its unit and playback volume,
then Listen to captured window. The default output must support 48 kHz stereo
PCM16. Band-limited resampling and clipping counts are separate from measurement
calculations. Mute, channel changes and new operations cancel playback and pending
requests. No auto-play or normalization occurs. This is window audition, not a
live monitor or seekable full-recording player.

## Exports and validation

Export result writes `analyses/<timestamp>/result.json` with run/channel, serial,
calibration settings, sample range, recipe and results. Applicable CSVs contain
full-resolution processed samples, order maps and modal fits. Graph previews
are reduced separately.

`scripts/test_lanxi_core.cmd` exercises native algorithms using licensed MSVC,
without Qt. The approved-environment Qt fixture covers acquisition/recovery,
repeated measurements, processing and exports; it has not run here. Qt build,
GUI/audio, physical accuracy and Yocto validation remain pending. The previously
reported Windows startup crash is not diagnosed by this feature work.

See [coverage and remaining suites](LANXI_FEATURES.md).

References: [HBK structural workflow](https://www.bksv.com/doc/bp1523.pdf),
[HBK orders](https://www.bksv.com/doc/bp2583.pdf),
[Qt QAudioSink](https://doc.qt.io/qt-6/qaudiosink.html).
