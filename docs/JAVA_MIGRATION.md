# Java workflow migration

Scope confirmed by the user: Live Electrodogram's **Live Electrodogram tab only**;
Acoustic Analytics Studio's **DSP Calibration and Automatic Mic Calibration first**.
The nine other acoustic workspaces are outside this first migration.

Both tools are native pages in the Suno device home. The supplied Java projects
are reference sources, not executables to launch or dependencies to package.
Their communication code and themes are excluded. Neither source repository
was modified. No Qt tool was executed locally for this update.

## Sources inspected

Acoustic reference root:
`C:\Users\shanmugapriyan.b\Downloads\EQ-Coeff_Calibration_MATLAB_Scripts_V7_0\Java_EQ_Calibration`

Live electrodogram reference root:
`C:\GitHub\Global Repository\electrodogram-analyzer-tool`

| Source | Behavior used in the Qt migration |
| --- | --- |
| `CalibrationFxApp.java`, DSP Calibration workspace | Setup, Channel Monitor, Electrodogram, Results and Event Log views; MCL/THR/IDR and convergence settings |
| `CalibrationProfile.java`, `EqualizationCalibrator.java` | Target Y = sqrt(MCL × THR), midpoint SPL = 30 + IDR/2, convergence error, EQ coefficient = 10^((measured SPL − midpoint SPL)/10) |
| `ExponentialAverager.java` | Filter initial state and retained measurement range preserved explicitly, including the source's unusual first-sample overshoot |
| `AdaptivePidLevelController.java` and its reference tests | Per-band PID, guarded logarithmic interpolation, anti-windup, collapsed-bracket recovery, zero-signal recovery and bounded stimulus corrections |
| `AutomaticMicCalibrationPane.java`, `AutomaticMicCalibrationSettings.java` | Settings/Calibration/Diagnostics pages, Front/Back selection, target, hold, RMS window, stability, hysteresis, tone and clipping checks |
| `TargetRmsGuidanceEngine.java`, `RmsAnalyzer.java` | Scaled RMS, dBFS, stability window, target latch/hysteresis and measured-time hold |
| `AutomaticMicCalibrationSession.java` | Front preference in Auto mode, both streams required in Both mode, stale-sample invalidation, continued acquisition until Finish |
| `LiveWaveformPanel.java`, `RollingElectrodogramBuffer.java`, `ElectrodogramCanvas.java` | 8000-row history, 1000-point initial viewport, electrode selection, pause/resume, zoom/history navigation and 16 stacked amplitude lanes |

## Implemented in this source update

- Native navigation from home and back, with settings retained while visiting home.
- Live view controls and bounded history; missing electrode values remain missing.
  Pause freezes the displayed snapshot while incoming rows can still be recorded.
- Live reference settings, per-electrode resistance, STIM/ASK source selection,
  stable electrode colors, per-lane amplitude ticks, editable point count and a
  draggable history bar. A collapsible settings panel gives the plot the full
  available width when closed. See [LIVE_ELECTRODOGRAM.md](LIVE_ELECTRODOGRAM.md)
  for parity details, deliberate differences and hardware-dependent behavior.
- Live CSV recording writes accepted mapped rows, their sequence numbers and the
  supplied batch timestamp. It does not label ADC codes as current or E-values.
- DSP setup and derived targets, optional electrical averaging, measured-channel
  plots, normalized electrodogram snapshots, converged-band table and CSV export.
- Adaptive DSP controller settings and per-band decisions, with iteration limits.
  Applied stimulus settings remain distinct from measured SPL. The coordinator
  receives the next requested level; no sound is played by the analysis module.
- Auto Mic normalized-PCM RMS, clipping, Hamming/Hann/rectangular FFT, dominant
  frequency check, dual microphone metrics, waveform/spectrum/RMS history plots
  and quality hold. Discontinuities break the RMS curve rather than joining across gaps.
  SPL remains blank unless the acquisition adapter supplies a verified conversion.
