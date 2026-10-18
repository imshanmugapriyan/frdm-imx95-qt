# Adding a built-in diagnostic tool

New diagnostics go under `src/tools/diagnostics/<tool_name>/`. They are compiled
and packaged with the application. The desktop discovers them from the build's
registry; it does not scan installed folders or launch external executables.

## 1. Create the tool entry point and source manifest

For a tool called Impedance Analysis, use:

```text
src/tools/diagnostics/impedance_analysis/
  Sources.cmake
  qml/
    ImpedanceAnalysis.qml
```

The entry point implements the host contract:

```qml
pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Item {
    id: tool
    required property var session
    property real uiScale: 1

    // Put this tool's actual pages and workflow here.
    // The host supplies width/height; use anchors or responsive layouts.
    // Shared controls receive uiScale: tool.uiScale.
}
```

`Sources.cmake` lists paths relative to the repository root:

```cmake
list(APPEND FRDM_QML_FILES
    src/tools/diagnostics/impedance_analysis/qml/ImpedanceAnalysis.qml
)
```

Add other QML/JavaScript files to this list, native implementation/header files
to `FRDM_NATIVE_SOURCES`, and images or other embedded assets to
`FRDM_RESOURCE_FILES`. Keep assets with their tool and use relative URLs.
Use tool-specific component filenames to avoid collisions in the shared QML
module. `native/` and `assets/` subfolders can be added when needed.

## 2. Include the source manifest

Add this line to `src/tools/Sources.cmake`:

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/diagnostics/impedance_analysis/Sources.cmake")
```

No change to the root CMake target is needed for ordinary QML/native files.
Additional libraries need explicit build and license review before introduction.

## 3. Register the tool

Add a `ListElement` to `src/tools/ToolRegistry.qml`:

```qml
ListElement {
    toolId: "impedance-analysis"
    title: qsTr("Impedance Analysis")
    summary: qsTr("Review impedance measurements")
    category: "diagnostic"
    iconKind: "electrodogram"
    accent: "#be9bff"
    entryPoint: "diagnostics/impedance_analysis/qml/ImpedanceAnalysis.qml"
}
```

The entry path is relative to `ToolRegistry.qml`. The registry resolves it for
the launcher/host, so it must not depend on a drive letter or working directory.
Use a stable unique `toolId`; use an existing icon kind or implement a new icon
in `src/shared/qml/ToolIcon.qml`. The entry automatically participates in the
diagnostic search, launcher, dock, header title and workspace host. Do not add
tool-ID branches to Main, DesktopShell or DiagnosticWorkspace.

## Acquisition and lifecycle

Implement real functionality before registering a product-visible tool. The
host injects the shared `session`; existing modes are `live-electrodogram`,
`dsp-calibration` and `automatic-mic`. A new acquisition mode needs an explicit
extension of the service contract and its data-validation tests. Registering
a QML file alone does not add hardware support. Do not enable capture until
the service advertises the required capability.

Use explicit start/stop commands. Let the shell manage Home and full-screen
navigation. Tool instances are retained while hidden; save transient page state
inside the tool and keep acquisition ownership in the session. Do not rely on
recreating a screen each time it is selected, or treat `visible: false` as Stop.

## Validate and deliver

From the repository root:

```powershell
cmake -P cmake/VerifySources.cmake
python scripts/check_structure.py
node tests/tools.test.cjs
node tests/analysis.test.cjs
node tests/desktop.test.cjs
python scripts/package_source.py
```

Add tests for any new calculation or service behavior. Then configure/build
with the approved Qt kit and run `all_qmllint`. Verify first launch, search,
resizing, F10, Exit full screen, Desktop return, settings retained when reopening,
load errors and acquisition locks. Test existing tools as well as the new one.
For hardware support, also validate the adapter and real data on the board.

The complete source ZIP is generated under `build/transfer/`. Keep actual tool
source under `src/tools/`, not in the build folder. This example is documentation;
Impedance Analysis is not currently installed or advertised by the application.
