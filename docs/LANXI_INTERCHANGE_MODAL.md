# Recording, interchange and structural workflows

Implemented source, 18 October 2026. These additions use original C++/QML and
introduce no vendor SDK, third-party DSP library or mandatory Qt dependency.
They extend Measurement Studio; full BK Connect equivalence is not asserted.

## Entire recordings

In **Time data processing**, enable **Entire recording**, choose a complete saved
run and select the module/channel. Processing executes in the worker thread;
Stop cancels it. A first pass validates the full selected channel and measures
its mean. A second pass applies the recipe with persistent filter state. Both
passes hash all stream segments of that module; a changed source invalidates the
result. Timestamps, rate, units, scaling, quality and final framing are checked.
Gaps, overload, lost samples and incomplete recordings are rejected here. Existing
valid-prefix replay/export is still available separately.

Captured data retains its acquisition settings when a different profile is loaded.
Inspecting a new hardware setup clears the active capture preview so old samples
cannot be attributed to newly inspected module identities; saved runs stay on disk.

Supported stages are whole-record mean removal (first stage only), gain,
second-order Butterworth high/low-pass, backward-difference differentiation and
bilinear leaky integration `H(s) = 1 / (s + 2*pi*cutoff)`. Filters start with zero
state; differentiation starts with zero output. Startup transients are retained.
These causal differentiation/integration methods differ from window analysis.
Envelope and correlation remain window-only. Output units are user annotations;
the default is the recorded unit, which must be changed when appropriate.

`analyses/<UUID>/` contains every sample in `processed.csv` and `processed.uff`,
plus `result.json` with the source manifest, hash, recipe, statistics and outcome.
RMS/peak include the final partial FFT block. The spectrum averages complete,
non-overlapping 4096-sample Hann blocks and records its omitted tail count.
The waveform preview uses 1024 min/max bins. Memory is bounded independently of
recording duration. Disk space and UFF's ten-character count field limit exports.
Failed/cancelled result records never declare completion.

## UFF interchange

**UFF interchange** accepts a local absolute file path. Select a function to
inspect its magnitude/value, phase and response/reference metadata. Export the
selected captured/replayed channel as complex FRF, RMS auto spectrum or PSD with
editable nodes/directions. Invalid FRF bins are omitted, with their remaining
frequencies exported explicitly rather than inventing missing values.

- Dataset 58 ASCII: all eight real/complex, single/double and even/uneven layouts.
  ID records, axis characteristics, unit labels, entities and DOFs are preserved.
  E and Fortran D exponents are accepted.
- Dataset 164: SI/non-SI unit-system detection. No conversion is performed.
  Non-SI global units prevent structural fitting and standalone function export.
- Other ASCII datasets, including geometry, are listed as skipped. Binary 58b
  is rejected. Re-exporting a function does not export the skipped datasets.

Import limits: 256 MiB, 256 functions and **1,048,576 values total**. This is a
bounded function workspace; a whole-record UFF export can exceed its import
limit. Graphs decimate to 4096 points and use point-index axes, while calculation
and export retain original coordinates. UFF's E13.5 abscissa fields impose format
precision limits. Qualification against real third-party exports is pending.

Native `.bkc`, `.pti`, `.BKConnectProject` and `.BKConnectTemplate` imports are
not implemented. They need a documented HBK route or an approved converter and
representative files; UFF support does not imply native vendor compatibility.

## Multi-mode analysis and ODS

**Modal / ODS** adds joint fitting alongside the existing isolated-mode page.
Supply 1–6 increasing resonance seeds inside a band of 32–2048 retained frequency
lines. Up to 32 response FRFs share natural frequency and damping parameters.
The common-pole receptance model solves complex residues using scaled,
re-orthogonalized QR and refines parameters using bounded, damped variable
projection. Optional constant and inverse-frequency-squared residuals represent
out-of-band contributions. Seed searches are bounded by adjacent seed midpoints;
poor seeds can miss modes.