- Local JSON profiles and CSV measurement summaries, using Qt Core already
  available through the existing application stack. Profiles are this application's
  format; legacy Java `.properties` and encrypted admin files are not imported.
- Explicit start/stop acknowledgements, session generations, late-data rejection,
  discontinuity/staleness handling and navigation locking during acquisition.

The C++ renderer bins min/max values when needed instead of discarding narrow
peaks. It uses Qt Quick's native painted item and Qt-managed display scaling.
The new plotting and file classes still require compilation and visual testing
with the user's approved Qt kit. The normal application never generates fake
measurement data. Synthetic measurements exist only in tests.

## Hardware-dependent and remaining parity work

This is the first UI and transport-independent processing migration. It is not
an end-to-end hardware calibration release or a complete port of every Java feature.

| Area | Remaining work |
| --- | --- |
| Acoustic acquisition | M7 ownership of the two UARTs, decoding, clock/rate metadata, UART role assignment and Linux delivery |
| Live acquisition | M7 FlexIO/AD7606 capture, ADC transfer function, channel-to-electrode mapping, synchronization and conversion into display quantities |
| DSP run automation | Active-band/workbook import, stimulus generation/playback, synchronized capture/retry scheduling and end-to-end coordinator that applies the implemented PID decisions |
| Auto Mic evidence | Full PCM32 WAV capture, raw-source recording and final evidence manifest in the acquisition/recording service; current native UI exports measurement summaries |
| Additional reference features | Live audio monitoring, logarithmic spectrum axis, full Java spectrum algorithm/window parity and Excel reports |
| Configuration/access | A device account/settings policy; Windows username allowlists, Java encryption keys and administrator files were not copied |
| Board operation | BSP/display selection, startup service, M7 boot and IPC, writable result/profile locations, throughput and rendering benchmarks |

The UI's Start actions are intentionally disabled while the adapter is absent.
They are not connected to the Java executables or the desktop's serial ports.
The future adapter must advertise a mode only when its entire run path is ready,
including stimulus/capture coordination for DSP calibration and evidence recording
for Auto Mic. Signal and metadata boundaries are in [ACQUISITION_INTERFACE.md](ACQUISITION_INTERFACE.md).

The old live parser's STIM/ASK headers, packet timestamps, DAC current tables,
resistance/gain interpretation, bipolar packet skipping and marker counters
depend on the old acquisition hardware. They are not applied to AD7606 samples.
The old Pulse Width/IFG/IPG controls also do not occur in that live processor's
`buildSettings()` arguments, so their labels are not evidence of implemented
capture behavior. Their applicability must be decided with the new measurement path.
The controls are now present as reference settings in the Live tool and are
included in its start request. That does not implement their hardware effects.

## Validation

Run `node tests/analysis.test.cjs` with an existing Node installation. Node is
only a test runner; it is not shipped or needed by the Qt executable.
The 26 checks cover reference calculation values, adaptive controller convergence,
bracketing/level limits, per-band iteration limits, FFT tones, invalid data,
target hold/reset, stale streams, sequence gaps, both-microphone selection,
start/stop acknowledgement, old-session rejection and view pause semantics.
Run `node tests/live.test.cjs` for 18 additional settings and graph-state checks,
including electrode identity, source isolation and history across buffer rollover.

Then build and run the CMake `all_qmllint` target in the approved Qt environment.
Review the three workspaces at 1920 x 1080 and Windows DPI scaling. Check
profile save/load, exports, display resize and keyboard/touch access. Test
native file failures (unwritable results folder/full disk) before using recordings.

UI-thread JavaScript analysis is currently a reference implementation. Before
streaming at board rates, bound adapter update sizes, retain lossless recording
off the display thread and measure CPU, memory and frame timing. Move sustained
analysis to native workers where those measurements require it. The 16 display
electrodes do not imply a particular number of physical ADC inputs.

Qt's [painted item API](https://doc.qt.io/qt-6/qquickpainteditem.html) is part of
Qt Quick. No charting, serial, Java, font or icon dependency was introduced.
