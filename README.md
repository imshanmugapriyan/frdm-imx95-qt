# FRDM i.MX95 Diagnostic Desktop

Qt Quick application for a 1920 x 1080 FRDM i.MX95 diagnostic device, with
Windows and Ubuntu development workflows and a Yocto Linux deployment target.

**Moving to Ubuntu:** start with [README_UBUNTU.md](README_UBUNTU.md) for source
extraction, the existing Linux Qt kit, tests and FRDM SDK build instructions.

The application provides the Suno boot sequence, desktop shell, diagnostic
workspaces and supporting tools. Diagnostic add-ons are **source modules
included in the application build**. They are registered centrally and opened
by a generic host; tool-specific screens do not belong in the desktop shell.

| Item | Current requirement |
| --- | --- |
| Target | NXP FRDM i.MX95, Yocto Linux |
| Display | 1920 x 1080 physical pixels, landscape |
| Application | C++17, Qt Quick/QML, CMake 3.21+ |
| Qt SDK | Company-approved Qt 6.10+ and a matching compiler |
| Executable | `appFRDM_IMX95_QT_DSW` |
| QML module | `FRDM_IMX95_QT_DSW` |

## Source organization

```text
CMakeLists.txt                 Application target and deployment
cmake/                        Source collection and preflight validation
src/
  app/                        C++ entry point and QML application composition
  shell/                      Desktop, navigation, tool host and boot screen
  shared/                     Reusable QML controls, icons and native plotting
  services/                   Diagnostic session/analysis and file services
  tools/
    ToolRegistry.qml          Launcher metadata and diagnostic entry URLs
    Sources.cmake             Explicit list of included tool source manifests
    diagnostics/
      live_electrodogram/     Live Electrodogram entry screen
      acoustic_analytics/    DSP and Automatic Mic Calibration workspaces
      lanxi/                 LAN-XI setup, calibration, stimulus, measurement and review
    utilities/
      notepad/
      calculator/
      results/
      settings/
docs/                         Architecture, extension and integration guides
scripts/                      Source validation and transfer packaging
tests/                        Calculation, session, calculator and routing checks
importedcontent/              Optional design-import staging
build/                        Generated files only; excluded from source packages
```

Each layer and each tool has a `Sources.cmake`. These manifests explicitly list
its QML, JavaScript, native sources and resources. CMake validates the complete
list before loading Qt. The embedded directory structure matches the source
layout, so local QML imports and image URLs remain portable.

Start with [ARCHITECTURE.md](docs/ARCHITECTURE.md). To add a diagnostic, follow
[ADDING_DIAGNOSTIC_TOOL.md](docs/ADDING_DIAGNOSTIC_TOOL.md): create a tool folder,
include its source manifest, and register its entry point. Application startup,
desktop navigation and the tool host need no per-tool routing changes.

## Current behavior

- Boot progress fills from 0% to 100% over four seconds, holds for 400 ms, then
  enters the desktop. This is simulated initialization until services supply
  real progress and readiness.
- The Workspace launcher contains only diagnostic tools. The dock also provides
  Notepad, Calculator, Results and Desktop Settings.
- Live Electrodogram provides a full-width plot with a collapsible settings panel,
  reference signal/resistance settings, STIM/ASK selection, per-electrode scales,
  zoom/history controls, pause and CSV recording. See the
  [Live tool guide](docs/LIVE_ELECTRODOGRAM.md) for settings and acquisition limits.
  Acoustic Analytics Studio contains DSP Calibration and Automatic Mic Calibration.
- LAN-XI Measurement Studio is the third diagnostic in the launcher and dock.
  Its native backend implements HTTP control and TCP input streaming for the
  Type 3676 + Type 3677 pair, saved profiles, microphone calibration, sine,
  stepped-sine and noise stimulus, waveform/RMS/SPL/spectrum, local-reference
  H1 response and coherence, raw recording, replay, and CSV/float-WAV export.
  Both modules retain independent clocks. Qt build/runtime and physical hardware
  acceptance are pending in the approved environment; source checks alone do not
  qualify a measurement instrument. See the [LAN-XI guide](docs/LANXI_INTERFACE.md).
- A diagnostic is loaded on first use and kept alive when hidden so its settings
  and view state survive navigation. Acquisition ownership stays in the shared
  session service. Changing tools or returning home is locked while busy.
- Full workspace view has a compact tool-name header without Suno branding.
  **Exit full screen** restores the desktop bar and dock while keeping the tool
  open. In normal workspace view, **Desktop** returns to the launcher when idle.
- Notepad saves/reopens notes and protects unsaved changes. Results previews
  exported CSV/text files. Desktop appearance and clock preferences persist.
- No M7 transport is implemented. Capture is disabled until an acquisition
  adapter advertises the relevant capability. The planned inputs are two UARTs
  for acoustic work and FlexIO with the supplied AD7606 hardware for electrical
  work. No Java communication code or runtime is included.

The source remains one executable and one QML module. Tool folders are build-time
modules, not separately installed binary plugins. LAN-XI adds **Qt Network** to the
existing Qt dependencies. Its decoder, FFT and tone fitting are original C++17;
no external DSP/parser library, BK Connect binary or Java runtime is bundled.