Captured/replayed fits use calibrated motion channels and an N reference on one
module. Imported inputs require complex Hz-domain FRFs labelled `m`, `m/s` or
`m/s2` over `N`. Mobility/accelerance convert to receptance. Including compatible
imported FRFs requires matching reference entity/node/direction and frequency
grid. Global unit conversion is never guessed.

Results contain frequency, damping, convergence, per-response fit error, FRAC,
complex residue shapes and auto-MAC. Numerical fit quality does not establish a
physically correct modal model. One response DOF cannot distinguish shapes using
MAC. A non-converged result stays visibly marked. JSON and reconstructed UFF
functions save automatically with source identity and settings.

Geometry supports global XYZ coordinates and signed translational DOF animation;
scalar/rotational DOFs are not animated. The wireframe joins nodes in row order.
Save workspace retains geometry and fit settings. This is a small test-geometry
editor, not an FE mesh importer.

Spectral ODS combines measured response RMS with cross-spectrum phase relative
to a common local reference, converted to displacement at the nearest FFT line.
Every included motion channel must meet the coherence threshold. This is an
operating shape, not an identified mode. Imported FRFs are not substituted for
operating measurements. The two modules' independent clocks are never merged.

Still absent: polyreference/ERA, stabilization diagrams, automatic model-order
selection, hammer/double-hit qualification, time ODS, FE/geometry interchange and
comparison with an external mode-shape set.

## Surface sound power

**Surface sound power** accepts paired source-on/background Leq and representative
surface area per position, plus environmental correction K2. Readings can be
entered or copied from a stopped/replayed calibrated Pa channel using matching
A/C/Z weighting. Copied values retain run/channel/weighting/duration metadata;
manual entries are labelled manual. Changing weighting clears readings.

The calculation averages linear pressure energies using area weights, subtracts
averaged background energy and K2, then adds `10*log10(total area / 1 m²)`.
It enforces a configurable minimum background separation of at least 6 dB and
rejects missing readings, invalid areas and source-on readings at/below background.
JSON and position CSV save automatically; Save workspace retains the worksheet.

This is an **engineering estimate**, not an ISO-qualified measurement system.
It assumes representative samples of an enveloping surface and stationary
operation. It does not prescribe standard microphone positions, measure K2,
estimate uncertainty or qualify calibration, environment or fixture conditions.
Band-by-band sound power, loudness/sharpness/roughness, room-acoustic procedures,
beamforming and source identification remain separate work.

## Validation

`scripts/test_lanxi_core.cmd` uses the licensed Visual Studio compiler. New native
checks cover independently assembled UFF layouts, metadata, Fortran exponents,
unit-system guards, malformed input/cancellation, 1.2-million-sample streaming,
filter chunk continuity, two nearby resonances with three complex response shapes,
damping/FRAC and sound-power arithmetic/rejection.

The optional Qt integration harness now exercises complete recording processing,
UFF import/re-export, imported modal fitting and sound-power export. It has not
run on this laptop. Qt build/UI/hardware/Yocto verification remains in the approved
environment. The earlier startup crash remains undiagnosed; these additions are
not presented as a crash fix.

## References

- [UC-SDRL UFF catalogue](https://www.ceas3.uc.edu/sdrluff/)
- [UMass Lowell modal report and UFF 58 specification, Appendix E](https://www.uml.edu/docs/DOE_DE-EE0001374_AppendixL_tcm18-139830.pdf)
- [Dataset 164 published structure](https://pyuff.readthedocs.io/en/stable/_modules/pyuff/datasets/dataset_164.html)
- [HBK modal analysis scope](https://www.bksv.com/doc/bp1523.pdf)
- [HBK sound-power workflows](https://www.bksv.com/media/doc/Bp2093.pdf)

These informed file structures and measurement concepts. No software
implementation from those projects was copied.
