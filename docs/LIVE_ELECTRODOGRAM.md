# Live Electrodogram reference alignment

The tool is implemented in
[`src/tools/diagnostics/live_electrodogram`](../src/tools/diagnostics/live_electrodogram/).
The reference is the **Live Electrodogram tab only** in
`C:\GitHub\Global Repository\electrodogram-analyzer-tool`:

- `src/main/java/com/hcl/electrodogram/ui/LiveWaveformPanel.java`
- `src/main/java/com/hcl/electrodogram/ui/ElectrodogramCanvas.java`
- `src/main/java/com/hcl/electrodogram/service/LiveWaveformStreamProcessor.java`

The Qt layout uses the application's theme. No reference theme, FTDI driver,
Java executable, packet parser or DAC conversion table is packaged.

## Layout and interaction

Capture, pause, recording and plot-source controls share a compact top row.
The plot takes the remaining width and height above the electrode shortcuts
and history bar. Settings opens a 480-unit side panel; closing it returns that
space to the plot. All geometry uses the workspace scale, including when the
desktop bar/dock is restored. The existing compact title and **Exit full screen**
navigation remain in the shell.

The settings panel has **Signal** and **Electrodes** tabs. Save commits an isolated
draft; Cancel, Escape or closing Settings discards it. Save retains settings in
the current application session, as the reference dialog does; it is not a disk
profile export. Acquisition settings are locked during capture. Display-only
electrode visibility can still be saved while capturing.

## Settings parity

| Control | Default | Values / behavior |
| --- | --- | --- |
| Plot source | STIM E-Value | STIM E-Value / ASK E-Value; changing it clears the displayed capture and requires a new capture |
| Visible points | 1000 | 10–8000, steps of 10; zoom changes by 20% with a minimum step of 50 |
| Electrode selection | Enable all | Enable all / Custom; all mode preserves the saved custom mask |
| Reference resistance | 2000 Ω per electrode | 16 independent integer fields, 1–20000 Ω; Up/Down changes by 100 |
| MCL / THL | 4095 / 1 | Integers 1–4095, THL ≤ MCL; invalid combinations block Save |
| Pulse Width | 40 | 8, 10, 20, 40, 60, 80, 100, 200 |
| Gain | 1 | 0.25, 0.5, 0.75, 1, 1.5, 2, 3 |
| PPS | 500 | 250, 500, 1000, 2000 |
| Tolerance | 20 | 10, 20, 30, 40, 50 |
| IFG / IPG | 3 / 8 | Integers 0–99 |
| BiPolar / BaseToApex | Off / Off | Independent toggles |

The original Pulse Width, IFG and IPG controls are absent from its
`buildSettings()` processor arguments. Their presence is not evidence of
hardware behavior. No unit or conversion is inferred for those values.
The Electrodogram/CIS labels identify the **reference profile**, not a detected
configuration of the new board.

## Plot and history

- EL16 is at the top and EL1 at the bottom. Hiding electrodes compacts the lanes.
- Odd/even electrode identities retain cyan/coral colors when other electrodes
  are hidden. These replace the reference blue/red to suit the current theme.
- Vertical stems show absolute normalized amplitude, bounded to 0–1. Each lane
  has 0 and 0.5 ticks, with 1 at the top lane, matching the reference scale layout.
- Peak-preserving pixel bins retain narrow spikes at long time windows. Missing
  values stay missing. Signed supplied values remain unchanged in CSV recording.
- The x-axis counts accepted data rows, starting at zero. The range summary uses
  human-readable row numbers starting at one. Neither is a fabricated time axis.
- Earlier/Later, a draggable history bar, and Follow latest navigate the 8000-row
  buffer. Arrow keys move the focused history bar; Home/End select oldest/latest.
- Manual history stays on the same accepted rows while new data arrives. If those
  rows are overwritten, the view advances to the oldest retained row. Its axis
  continues across rollover instead of restarting at zero.
- Pause freezes both rows and their axis origin. Incoming acquisition and CSV
  recording continue. Resume returns to the current buffer. This intentionally
  retains the existing Qt display-pause behavior instead of dropping input while
  the Java live view is paused.
- The footer shows view range, buffer size, missing row count, latest received
  batch elapsed time, and a local clock. The batch time may advance while the
  displayed snapshot is paused; it describes acquisition, not a per-row timestamp.

All/None shortcuts change graph visibility only. Capture and recording always
retain all 16 mapped columns. With no selected electrodes, the plot explains
how to restore them. Without capture data it shows an empty grid, never a
generated waveform. A source change cannot relabel old STIM data as ASK.

## Acquisition boundary

Signal settings form a versioned request at Start capture; they do **not** apply
the legacy normalization algorithm to incoming normalized rows. The future
adapter must accept the requested source and settings before acknowledging
start, or fail with an explanation if unsupported. Signal changes require a
stopped capture and another start; there is no unacknowledged live reconfiguration.

The M7 FlexIO/AD7606 mapping, impedance/current interpretation, gain conversion,
source definitions, timing and bipolar behavior remain integration work. Selecting
ASK in the UI does not turn ADC samples into decoded ASK packets. See the exact
request and data contract in [ACQUISITION_INTERFACE.md](ACQUISITION_INTERFACE.md).

## Validation

`node tests/live.test.cjs` checks reference defaults/ranges, settings draft isolation,
custom masks, immutable capture requests, electrode order/color identity, null and
signed values, zoom limits, stationary history, buffer rollover, paused origins,
source mismatch rejection, tool switching, acquisition locks and Home/End key
handling through `Keys.onPressed`. These 18 checks exercise JavaScript without
compiling QML; the regression also rejects unsupported Home/End signal handlers. The existing
diagnostic, calculator and registry checks also pass without Qt.

No Qt SDK, compiler or runtime was executed on this laptop. In the approved Qt
environment, build the complete source and run `all_qmllint`, then check:

1. At 1920×1080 and Windows display scaling, open Live Electrodogram, expand with
   F10, and open both settings tabs. The plot, field text, footer, Save and Cancel
   must remain accessible. Test desktop view and a resized window as well.
2. Change a resistance and gain, then Cancel and reopen. Save a second edit and
   verify it survives returning to the desktop and reopening the tool.
3. Enter an out-of-range or empty number, then correct it. Check THL > MCL blocks
   Save, and keyboard Up/Down and Tab navigation work in numeric fields.
4. In Custom mode hide alternating electrodes; confirm fixed colors and EL1 at
   the bottom. Enable all and return to Custom to check mask retention.
5. With an approved test adapter, check 0, 0.5 and 1 amplitudes, isolated peaks,
   nulls and negative samples. Check CSV values are unchanged by display clipping.
6. Stream more than 8000 rows. Pan away from latest, pause, resume, and change the
   point count. Verify view stability, oldest-row clamping and Follow latest.
   Focus the history slider and check Left/Right, Home (oldest) and End (latest).
7. Verify STIM/ASK and signal edits are locked during capture. After stopping,
   change source and confirm old data disappears. An unsupported request must
   fail explicitly; a mismatched-source batch must request stop without recording it.
8. Check DSP and microphone plots after the shared renderer changes. Lane scales
   are opt-in for Live Electrodogram; their own amplitude units remain unchanged.

The fields use Qt Quick's [TextInput](https://doc.qt.io/qt-6/qml-qtquick-textinput.html)
and [IntValidator](https://doc.qt.io/qt-6/qml-qtquick-intvalidator.html). No extra Qt
module, chart package, font, icon set or other runtime dependency was added.