## Build and transfer

Use the approved Qt environment. Open the **repository-root `CMakeLists.txt`**.
The entry points are now `src/app/main.cpp` and `src/app/qml/Main.qml`.

For an Ubuntu transfer, run `python scripts/package_source.py --ubuntu`.
It produces `build/transfer/FRDM_IMX95_QT_DSW-ubuntu-source.zip` and its SHA-256
checksum. The complete source includes the Ubuntu guide, LF text and executable
shell helpers. Follow [README_UBUNTU.md](README_UBUNTU.md) on the Linux machine.

For this structure migration, extract the complete source ZIP into a fresh
source folder and select a fresh build directory. Do not copy generated build
caches between computers or Qt kits. The application name and data locations
are unchanged, so existing notes, settings and diagnostic exports are preserved.
For Windows MinGW, prefer a short build directory such as `E:\b\suno-debug`:
Qt's generated filenames can exceed Windows path limits under Qt Creator's
long default directory name. See [build-path recovery](docs/BUILDING.md#mingw-cannot-open-a-generated-cppobjd-dependency-file).

```powershell
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<approved-Qt-kit-directory>"
cmake --build build/desktop --parallel
cmake --build build/desktop --target all_qmllint
cmake --install build/desktop --prefix "$PWD/build/desktop/install"
```

Run `build/desktop/install/bin/appFRDM_IMX95_QT_DSW.exe` on Windows. The default
is fullscreen; `--windowed` uses a resizable desktop preview. See
[BUILDING.md](docs/BUILDING.md) for kit setup, deployment, missing-file errors,
QML type information and rendering troubleshooting.

## Checks without a Qt SDK

```powershell
cmake -P cmake/VerifySources.cmake
cmake -P tests/build_paths.test.cmake
python scripts/check_structure.py
node tests/analysis.test.cjs
node tests/live.test.cjs
node tests/desktop.test.cjs
node tests/tools.test.cjs
node tests/lanxi/session.test.cjs
python scripts/package_source.py
```

Node runs development checks only. Python checks source paths and creates the
transfer ZIP at `build/transfer/FRDM_IMX95_QT_DSW-source.zip`. Neither is an
application runtime or CMake build dependency. The ZIP contains the complete
source tree and documentation, without Qt binaries, build caches or IDE settings.

The current source passes 26 diagnostic/session checks, 18 live graph/settings
checks, nine calculator checks, 14 registry/lifecycle checks, source validation
and CMake script mode.
Five build-path checks also cover the reported 260-character dependency path
and its recovery with a short build directory.
LAN-XI adds 11 dependency-free native decoder/analysis/stop-gate checks and six
shared-session checks. A separate Qt backend harness covers paired capture,
generator operation, exports, partial startup, failed stop, connection loss and
truncated recordings against loopback fixtures; it must be built and run in the
approved Qt environment. See [test commands](docs/LANXI_INTERFACE.md#validation).
These checks do not render QML. Qt compilation, `all_qmllint`, native network/file
integration and interactive testing must run in the approved environment.
Use the [desktop validation checklist](docs/DESKTOP_SHELL.md#validation), including
first launch and reopening all three diagnostic tools after changing their settings.

## Display and Linux integration

The UI scales from a 1920 x 1080 logical reference using Qt-managed DPI. Text,
controls and plot geometry render at the current window size. The boot PNG is
native 1920 x 1080 raster artwork; enlarging it cannot create extra image detail.
The desktop bar touches the top edge, and diagnostic plots use the space between
the controls and status line. F10 expands/restores a diagnostic workspace;
Ctrl+Space opens launcher search while idle. F11 toggles the Windows preview.

Fullscreen uses the display mode already configured by the operating system.
Yocto integration still requires the NXP BSP/Qt version, graphics platform,
1920 x 1080 display mode, startup service and boot splash handover. The BSP release
has not been selected and no board deployment has been validated.

- [Architecture and ownership](docs/ARCHITECTURE.md)
- [Adding a diagnostic tool](docs/ADDING_DIAGNOSTIC_TOOL.md)
- [Launcher and registry](docs/LAUNCHER.md)
- [Desktop and supporting tools](docs/DESKTOP_SHELL.md)
- [Java feature migration](docs/JAVA_MIGRATION.md)
- [Live Electrodogram settings and graph](docs/LIVE_ELECTRODOGRAM.md)
- [Third tool: LAN-XI Measurement Studio development plan](docs/LANXI_TOOL_PLAN.md)
- [LAN-XI workbench, processing, orders, structural response and acceptance](docs/LANXI_WORKBENCH.md)
- [LAN-XI source coverage and remaining BK Connect suites](docs/LANXI_FEATURES.md)
- [Full recordings, UFF, multi-mode/ODS and sound power](docs/LANXI_INTERCHANGE_MODAL.md)
- [Acquisition service contract](docs/ACQUISITION_INTERFACE.md)
- [Boot screen source and preview](src/shell/qml/boot/README.md)
- [Future Linux boot handover](docs/BOOT_HANDOFF.md)
