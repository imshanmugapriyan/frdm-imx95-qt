# LAN-XI feature coverage and completion criteria

Updated 18 October 2026. The requested target is a fuller BK Connect-style tool,
with device validation after feature development. This file records actual source
coverage. **Full BK Connect parity is not complete.** No percentage or commercial
product equivalence is asserted. BK Connect is a family of licensed applications;
the exact version and option list have been requested but not supplied.

## Source delivered

| Workflow | Source implementation | Limits that must remain visible |
| --- | --- | --- |
| Paired hardware control | Existing HTTP/TCP controller, capabilities, generator, recording and recovery | 3676/3677 independent clocks; reference/response pairs stay on one module |
| Discovery | User-initiated IPv4 /24 HTTP identity scan, eight requests at a time, bounded responses, cancellation | Port 80; no mDNS, routed-network inventory or firmware management |
| TEDS | Documented detect and transducer-read commands; display serial, sensitivity, units and powering requirements | Requires inspected, idle modules; no automatic powering; supported units: V, Pa, N, m/s2, m/s, m |
| Sensor library | Local persistent named sensor records with serial, sensitivity and calibration metadata | Not a shared enterprise database; no automatic calibration-expiry workflow |
| FFT processing | Hann, Hamming, Blackman, flat-top and rectangular windows; cumulative/exponential averaging; optional mean removal | Non-overlapping blocks; original raw recording retained |
| Analysis filters | Optional second-order Butterworth high-pass then low-pass on spectra and FRF | Fixed processing order; no graphical arbitrary processing-chain editor |
| Levels | Z/A/C, Fast (125 ms), Slow (1 s), equivalent level, maximum Fast and peak calculations | A/C use bilinear digital filters; sample rate must exceed 2 kHz; no IEC instrument-class claim |
| Octave / third-octave | Base-ten band centers, PSD energy integration, frequency weighting and resolution flags | FFT band estimates, not a qualified IEC 61260 filter bank; unresolved bands do not show a level |
| Spectrogram | Last 96 non-overlapping spectral frames, 256 display frequency groups, time coordinates and PSD colour scale | Engineering monitor; reduced display resolution; full-rate raw data is separate |
| Harmonic analysis | Joint least-squares DC/fundamental/harmonic fit, THD, THD+N, SINAD and harmonic amplitudes | Known stationary fundamental; at least four cycles; harmonics limited by span; THD+N spans acquired bandwidth |
| Trigger capture | First rising/falling crossing, hysteresis, pre/post-trigger buffer, sample-coordinate marker and event CSV | One selected channel; independent clock domain; full raw recording continues; no synchronous group trigger |
| Review | Saved-run search, replay, revised analysis settings, pinned spectrum/FRF comparison, plot zoom/pan/value cursor | Overlay requires matching units and frequency grid; one pinned curve; no waveform region editor |
| Reports | Operator/DUT/notes, printable standalone HTML, JSON provenance, per-channel analysis CSV and event CSV | No Office automation or PDF library; inspect reportOutcome in results.json after an interrupted export |
| Batch processing | Select up to 100 recordings; replay and report each using analysis settings; per-run success/errors and cancellation | Recorded raw streams required; processed output does not overwrite original recordings |

Additional source delivered in the workbench expansion:

| Workflow | Implementation | Limits |
| --- | --- | --- |
| Workbench UI | Project/channel browser, compact commands, collapsible properties, four measurement displays, channel monitor and original blue/slate styling | Qt source; interactive validation pending |
| Processing chains | Ordered mean removal, gain, HP/LP, differentiation, band-limited integration and Hilbert envelope; spectrum and CSV | Final contiguous window, maximum 262144 samples; finite-window transients and user-supplied output-unit annotation |
| Correlation | Mean-removed normalized correlation, signed peak delay and coefficient | Aligned channels on one module |
| Orders | Tachometer threshold/hysteresis/PPR, angular sampling, anti-alias interpolation, order spectrum, RPM trace, extracted order, RPM-binned map and CSV | Final analysis window; no full-recording streaming order history |
| Structural response | N reference, motion-unit conversion to receptance, complex single-pole fit, damping, FRAC, synthesized FRF and editable-node animation | Isolated SDOF modes; local-reference normalized FRF visualization, not a qualified modal/ODS suite |
| Acceptance | Frequency breakpoint envelopes for spectrum, H1 gain and coherence; pass/fail/invalid and worst margin | Missing bins or incomplete coverage cannot pass |
| Repeated tests | 1–100 runs, idle interval, selected-channel acceptance, stop-on-fail, cancellation, per-run JSON | Same acquisition settings each repetition; closes both modules and stops output between runs |
| Audio audition | Captured-window playback, full scale, volume, mute, clipping count and resampling | Optional Qt Multimedia build, off by default; no live monitor or full-recording player |
| Persistence | Recipes, limit curves, geometry and inspector visibility in local JSON; result provenance and CSV exports | No vendor project format or shared enterprise catalogue |

Additional source in the recording/interchange expansion:

