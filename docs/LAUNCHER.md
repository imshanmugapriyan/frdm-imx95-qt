# Launcher and tool registry

`src/shell/qml/HomeScreen.qml` is the public desktop component used by
`src/app/qml/Main.qml`. It delegates to `DesktopShell.qml`, which owns the
launcher, dock, clock, status widgets and supporting-tool windows.

`src/tools/ToolRegistry.qml` supplies metadata for both the launcher and dock.
`Main.qml` creates one registry and injects it into the shell. The launcher
filters `diagnostic` entries and searches their titles/descriptions. The dock
shows both diagnostics and utilities independently of the search query.

| Role | Purpose |
| --- | --- |
| `toolId` | Stable unique routing key |
| `title` | Tool label and diagnostic header |
| `summary` | Searchable description |
| `category` | `diagnostic` or `utility` |
| `iconKind` | Symbol implemented by `src/shared/qml/ToolIcon.qml` |
| `accent` | Icon/selection color |
| `entryPoint` | Diagnostic QML path relative to the registry; empty for utilities |

The registry resolves entry points before returning descriptors. Application
startup and the diagnostic host do not contain lists of diagnostic IDs.
`DiagnosticToolHost.qml` creates the selected diagnostic with the shared
`session` and `uiScale` properties, then retains it while hidden. Changing the
selection changes visibility, not the QML source of an already loaded tool.
Load failures show a message; reopening that tool retries loading it.

Utilities retain their dedicated `UtilityHost.qml` integration because their
note-save prompts, file access and preference signals have different contracts.
Adding a diagnostic does not require editing UtilityHost. Follow
[ADDING_DIAGNOSTIC_TOOL.md](ADDING_DIAGNOSTIC_TOOL.md) for the build and registry
steps; adding a utility also needs its UtilityHost integration.

`ToolTile.qml` in shared UI remains the unused earlier launcher card.
`ToolNotice.qml` handles a request that has no registered diagnostic entry point.
All current diagnostic entry points are packaged resources. No tool launches a
separate process. The three diagnostics are Live Electrodogram, Acoustic
Analytics Studio and LAN-XI Measurement Studio. LAN-XI opens its Setup page;
hardware inspection and acquisition require explicit actions. Its native API
and validation status are documented in [LANXI_INTERFACE.md](LANXI_INTERFACE.md).

## Validation in the approved Qt environment

Build the complete source tree, run `all_qmllint`, then follow the
[desktop checklist](DESKTOP_SHELL.md#validation). Check launcher search, all three
diagnostic routes, state retention on return, F10 expansion, Exit full screen,
Desktop return, utility windows and acquisition locks. Build references and
routing checks can run first without Qt; see the [root README](../README.md).

Linux startup/session work is tracked in [BOOT_HANDOFF.md](BOOT_HANDOFF.md).
Measurement integration is tracked in [ACQUISITION_INTERFACE.md](ACQUISITION_INTERFACE.md).