| Workflow | Implementation | Limits |
| --- | --- | --- |
| Entire-record processing | Two-pass validation/hash, stateful filters, whole-record mean, causal differentiation/leaky integration, CSV/UFF | Complete valid recordings; envelope/correlation/orders remain window-only |
| UFF | Eight ASCII dataset-58 layouts, unit-system detection, metadata and DOFs, captured function export | Bounded import; no binary 58b or geometry; no implicit unit conversion |
| Multi-mode fitting | Joint common-pole model, 1–6 seeded modes, complex residues, fit error, FRAC, auto-MAC, reconstructed UFF | Same-reference FRFs; no polyreference/ERA or stabilization diagrams |
| Spectral ODS | RMS/cross-phase shape, displacement conversion, signed XYZ animation | One local clock/reference; no time ODS or rotational/FE geometry |
| Surface sound power | Area-weighted paired Leq, background subtraction, supplied K2, JSON/CSV | Engineering estimate; no standards/environment/uncertainty qualification |

The pages are **Setup, Calibrate, Stimulus, Analysis, Measure, Processing, Orders,
Structural, Modal / ODS, Limits, Interchange, Sound power and Review**.
See [workbench operation](LANXI_WORKBENCH.md) and
[recording/interchange/modal details](LANXI_INTERCHANGE_MODAL.md).
Profiles save analysis and trigger settings. Existing schema-1 profiles without
these fields retain their previous Hann/cumulative/no-trigger behavior.

## Not implemented — still required before a broader completion claim

| Area | Remaining work / information needed |
| --- | --- |
| Exact parity target | BK Connect version, licensed application/option names, required input/output formats and acceptance cases |
| Order tracking | Full-recording streaming run-up/down history, specialist order filters and qualified tachometer dropout handling |
| Modal / ODS | Polyreference/ERA, stabilization diagrams, hammer rejection, FE/geometry files, external mode-set comparison and qualified time/spectral ODS |
| Specialist acoustics | Qualified sound-power methods/band results, sound quality, source identification and measurement-method/fixture qualification |
| Advanced processing | Branching processing chains, overlap/triggered averaging, arbitrary sequencing/closed-loop branches and multi-channel limit policies |
| Audio | Live monitoring, full-recording seek/loop editing, device selection and optional dependency deployment qualification |
| File interoperability | Native .bkc/.pti and BK Connect project/template formats need a documented or approved interoperability route and representative files; do not invent a compatible format |
| Enterprise workflow | Shared data catalogue, team server, permission model, audit requirements and enterprise report templates |
| More generator functions | Additional documented waveform/sweep/streamed-output modes, repeatable timing and any required closed-loop regulation |
| Target deployment | NXP BSP, approved Qt version, Yocto recipe, startup and measured CPU/memory/rendering/storage budgets |

Cross-module sample synchronization is a hardware limitation, not an unfinished
checkbox: LAN-XI Light Types 3676/3677 cannot provide PTP-synchronous acquisition.
Do not label two concurrently acquired streams as synchronized.

## Verification status

- Existing 11 dependency-free C++ tests and 73 JavaScript domain/session tests
  pass. New dependency-free analysis checks pass with the licensed MSVC compiler.
- New checks exercise frequency-weighting reference points, Fast/Slow step
  responses, Leq, 1 kHz digital-filter normalization, all FFT window gains/PSD
  energy, band resolution and energy, exponential averaging, non-bin-centred
  harmonic fits, trigger fragmentation/re-arming and Butterworth cutoff.
- Advanced checks cover differentiation/integration/envelope, correlation delay,
  stationary and changing-speed orders, angular anti-alias rejection, exact-block
  boundaries, known complex SDOF fitting, MAC properties and invalid/missing limits.
- Source manifests, casing and QML delimiter checks pass. These are not QML
  compilation or runtime tests.
- New native checks cover UFF layouts, unit-system guards, streaming beyond the
  preview length, common-pole multi-mode fitting and surface sound power.
- The Qt integration harness also includes repeated captures, workbench
  processing, processed CSV, invalid/failing limits, full recording/UFF interchange,
  imported modal fitting and sound power. It has **not been run here**.
- The reported Windows startup crash remains undiagnosed. No claim is made that
  these feature changes resolve it.
- Qt build, GUI checks, hardware qualification and Yocto validation are pending.
  No Qt SDK/compiler/runtime has been invoked for this expansion on this laptop.

## References

- [HBK Open API hardware and synchronization limits](https://www.bksv.com/-/media/literature/Product-Data/bp2630.ashx)
- [HBK Open API user guide; transducer commands](https://github.com/hbkworld/open-api-tutorials/blob/master/doc/be187215.pdf)
- [BK Connect order-analysis requirements](https://www.bksv.com/doc/bp2583.pdf)
- [BK Connect modal-analysis requirements](https://www.bksv.com/doc/bp1523.pdf)
- [NTi Audio Fast/Slow time weighting](https://www.nti-audio.com/en/support/know-how/fast-slow-impulse-time-weighting-what-do-they-mean)
- [NI windowing and FFT fundamentals](https://www.ni.com/en/shop/data-acquisition/measurement-fundamentals/analog-fundamentals/understanding-ffts-and-windowing.html)

Calculations and UI are original source. No BK Connect binaries, artwork,
commercial SDK or third-party DSP library was added.
